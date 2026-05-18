#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "monky.h"

#define MAX_LINE_LEN 256

// main function; entry point
int main(int argc, char *argv[])
{
  bool from_file = false;
  char line_buf[MAX_LINE_LEN] = {0};
  FILE *src = NULL;
  char cursor = '>';

  // determine input source
  if (argc >= 2)
  {
    // input from file
    src = fopen(argv[1], "r");
    if (src == NULL)
    {
      fprintf(stderr, "Error: Unable to open file \"%s\".\n", argv[1]);
      return EXIT_FAILURE;
    }
    from_file = true;
  }
  else
  {
    // command line input
    src = stdin;
  }

  // init monky interpreter
  printf("MNKYNTRPRTR v0.1\n");
  monky_reset();

  // REPL loop
  while(true)
  {
    // print cursor
    if (!from_file)
    {
      printf("%c ", cursor);
      fflush(stdout);
    }

    // read line - break on EOF or error
    if (fgets(line_buf, MAX_LINE_LEN, src) == NULL) {  break; }

    // remove CR/LF
    size_t len = strlen(line_buf);
    if (len > 0 && line_buf[len - 1] == '\n') { line_buf[--len] = '\0'; }
    if (len > 0 && line_buf[len - 1] == '\r') { line_buf[--len] = '\0'; }

    // skip empty lines
    if (len == 0) { continue; }

    // echo file input
    if (from_file) { printf("%c %s\n", cursor, line_buf); }

    // process line
    bool newline;
    char error = monky_parse(line_buf, &newline);
    if (newline) { printf("\n"); }

    // error handling
    switch(error)
    {
      case ERROR_NONE:
        cursor = '>';
        break;

      case ERROR_NONE_FUNC:
        cursor = '|';
        break;

      default:
        printf("E%d\n", error);
        monky_reset();
    }

  } // end while

  // close file
  if (from_file) { fclose(src); }

  return EXIT_SUCCESS;
}




