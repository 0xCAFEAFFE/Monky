#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
  #include <conio.h>
#else
  #include <termios.h>
  #include <unistd.h>
  static int _getch(void) {
      struct termios oldt, newt;
      tcgetattr(STDIN_FILENO, &oldt);
      newt = oldt;
      newt.c_lflag &= ~(ICANON | ECHO);
      newt.c_cc[VMIN] = 1;
      newt.c_cc[VTIME] = 0;
      tcsetattr(STDIN_FILENO, TCSANOW, &newt);
      int c = getchar();
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      return c == EOF ? 0 : c;
  }
#endif

#include "monky.h"

// constants
#define TOK_BUF_SIZE      64u   // limit token / string length
#define DATA_STACK_SIZE   256u
#define DATA_ARRAY_SIZE   128u
#define FUNC_BUF_SIZE     256u
#define NUM_LETTERS       26 // number of letters A-Z in alphabet

// internal data structures and variables
typedef struct
{
  int start;    // start offset position (function content buffer)
  int end;      // end offset position
  int ret_pos;  // input return position
  int ret_func; // previously active function
} func_t;

static char s[DATA_STACK_SIZE];   // data stack, top of stack at index -1
static char v[NUM_LETTERS];       // variables, lowercase letters
static func_t f[NUM_LETTERS];     // function index array, uppercase letters
static char f_buf[FUNC_BUF_SIZE]; // function content buffer
static char a[DATA_ARRAY_SIZE];   // random access data array
static char t[TOK_BUF_SIZE];      // global token buffer

static int n;           // stack pointer, s[n] points to empty element,
static int f_active;    // active function index, -1 = none
static int f_start;     // function start offset
static int f_end;       // function end offset
static int f_pos;       // position offset for function buffer
static bool f_def;      // true while function is being defined

// reset all static variables to initial state, called at init and on error
void monky_reset(void)
{
  memset(&s, 0, sizeof(s));
  memset(&v, 0, sizeof(v));
  memset(&a, 0, sizeof(a));
  memset(&f, 0, sizeof(f));
  memset(&f_buf, 0, sizeof(f_buf));

  n = 0;
  f_active = -1;
  f_start = 0;
  f_end = 0;
  f_pos = 0;
  f_def = false;
}


// get next whitespace separated token out of input string, starting at offset pos
int getToken(char *src, int *pos)
{
    // skip leading spaces
    int skips = 0;
    while (src[*pos+skips] == ' ') { skips++; }
    *pos += skips;

    // copy from input to global token buffer
    int l = 0; // token length
    while (src[*pos+l] && (src[*pos+l] != ' '))
    {
      t[l] = src[*pos+l];
      if (++l>=TOK_BUF_SIZE) { return ERROR_LITERAL_INVALID; }
    }

    *pos += l;  // advance input offset
    t[l] = 0;   // null-terminate string
    return l;   // return length
}


