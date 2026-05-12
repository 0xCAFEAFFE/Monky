#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "monky.h"

// internal data structures and variables

#define TOK_BUF_SIZE  64      // limit token / string length
static char t[TOK_BUF_SIZE];  // global token buffer

typedef struct
{
  int start;
  int end;
  int ret;
} func_t;

static char s[256];  // data stack
static char a[128];  // data array
static char v[NUM_LETTERS];   // variables, lowercase letters
static func_t f[NUM_LETTERS]; // function array, uppercase letters

static int n;           // stack pointer
static int func;        // active function, -1 = none
static bool execute;    // used by loops, blocks, functions to defer execution of primitive instructions

static int loop_start;
static int block_start;
static int block_end;

// reset parser to initial state
void monky_reset(void)
{
  memset(&f, 0, sizeof(f));  // reset functions
  memset(&v, 0, sizeof(v));  // reset variables
  memset(&s, 0, sizeof(s));  // reset stack
  memset(&a, 0, sizeof(a));  // reset data array

  n = -1;         // reset stack pointer
  func = -1;      // reset active function
  execute = true; // reset execute flag

  loop_start = -1;
  block_start = -1;
  block_end = -1;
}

// get next whitespace separated token out of input string, starting at offset pos
int getToken(const char *str, int *pos)
{
    // skip leading spaces
    int skips = 0;
    while (str[*pos+skips] == ' ') { skips++; }
    *pos += skips;

    // copy from input to global token buffer
    int i = 0; // token length
    while (str[*pos+i] && (str[*pos+i] != ' '))
    {
      // TODO: if pos > function.end -> function = 0, pos = function.ret
      if ((*pos+i > f[func].end) && (func != -1))
      {
        *pos = f[func].ret;
        func = -1;
        i = 0;
        break;
      }
      t[i]=str[*pos+i];
      if (++i>=TOK_BUF_SIZE) { return ERROR_LITERAL_INVALID; }
    }
    *pos += i;

    t[i] = 0; // null-terminate string
    return i; // return length
}


