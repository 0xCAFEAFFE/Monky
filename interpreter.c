#include <string.h>
#include <stdlib.h>

// DEBUG
#include <stdio.h>

char monky(const char* input)
{
  char s[256];  // data stack
  int n = -1;   // data stack pointer

  // copy input string to modifiable buffer
  char expr[256];
  strcpy(expr, input);

  // get first token
  char *tok = strtok(expr, " ");
  if (!tok) { return 0; } // ignore empty input

  // parse tokens
  while (tok)
  {
    // try converting token to literal
    char *end;
    char val = strtol(tok, &end, 10);
    int len = strlen(tok);
    char sym = tok[0];
    
    //printf("%s %d %d %d\n",tok,len,sym,val);
    
    if (!*end)
    {
      // push numeric literal to stack
      s[++n] = val;
    }
    else if (sym>='a' && sym<='z' && len==1)
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
          s[n-1] += s[n];
          n--;
        break;

        case '-': // sub
          s[n-1] -= s[n];
          n--;
        break;

        case '*': // multiply
          s[n-1] *= s[n];
          n--;
        break;

        case '/': // divide
          s[n-1] /= s[n];
          n--;
        break;
          
        case '_': // drop
          n--;
        break;
          
        case '%': // dup
          s[n+1] = s[n];
          n++;
        break;
          
        case '$': // swap
          s[n+1] = s[n];
          s[n] = s[n-1];
          s[n-1] = s[n+1];
        break;
          
        case '@': // rot
          s[n+1] = s[n-2];
          s[n-2] = s[n-1];
          s[n-1] = s[n];
          s[n] = s[n+1];
        break;

        case '^': // over
          s[n+1] = s[n-1];
          n++;
        break;
          
        case '&': // and
          s[n-1] &= s[n];
          n--;
        break;
          
        case '|': // or
          s[n-1] |= s[n];
          n--;
        break;
          
        case '#': // xor
          s[n-1] ^= s[n];
          n--;
        break;
          
        case '~': // bitwise not
          s[n] = ~s[n];
        break;
          
        case '!': // logical not
          s[n] = !s[n];
        break;
          
        case '=': // equal
          s[n-1] = (s[n-1] == s[n]);
          n--;
        break;
          
        case '<': // less
          s[n-1] = (s[n] < s[n-1]);
          n--;
        break;
          
        case '>': // greater
          s[n-1] = (s[n] > s[n-1]);
          n--;
        break;
          
        case '.': // pop integer
          printf("%d", (n<0)?0:s[n]);
          n -= (n<0) ? 0 : 1; // avoid underflow
        break;
          
        case ',': // pop char
          printf("%c", (n<0)?0:s[n]);
          n -= (n<0) ? 0 : 1; // avoid underflow
        break;
          
        default:
        {
          // ignore unknown single char keywords
          return 0;
        }
      }
    }
    else
    {
      //printf("WTF! %s %d %d %d\n",tok,len,sym,val);
      
      // discard mixed input
      return 0;
    }
    
    // get next token
    tok = strtok(NULL, " ");
  }
  
  //printf("%d\n",s[n]);
  return (n<0)?0:s[n];
}