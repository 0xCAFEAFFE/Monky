#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// TODO "windows only"
#include <conio.h>

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
  int start;  // start offset position (function content buffer)
  int end;    // end offset position
  int ret;    // input return position
} func_t;

static char s[DATA_STACK_SIZE];   // data stack, top of stack at index -1
static char v[NUM_LETTERS];       // variables, lowercase letters
static func_t f[NUM_LETTERS];     // function index array, uppercase letters
static char f_buf[FUNC_BUF_SIZE]; // function content buffer
static char a[DATA_ARRAY_SIZE];   // random access data array
static char t[TOK_BUF_SIZE];      // global token buffer

static int n;           // stack pointer, s[n] points to empty element,
static int loop_start;  // loop return jump address, -1 = disabled
static int block_depth; // nested block bracket counter
static int f_active;    // active function index, -1 = none
static int f_start;     // function start offset
static int f_end;       // function end offset
static int f_pos;       // position offset for function buffer
static bool f_def;      // true while function is being defined

// reset all static variables to initial state
void monky_reset(void)
{
  memset(&s, 0, sizeof(s));
  memset(&v, 0, sizeof(v));
  memset(&a, 0, sizeof(a));
  memset(&f, 0, sizeof(f));
    memset(&f_buf, 0, sizeof(f_buf)); // debug

  n = 0;
  loop_start = -1;
  block_depth = 0;
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
      t[l]=src[*pos+l];
      if (++l>=TOK_BUF_SIZE) { return ERROR_LITERAL_INVALID; }
    }

    *pos += l;  // advance input offset
    t[l] = 0;   // null-terminate string
    return l;   // return length
}


// execute instruction string
char monky_parse(char* input, bool *newline)
{
  // init
  char *token_src = input;  // start with user input as token source
  *newline = false;         // print newline after output
  int pos = 0;              // next input offset position
  int len;                  // current token length

  // parse next token
  while((len = getToken(token_src, &pos)))
  {
    // try converting token to literal
    char *end;
    int val = strtol(t, &end, 10);
    char sym = t[0];

    // execute primitive keywords only if we are not
    // jumping a block or defining a function
    bool skip_prim = (block_depth || f_def);

    if (!*end)
    {
      // push numeric literal to stack
      if (skip_prim) { continue; }
      if (val>127 || val<-128) { return ERROR_LITERAL_SIZE; }
      if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
      s[n++] = (char)val;
    }
    else if ((sym>='a' && sym<='z' && len==1) || (sym>='A' && sym<='Z' && len==1))
    {
      // push ASCII char to stack
      if (skip_prim) { continue; }
      if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
      s[n++] = sym;
    }
    else if (len==1)
    {
      // decode structural keywords always
      char tmp;
      switch(sym)
      {
        case '(': // block start
          block_depth++;
          break;

        case ')': // block end
          block_depth--;
          if (block_depth<0) { block_depth = 0; }
          break;

        case '[': // loop start
          if (skip_prim) { break; }
          loop_start = pos-len;
          // TODO: this is too simple if we want to have nested loops..
          break;

        case ']': // loop end
          if (skip_prim) { break; }
          if (loop_start == -1) { return ERROR_LOOP_START; }
          pos = loop_start; // jump back
          loop_start = -1;
          break;

        case '{': // function start
          if (f_def) { return ERROR_FUNCTION_NEST; }
          f_def = true;
          f_start = pos;
          break;
#warning "check if this works"
        case '}': // function end
          if (f_active == -1)
          {
            // function definition
            if (!f_def) { return ERROR_FUNCTION_NEST; }
            f_def = false;
            f_end = pos;
          }
          else
          {
            // function execution
            token_src = input;
            pos = f[f_active].ret;
            f_active = -1;
          }
          break;

        // fall-through needed to check for unknown instructions even if not executed
        // specific logic handled in separate switch statement outside
        case '+':
        case '-':
        case '*':
        case '/':
        case '_':
        case '%':
        case '$':
        case '@':
        case '^':
        case '\\':
        case '#':
        case '&':
        case '|':
        case '~':
        case '=':
        case '<':
        case '>':
        case '.':
        case ',':
        case '\'':
        case '?':
        case '!':
        case ':':
        case ';':
          break;

        default: // ignore unknown single chars
          return ERROR_UNKNOWN_CMD;

      } // end structural switch

      if (skip_prim) { continue; }

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
          if (s[n-1]>=n) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1]<0) { return ERROR_STACK_OVERFLOW; }
          s[n-1] = s[n-s[n-1]-2]; // index 1 return previous element
          break;

        case '#': // count
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

        case '.': // pop integer
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          printf("%d ", s[n-1]);
          n--;
          *newline = true;
          break;

        case ',': // pop char
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          printf("%c", s[n-1]);
          n--;
          *newline = true;
          break;

        case '\'': // read char
          if (n>=DATA_STACK_SIZE) { return ERROR_STACK_OVERFLOW; }
          // TODO: windows only for now..
          char c = _getch();
          if (c == 3) { exit(0); }      // intercept Ctrl+C
          if (c == '\r') { c = '\n'; }  // convert CR to LF
          s[n++] = c;
          break;

        case '?': // skip if true
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          #warning "will fail if input is function buffer"
          if (s[n-1] != 0) { len = getToken(input, &pos); }
          n--;
          break;

        case '!': // skip if false
          if (n<1) { return ERROR_STACK_UNDERFLOW; }
          if (s[n-1] == 0) { len = getToken(input, &pos); }
          n--;
          break;

        case ':': // store / define
        {
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
            if (n<1) { return ERROR_STACK_UNDERFLOW; }
            int i = s[n-1]-'A';
            f[i].start = f_pos;

            // copy from input to to function buffer
            for(int p=f_start+1; p<=f_end; p++)
            {
              //printf("%c",input[p]);
              f_buf[f_pos++] = input[p];
            }
            f[i].end = f_pos;
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
            if (f[i].start == f[i].end) { return ERROR_FUNCTION_UNDEF; }
            n--; // pop index from stack

            // debug out
            //for(int p=f[i].start; p<f[i].end; p++) { printf("%c", f_buf[p]); }
            #warning "redefining function does not yet work"

            // switch token source to function buffer
            f_active = i;
            #warning "claude says this wrong, double deref"
            //token_src = &f_buf[f[i].start];
            token_src = f_buf;
            f[i].ret = pos;
            pos = f[i].start;
          }
          else { return ERROR_DATA_INDEX; }
          break;
        }
      } // end primitive switch
    }
    else if (sym=='"')
    {
      // tokenize until " symbol
      char *str = (char*)input+pos-len+1;
      int l = strcspn(str, "\"");
      if (str[l]!='"') { return ERROR_STRING_END; }
      strncpy(t, str, l);
      pos = pos-len+l+2;
      t[l] = 0; // null-terminate

      if (skip_prim) { continue; }

      // push string onto stack in reverse order
      for (int i=l; i>=0; i--) { s[n++] = t[i]; }
    }
    else
    {
      printf("<<%c %s>>", sym, t);
      printf("<<%s>>", f_buf);
      #warning "why do we end up here?!"
      return ERROR_LITERAL_INVALID;
    }

  } // end while

  #warning "todo"
  //return exec_instr ? ERROR_NONE : ERROR_SILENT;
  return ERROR_NONE;
}





