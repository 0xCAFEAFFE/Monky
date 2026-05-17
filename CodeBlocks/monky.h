
#define LOGIC_TRUE -1
#define LOGIC_FALSE 0

#define ERROR_NONE              0
#define ERROR_STACK_UNDERFLOW   -1
#define ERROR_UNKNOWN_CMD       -2
#define ERROR_LITERAL_INVALID   -3
#define ERROR_BLOCK_END         -4
#define ERROR_LOOP_START        -5
#define ERROR_STRING_END        -6
#define ERROR_LITERAL_SIZE      -7
#define ERROR_DATA_INDEX        -8
#define ERROR_STACK_OVERFLOW    -9
#define ERROR_FUNC_UNDEF        -10
#define ERROR_FUNC_NESTING      -11

#define ERROR_SILENT            -99

char monky_parse(char* input, bool *newline);
void monky_reset(void);
