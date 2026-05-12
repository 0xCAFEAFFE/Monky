

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


char monky(const char* input, bool *newline);
