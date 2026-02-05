#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// TODO:
// generalize handling of underflow? macro ?

#define TRUE -1
#define FALSE 0

#define ERROR_INPUT_EMPTY       -1
#define ERROR_STACK_UNDERFLOW   -2
#define ERROR_UNKNOWN_CMD       -3
#define ERROR_LITERAL_INVALID   -4
#define ERROR_BLOCK_END         -5
#define ERROR_LOOP_START        -6
#define ERROR_STRING_END        -7
#define ERROR_LITERAL_SIZE      -8
#define ERROR_DATA_INDEX        -9
#define ERROR_STACK_OVERFLOW    -10
#define ERROR_SUBROUTINE_UNDEF  -11

#define TOK_BUF_SIZE  32
char tok[TOK_BUF_SIZE]; // global token buffer

typedef struct
{
  int start;
  int end;
  int ret;
} function_t;

int subr = -1; // active subroutine
function_t f[26];  // function array

int getToken(const char *str, int *pos)
{
    // ignore leading spaces
    int skip = 0;
    while (str[*pos+skip] == ' ') { skip++; }
    *pos += skip;
    
    // copy to token buffer
    int i = 0;
    while (str[*pos+i] && str[*pos+i] != ' ')
    {
        // TODO: if pos > function.end -> function = 0, pos = function.ret
        if ((*pos+i > f[subr].end) && (subr != -1))
        {
          subr = -1;
          *pos = f[subr].ret;
          i = 0;
          break;
        }
        tok[i]=str[*pos+i];
        if (++i>=TOK_BUF_SIZE) { return ERROR_LITERAL_INVALID; }
    }
    *pos += i;
    
    tok[i] = 0; // null-terminate string
    return i; // return length
}

// accepts instruction string, returns top of stack
char monky(const char* input)
{
  char s[256] = {0};    // stack
  int n = -1;           // stack pointer
  char v[26] = {0};   // variables
  char a[128] = {0};  // data array

  int loop_start = -1;
  int block_start = -1;
  int block_end = -1;
  subr = -1;

  // get first token
  int pos = 0;
  int len = getToken(input, &pos);
  if (!len) { return ERROR_INPUT_EMPTY; }
  
  // parse tokens
  while (len)
  {
    // try converting token to literal
    char *end;
    int val = strtol(tok, &end, 10);
    char sym = tok[0];

    // DEBUG
    //printf("%d %s %d %c %d\n", pos, tok, len, sym, val);
    
    if (!*end)
    {
      // push numeric literal to stack
      if (val>127 || val<-128) { return ERROR_LITERAL_SIZE; }
      s[++n] = (char)val;
    }
    else if ((sym>='a' && sym<='z' && len==1) || (sym>='A' && sym<='Z' && len==1))
    {
      // push ASCII char to stack
      s[++n] = sym;
    }
    else if (len==1)
    {
      // decode keywords
      switch(sym)
      {
        case '+': // add
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] += s[n];
          n--;
          break;

        case '-': // sub
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
          
        case '%': // dup
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
          
        case '@': // rot
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
          
        case '#': // xor
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n-1] ^= s[n];
          n--;
          break;
          
        case '~': // bitwise not
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = ~s[n];
          break;
          
        case '=': // equal
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = (s[n-1] == s[n]) ? TRUE : FALSE;
          break;
          
        case '<': // less
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = (s[n] < s[n-1]) ? TRUE : FALSE;
          break;
          
        case '>': // greater
          if (n<=0) { return ERROR_STACK_UNDERFLOW; }
          s[n] = (s[n] > s[n-1]) ? TRUE : FALSE;
          break;
          
        case '.': // print integer
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          printf("%d", s[n]);
          break;
          
        case ',': // pop char
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          printf("%c", s[n--]);
          break;
          
        case '?': // skip if true
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          if (s[n] != 0) { len = getToken(input, &pos); }
          break;
          
        case '!': // skip if false
          if (n<0) { return ERROR_STACK_UNDERFLOW; }
          if (s[n] == 0) { len = getToken(input, &pos); }
          break;

        case '(': // block start
          block_start = pos;
          do
          {
            len = getToken(input, &pos);
            if (!len) { return ERROR_BLOCK_END; }
          } while (tok[0]!= ')');
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
            int i = s[n]-'A';
            f[i].start = block_start;
            f[i].end = block_end;
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
            int i = s[n]-'A';
            if (f[i].start == f[i].end) { return ERROR_SUBROUTINE_UNDEF; }
            subr = i;
            f[i].ret = pos;
            pos = f[i].start;
            n--;
          }
          else { return ERROR_DATA_INDEX; }
          break;
        }
          
        default:
        {
          // ignore unknown single chars
          return ERROR_UNKNOWN_CMD;
        }
      }
    }
    else if (sym=='"')
    {
      // tokenize until " symbol
      char *str = (char*)input+pos-len+1;
      int l = strcspn(str, "\"");
      if (str[l]!='"') { return ERROR_STRING_END; }
      strncpy(tok, str, l); 
      tok[l] = 0; // null-terminate
      
      // push string onto stack in reverse order
      for (int i=l; i>=0; i--) { s[++n] = tok[i]; }
      pos = pos-len+l+2;
    }
    else
    {
      return ERROR_LITERAL_INVALID;
    }

    // get next token
    len = getToken(input, &pos);
    
  } // end while

  //printf("%d\n",s[n]);

  // return top of stack
  return (n<0)?0:s[n];
}




