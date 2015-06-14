/*
 * sbfi - simple brainfuck interpreter
 * 
 * version: v0.3
 * file: sbfi.c
 *
 * (c) 2015 Bernd Busse
 * Licensed under The MIT License
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static const int STACK_SIZE = 65536;
static const unsigned int BUF_SIZE = 4096;
static int debug = 0;

static unsigned char* stack_start = NULL;
static unsigned char* stack_end = NULL;
static unsigned char* ptr = NULL;

void version() {
    printf("sbfi - simple brainfuck interpreter\n");
    printf("(c) 2015 Bernd Busse v0.3\n");
}

void printerr(const char* prefix, const char* msg, va_list ap) {
    fprintf(stderr, prefix);
    fprintf(stderr, ": ");
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
}

void parse_err(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    printerr("ParsingError", msg, args);
    va_end(args);
    exit(1);
}

void opts_err(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    printerr("OptionsError", msg, args);
    va_end(args);
    exit(1);
}

void printi(const int lvl, const char* msg, ...) {
    va_list args;
    for (int i = 0; i < lvl*2; i++)
        printf(" ");
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

int getch() {
    struct termios old_tio, new_tio;
    int ch;
    
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    
    if (ch == 4)
        ch = EOF;
    else
        printf("%c", ch);

    return ch;
}

void interpret(unsigned char* in, int br_lvl) {
    int len = strlen(in);
    int inchar;
    
    for (int i = 0; i < len; i++) {
        switch (in[i]) {
            case '>':
                if (debug)
                    printi(br_lvl+1, "> Move pointer right: %d [%hhu]", ptr-stack_start+1, *(ptr+1));

                if (ptr < stack_end)
                    ptr++;
                else
                    parse_err("stack underflow!");
                continue;
            case '<':
                if (debug)
                    printi(br_lvl+1, "< Move pointer left: %d [%hhu]", ptr-stack_start-1, *(ptr-1));

                if (ptr > stack_start)
                    ptr--;
                else
                    parse_err("stack overflow!");
                continue;
            case '+':
                if (debug)
                    printi(br_lvl+1, "+ increment pos: %d [%hhu]", ptr-stack_start, (*ptr)+1);

                (*ptr)++;
                continue;
            case '-':
                if (debug)
                    printi(br_lvl+1, "- decrement pos: %d [%hhu]", ptr-stack_start, (*ptr)-1);

                (*ptr)--;
                continue;
            case '.':
                if (debug)
                    printi(br_lvl+1, ". output value of: %d [%hhu] => ", ptr-stack_start, *ptr);

                putchar(*ptr);
                fflush(stdout);
                
                if (debug)
                    printf("\n");

                continue;
            case ',':
                if ((inchar = getch()) != EOF) {
                    if (debug)
                        printi(br_lvl+1, ", read input in: %d [%hhu]", ptr-stack_start, inchar);

                    *ptr = inchar;
                } else {
                    if (debug)
                        printi(br_lvl+1, ", read EOF in: %d [%hhu]", ptr-stack_start, *ptr);
                }
                fflush(stdout);
                continue;
            case '[':
                br_lvl++;
                while (*ptr != 0) {
                    if (debug)
                        printi(br_lvl, "[ while item %d not '0' [%hhu]:", ptr-stack_start, *ptr);

                    interpret(in+i+1, br_lvl);
                }
                br_lvl--;

                if (debug)
                    printi(br_lvl+1, "[ item %d is '0' [%hhu]", ptr-stack_start, *ptr);

                int lvl = 0;
                for (int j = i; j < len; j++) {
                    switch (in[j]) {
                        case '[':
                            lvl++;
                            break;
                        case ']':
                            lvl--;
                            break;
                        default:
                            break;
                    }
                    
                    if (lvl == 0) {
                        i = j;
                        break;
                    }
                }

                if (lvl != 0) {
                    parse_err("can't find closing brace!");
                }
                
                continue;
            case ']':
                if (br_lvl == 0)
                    parse_err("found unmatched brace!");
                return;
            default:
                continue;
        }
    }
}

int main(int argc, char** argv) {
    unsigned int stack_size = STACK_SIZE;
    char filename[128] = {0};
    char line_buf[BUF_SIZE];
    for (int i = 0; i < BUF_SIZE; i++)
        line_buf[i] = 0;

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--size") == 0) {
                if (argc > i+1) {
                    stack_size = atoi(argv[++i]);
                    if (stack_size == 0)
                        opts_err("Invalid size of 0!");
                } else {
                    opts_err("Missing argument 'size' for option '%s'", argv[i]);
                }
                continue;
            } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) {
                if (argc > i+1) {
                    strncpy(filename, argv[++i], 128);
                    filename[127] = '\0';
                } else {
                    opts_err("Missing argument 'filename' for option '%s'", argv[i]);
                }
                continue;
            } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
                debug = 1;
                continue;
            } else {
                opts_err("Unkown argument: '%s'", argv[i]);
            }
        }
    }

    FILE* infile = NULL;
    if ((strlen(filename) > 0) && ((infile = fopen(filename, "r")) == NULL)) {
        fprintf(stderr, "Can't open file '%s'\n", filename);
        return 1;
    }

    if (infile == NULL)
        version();

    if (debug) {
        printf("Debug Mode!\n");
        printf("Generating stack with %u items\n", stack_size);
    }
    
    unsigned char* stack = calloc(stack_size, sizeof(char));
    if (stack == NULL) {
        fprintf(stderr, "Error occured on allocating stack. Exiting...\n");
        return 1;
    }

    stack_start = stack;
    stack_end = stack+stack_size-1;
    ptr = stack;

    if (infile) {
        char command[STACK_SIZE];
        command[0] = '\0';
        char last_in = 0;
        
        if (debug)
            printf("Reading file '%s'\n", filename);
        
        while (fgets(line_buf, BUF_SIZE, infile) != NULL) {
            for (int i = 0; i < BUF_SIZE; i++) {
                if (line_buf[i] == '\n')
                    line_buf[i] = '\0';
            }
            size_t size = STACK_SIZE - strlen(command);
            strncat(command, line_buf, size);
            command[STACK_SIZE-1] = '\0';
        }
        interpret(command, 0);
    } else {
        while (1) {
            printf(">>> ");
            if (fgets(line_buf, BUF_SIZE, stdin) == NULL) {
                printf("\n");
                break;
            }
            
            if (strcmp(line_buf, "exit\n") == 0) {
                printf("Exiting...\n");
                break;
            } else if (strcmp(line_buf, "clear\n") == 0) {
                printf("Clear stack!\n");
                for (int i = 0; i < stack_size; i++) {
                    stack[i] = 0;
                }
                continue;
            } else if (strcmp(line_buf, "len\n") == 0) {
                printf("Stack length: %u\n", stack_size);
                continue;
            } else if (strncmp(line_buf, "show", 4) == 0) {
                unsigned int pos;
                if ((line_buf[4] == '\n') || (sscanf(line_buf+4, "%u", &pos) <= 0))
                    pos = 0;

                if (pos >= stack_size)
                    pos = stack_size - 1;

                printf("#%u element: %3hhu [%c]\n", pos, stack[pos], stack[pos]);
                continue;
            } else if (strncmp(line_buf, "print", 5) == 0) {
                unsigned int num;
                if ((line_buf[5] == '\n') || (sscanf(line_buf+5, "%u", &num) <= 0))
                    num = 16;

                if (num > stack_size)
                    num = stack_size;

                printf("First %d entries of stack:\n", num);
                for (int i = 0; i < num; i++) {
                    if (ptr == stack+i)
                        printf("[%3hhu] ", stack[i]);
                    else
                        printf("%3hhu ", stack[i]);
                }
                printf("\n");
                continue;
            } else {
                char last_in = 0;
                
                for (int i = 0; i < BUF_SIZE; i++) {
                    if (line_buf[i] == '\n')
                        line_buf[i] = '\0';
                }

                interpret(line_buf, 0);
            }
        }
    }

    free(stack);
    ptr = stack = NULL;
    stack_start = stack_end = NULL;
    stack_size = 0;

    return 0;
}