// execute instruction string
char monky_parse(char* ui_buf, bool *newline)
{
  // init
  char *tok_src = ui_buf;   // start with user input as token source
  *newline = false;         // print newline after output
  int pos = 0;              // next input offset position
  int len;                  // current token length

  // parse next token
  while ((len = getToken(tok_src, &pos)))
  {
    // try converting token to literal
    char *end;
    int val = strtol(t, &end, 10);
    char sym = t[0];

    // special handling of function keywords before primitive instructions
    if ((sym == '{') && (len==1))
    {
      // function start
      if (f_def) { return ERROR_FUNC_DEF; }
      f_def = true;
      f_start = f_pos;
      continue;
    }

    // check if we're defining a function
    if (f_def)
    {
      // copy tokens to function buffer
      for (int p=pos-len; p<pos+1; p++)
      {
        // replace null byte with space for multi line functions
        f_buf[f_pos++] = ui_buf[p] ? ui_buf[p] : ' ';
        if (f_pos>=FUNC_BUF_SIZE) { return ERROR_FUNC_BUFFER; }

        // debug
        //printf("%c", ui_buf[p]);
      }
    }

    if ((sym == '}') && (len==1))
    {
      // function end
      if (f_active == -1)
      {
        // end of function definition
        if (!f_def) { return ERROR_FUNC_DEF; }
        f_def = false;
        f_end = f_pos;

        // debug
        //for (int p=0; p<f_pos; p++) { printf("%c", f_buf[p]); }
      }
      else
      {
        // end of function execution
        pos = f[f_active].ret_pos;
        f_active = f[f_active].ret_func;
        if (f_active == -1) { tok_src = ui_buf; }
      }
      continue;
    }

    // skip primitive token execution
    if (f_def) { continue; }

    if (!*end)
    {
      // push numeric literal to stack
      if (val>127 || val<-128) { return ERROR_LITERAL_INVALID; }
      if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
      s[n++] = (char)val;
    }
    else if ((sym>='a' && sym<='z' && len==1) || (sym>='A' && sym<='Z' && len==1))
    {
      // push ASCII char to stack
      if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
      s[n++] = sym;
    }
    else if (len==1) // primitive instructions
    {
      char tmp;
      switch(sym)
      {
        case '+': // add
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-2] += s[n-1];
          n--;
          break;

        case '-': // subtract
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-2] -= s[n-1];
          n--;
          break;

        case '*': // multiply
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-2] *= s[n-1];
          n--;
          break;

        case '/': // divide
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-2] /= s[n-1];
          n--;
          break;

        case '_': // drop
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          n--;
          break;

        case '%': // duplicate
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
          s[n] = s[n-1];
          n++;
          break;

        case '$': // swap
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          tmp = s[n-1];
          s[n-1] = s[n-2];
          s[n-2] = tmp;
          break;

        case '@': // rotate
          if (n<3) { return ERROR_STACK_UNDERFLOW; }
          tmp = s[n-3];
          s[n-3] = s[n-2];
          s[n-2] = s[n-1];
          s[n-1] = tmp;
          break;

        case '^': // over
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
          s[n] = s[n-2];
          n++;
          break;

        case '\\': // pick
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1]>=n-1) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1]<0) { return ERROR_STACK_OVERFLOW; }
          s[n-1] = s[n-s[n-1]-2]; // index 0 returns item before index
          break;

        case '#': // count
          if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
          s[n] = n;
          n++;
          break;

        case '&': // and
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-2] &= s[n-1];
          n--;
          break;

        case '|': // or
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-2] |= s[n-1];
          n--;
          break;

        case '~': // not
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] = ~s[n-1];
          break;

        case '`': // xor
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-2] ^= s[n-1];
          n--;
          break;

        case '=': // equal
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] = (s[n-2] == s[n-1]) ? LOGIC_TRUE : LOGIC_FALSE;
          break;

        case '<': // less
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] = (s[n-2] < s[n-1]) ? LOGIC_TRUE : LOGIC_FALSE;
          break;

        case '>': // greater
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] = (s[n-2] > s[n-1]) ? LOGIC_TRUE : LOGIC_FALSE;
          break;

        case '.': // print integer
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          printf("%d ", s[n-1]);
          *newline = true;
          break;

        case ',': // pop char
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          printf("%c", s[n-1]);
          n--;
          *newline = true;
          break;

        case '\'': // read char
          if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
          char c = _getch();
          if (c == 3) { exit(0); }      // intercept Ctrl+C
          if (c == '\r') { c = '\n'; }  // convert CR to LF
          s[n++] = c;
          break;

        case '?': // skip if true
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1] != 0) { len = getToken(tok_src, &pos); }
          n--;
          break;

        case '!': // skip if false
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1] == 0) { len = getToken(tok_src, &pos); }
          n--;
          break;

        case '(': // block start
        {
          // skip forward to matching block end
          int depth = 1;
          while (depth > 0)
          {
            len = getToken(tok_src, &pos);
            if (!len) { return ERROR_BLOCK_END; }
            if ((f_active != -1) && (pos > f[f_active].end)) { return ERROR_BLOCK_END; }
            if (t[0] == '(') depth++;
            if (t[0] == ')') depth--;
          }
          break;
        }

        case ')': // block end
          // nothing to do, block start jumps to this
          break;

        case '[': // loop start
          // nothing to do, loop end scans back to this
          break;

        case ']': // loop end
        {
          int depth = 1;
          int p = pos-len-1; // start just before token
          int bound = (f_active == -1) ? 0 : f[f_active].start;
          while ((p>=bound) && (depth>0))
          {
            if (tok_src[p] == ']') depth++;
            if (tok_src[p] == '[') depth--;
            if (depth > 0) p--;
          }
          if (depth != 0) { return ERROR_LOOP_START; }
          pos = p; // jump back to matching loop start
          break;
        }

        case ':': // store / define
        {
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1]>='a' && s[n-1]<='z')
          {
            // variable
            if (n<2) { return ERROR_STACK_UNDERFLOW; }
            v[s[n-1]-'a'] = s[n-2];
          }
          else if (s[n-1]<0)
          {
            // data array: -128..-1 (0x80-0xff), ASCII: 0..127 (0x00-0x7f)
            if (n<2) { return ERROR_STACK_UNDERFLOW; }
            unsigned char i = s[n-1];
            a[(int)(i-0x80)] = s[n-2];
          }
          else if (s[n-1]>='A' && s[n-1]<='Z')
          {
            // define function
            int i = s[n-1]-'A';
            f[i].start = f_start;
            f[i].end = f_end;

            // debug
            //for (int p=f_start; p<f_end; p++) { printf("%d ", f_buf[p]); }
          }
          else { return ERROR_DATA_INDEX; }
          n--;
          break;
        }

        case ';': // load / call
        {
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1]>='a' && s[n-1]<='z')
          {
            // variable
            s[n-1] = v[s[n-1]-'a'];
          }
          else if (s[n-1]<0)
          {
            // data array
            unsigned char i = s[n-1];
            s[n-1] = a[(int)(i-0x80)];
          }
          else if (s[n-1]>='A' && s[n-1]<='Z')
          {
            // get function index
            int i = s[n-1]-'A';
            if (f[i].start == f[i].end) { return ERROR_FUNC_UNDEF; }
            n--; // pop index from stack

            // debug
            //for (int p=f[i].start; p<f[i].end; p++) { printf("%c", f_buf[p]); }

            // switch token source to function buffer
            f[i].ret_func = f_active;
            f_active = i;
            tok_src = f_buf;
            f[i].ret_pos = pos;
            pos = f[i].start;
          }
          else { return ERROR_DATA_INDEX; }
          break;
        }

        default:
          return ERROR_UNKNOWN_CMD;

      } // end switch
    }
    else if (sym=='"')
    {
      // tokenize until " symbol
      char *str = (char*)tok_src+pos-len+1;
      int l = strcspn(str, "\"");
      if (str[l]!='"') { return ERROR_STRING_END; }
      strncpy(t, str, l);
      pos = pos-len+l+2;
      t[l] = 0; // null-terminate

      // push string onto stack in reverse order
      for (int i=l; i>=0; i--)
      {
        if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
        s[n++] = t[i];
      }
    }
    else
    {
      return ERROR_LITERAL_INVALID;
    }

  } // end while

  return f_def ? ERROR_NONE_FUNC : ERROR_NONE;
}