// execute instruction string
char monky_parse(const char* input, bool *newline)
{
  // init
  *newline = false; // print newline after output
  int len;          // token length
  int pos = 0;      // input offset position
#warning "might as well make this global too"

  // parse next token
  while((len = getToken(input, &pos)))
  {
    // try converting token to literal
    char *end;
    int val = strtol(t, &end, 10);
    char sym = t[0];

    // DEBUG
    //printf("%d %s %d %c %d\n", pos, t, len, sym, val);

    if (!*end)
    {
      if (!execute) { continue; }

      // push numeric literal to stack
      if (val>127 || val<-128) { return ERROR_LITERAL_SIZE; }
      s[++n] = (char)val;
    }
    else if ((sym>='a' && sym<='z' && len==1) || (sym>='A' && sym<='Z' && len==1))
    {
      if (!execute) { continue; }

      // push ASCII char to stack
      s[++n] = sym;
    }
    else if (len==1)
    {
      // decode structural keywords in any case
      switch(sym)
      {
        case '(': // block start
          block_start = pos;
          do
          {
            len = getToken(input, &pos);
            if (!len) { return ERROR_BLOCK_END; }
          } while (t[0]!= ')');
          block_end = pos;
          break;

        case ')': // block end
          break;

        case '[': // loop start
          loop_start = pos-len;
          break;

        case ']': // loop end
          if (loop_start == -1) { return ERROR_LOOP_START; }
          pos = loop_start;
          loop_start = -1;
          break;

        case '{': // function start
          if (execute != true) { return ERROR_FUNCTION_NEST; }
          execute = false;
          break;

        case '}': // function end
          if (execute != false) { return ERROR_FUNCTION_NEST; }
          execute = true;
          break;

        // all fallthrough, handled in second switch statement outside
        // needed to check for unknown instructions even if execute = false
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
        case '?':
        case '!':
        case ':':
        case ';':
          break;

        default: // ignore unknown single chars
          return ERROR_UNKNOWN_CMD;

      } // end structural switch

      // decode primitive keywords only if execute flag is set
      if (!execute) { continue; }

      switch(sym)
      {
        case '+': // add
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] += s[n];
          n--;
          break;

        case '-': // subtract
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] -= s[n];
          n--;
          break;

        case '*': // multiply
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] *= s[n];
          n--;
          break;

        case '/': // divide
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] /= s[n];
          n--;
          break;

        case '_': // drop
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          n--;
          break;

        case '%': // duplicate
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          if (n>=0xff) { return ERROR_STACK_OVERFLOW; }
          s[n+1] = s[n];
          n++;
          break;

        case '$': // swap
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          if (n>=0xff) { return ERROR_STACK_OVERFLOW; }
          s[n+1] = s[n];
          s[n] = s[n-1];
          s[n-1] = s[n+1];
          break;

        case '@': // rotate
          if (n<2) { return ERROR_STACK_UNDERFLOW; }
          if (n>=0xff) { return ERROR_STACK_OVERFLOW; }
          s[n+1] = s[n-2];
          s[n-2] = s[n-1];
          s[n-1] = s[n];
          s[n] = s[n+1];
          break;

        case '^': // over
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          if (n>=0xff) { return ERROR_STACK_OVERFLOW; }
          s[n+1] = s[n-1];
          n++;
          break;

        case '\\': // pick
          if (s[n]>=n) { return ERROR_STACK_UNDERFLOW; }
          if (s[n]<0) { return ERROR_STACK_OVERFLOW; }
          s[n] = s[n-s[n]-1];
          break;

        case '#': // count
          n++;
          s[n] = n;
          break;

        case '&': // and
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] &= s[n];
          n--;
          break;

        case '|': // or
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] |= s[n];
          n--;
          break;

        case '~': // bitwise not
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = ~s[n];
          break;

        case '=': // equal
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = (s[n-1] == s[n]) ? LOGIC_TRUE : LOGIC_FALSE;
          break;

        case '<': // less
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = (s[n] < s[n-1]) ? LOGIC_TRUE : LOGIC_FALSE;
          break;

        case '>': // greater
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = (s[n] > s[n-1]) ? LOGIC_TRUE : LOGIC_FALSE;
          break;

        case '.': // print integer
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          printf("%d ", s[n]);
          *newline = true;
          break;

        case ',': // pop char
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          printf("%c", s[n--]);
          *newline = true;
          break;

        case '?': // skip if true
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          if (s[n] != 0) { len = getToken(input, &pos); }
          break;

        case '!': // skip if false
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          if (s[n] == 0) { len = getToken(input, &pos); }
          break;

        case ':': // store / define
        {
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          if (s[n]>='a' && s[n]<='z')
          {
            // variable
            v[s[n]-'a'] = s[n-1];
          }
          else if (s[n]<0)
          {
            // data array: -128..-1 (0x80-0xff), ASCII: 0..127 (0x00-0x7f)
            unsigned char i = s[n];
            a[(int)(i-0x80)] = s[n-1];
          }
          else if (s[n]>='A' && s[n]<='Z')
          {
            // define function
            /*
            int i = s[n]-'A';
            f[i].start = block_start;
            f[i].end = block_end;
            */
          }
          else { return ERROR_DATA_INDEX; }
          n--;
          break;
        }

        case ';': // load / call
        {
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          if (s[n]>='a' && s[n]<='z')
          {
            // variable
            s[n] = v[s[n]-'a'];
          }
          else if (s[n]<0)
          {
            // data array
            unsigned char i = s[n];
            s[n] = a[(int)(i-0x80)];
          }
          else if (s[n]>='A' && s[n]<='Z')
          {
            // call function
            /*
            int i = s[n]-'A';
            if (f[i].start == f[i].end) { return ERROR_FUNCTION_UNDEF; }
            func = i;
            f[i].ret = pos;
            pos = f[i].start;
            n--;
            */
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

      if (!execute) { continue; }

      // push string onto stack in reverse order
      for (int i=l; i>=0; i--) { s[++n] = t[i]; }
    }
    else
    {
      return ERROR_LITERAL_INVALID;
    }

  } // end while

  return ERROR_NONE;
}





