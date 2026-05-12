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
  bool newline = false;
  char line_buf[MAX_LINE_LEN] = {0};
  FILE *in = NULL;

  // determine input source
  if (argc >= 2)
  {
    // input from file
    in = fopen(argv[1], "r");
    if (in == NULL)
    {
      fprintf(stderr, "Error: Unable to open file \"%s\".\n", argv[1]);
      return EXIT_FAILURE;
    }
    from_file = true;
  }
  else
  {
    // command line input
    in = stdin;
  }

  // REPL loop
  while(true)
  {
    // print cursor
    if (!from_file)
    {
      if (newline) { printf("\n"); }
      printf("> ");
      fflush(stdout);
    }

    // read line - break on EOF or error
    if (fgets(line_buf, sizeof(line_buf), in) == NULL) {  break; }

    // remove CR/LF
    size_t len = strlen(line_buf);
    if (len > 0 && line_buf[len - 1] == '\n') { line_buf[--len] = '\0'; }
    if (len > 0 && line_buf[len - 1] == '\r') { line_buf[--len] = '\0'; }

    // skip empty lines
    if (len == 0) { continue; }

    // echo file input
    if (from_file) { printf("> %s\n", line_buf); }

    // process line
    monky(line_buf, &newline);

  } // end while

  // close file
  if (from_file) { fclose(in); }

  return EXIT_SUCCESS;
}




