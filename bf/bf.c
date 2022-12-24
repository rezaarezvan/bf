#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#ifdef NO_CHECK_BOUNDS
#define CHECK_BOUNDS ;
#define BOUNDS_ARGS ""

#else

bool check = false;
bool circular = false;
bool infinite = false;
unsigned char* tapeEnd;

#define CHECK_BOUNDS \
    if (check) {\
        if (tapePtr < tape) {\
            if (!circular) {\
                fprintf(stderr, "err: tape pointer out of bounds\n");\
                return NULL;\
            }\
            while (tapePtr < tape) tapePtr += tapeLength;\
        } else if (tapePtr >= tapeEnd) {\
            if (!circular && !infinite) {\
                fprintf(stderr, "err: tape pointer out of bounds\n");\
                return NULL;\
            } else if (circular) {\
                while (tapePtr >= tapeEnd) tapePtr -= tapeLength;\
            } else {\
                size_t position = tapePtr - tape;\
                while (position >= tapeLength) tapeLength *= 2;\
                tape = realloc(tape, tapeLength * sizeof(unsigned char));\
                if (!tape) {\
                    fprintf(stderr, "err: failed to allocate tape\n");\
                    return NULL;\
                }\
                while (tapeEnd < tape + tapeLength) {\
                    *(tapeEnd++) = 0;\
                }\
                tapePtr = tape + position;\
            }\
        }\
    }
#define BOUNDS_ARGS "bcf"

#endif

typedef enum {
    EOF_UNCHANGED,
    EOF_VALUE
} eof_mode;

unsigned char* tape;
unsigned char* tapePtr;
size_t tapeLength = 30000;
eof_mode onEof = EOF_UNCHANGED;
unsigned char eofValue = 0;

typedef struct Code Code;

// function that returns ptr to next Code struct to run
typedef Code* (*bf_func)(Code* self);

struct Code {
    bf_func func;
    Code *next;
    Code *branch;
    int shift;
    unsigned char inc;
};

// instructions
Code* bf_plus(Code* self) {
    (*tapePtr)++;
    return self->next;
}

Code* bf_minus(Code* self) {
    (*tapePtr)--;
    return self->next;
}

Code* bf_left(Code* self) {
    tapePtr--;
    CHECK_BOUNDS
    return self->next;
}

Code* bf_right(Code* self) {
    tapePtr++;
    CHECK_BOUNDS
    return self->next;
}

Code* bf_lb(Code* self) {
    if (*tapePtr) return self->next;
    else return self->branch;
}

Code* bf_rb(Code* self) {
    if (*tapePtr) return self->branch;
    else return self->next;
}

Code* bf_rb_nop(Code* self) {
    return self->next;
}

Code* bf_getc_unc(Code* self) {
    int c = getchar();
    if (c != EOF) *tapePtr = (unsigned char) c;
    return self->next;
}

Code* bf_getc_val(Code* self) {
    int c = getchar();
    if (c == EOF) *tapePtr = eofValue;
    else *tapePtr = (unsigned char) c;
    return self->next;
}

Code* bf_putc(Code* self) {
    putchar(*tapePtr);
    return self->next;
}

Code* bf_zero(Code* self) {
    *tapePtr = 0;
    return self->next;
}

Code* bf_inc(Code* self) {
    *tapePtr += self->inc;
    return self->next;
}

Code* bf_shift(Code* self) {
    tapePtr += self->shift;
    CHECK_BOUNDS
    return self->next;
}

Code* bf_inc_shift(Code* self) {
    *tapePtr += self->inc;
    tapePtr += self->shift;
    CHECK_BOUNDS
    return self->next;
}

Code* bf_inc_shift_lb(Code* self) {
    *tapePtr += self->inc;
    tapePtr += self->shift;
    CHECK_BOUNDS
    if (*tapePtr) return self->next;
    else return self->branch;
}

Code* bf_inc_shift_rb(Code* self) {
    *tapePtr += self->inc;
    tapePtr += self->shift;
    CHECK_BOUNDS
    if (*tapePtr) return self->branch;
    else return self->next;
}

Code* bf_add(Code* self) {
    unsigned char val = *tapePtr;
    *tapePtr = 0;
    tapePtr += self->shift;
    CHECK_BOUNDS
    *tapePtr += val * self->inc;
    tapePtr -= self->shift;
    return self->next;
}

Code* bf_nop(Code *self) {
    return self->next;
}

Code* bf_end(Code* self) {
    return NULL;
}

// at least one of inc, shift should be nonzero
Code* emit_code(Code* c, unsigned char inc, int shift) {
    c->inc = inc;
    c->shift = shift;
    if (!inc) {
        if (shift == 1) {
            c->func = bf_right;
        } else if (shift == -1) {
            c->func = bf_left;
        } else {
            c->func = bf_shift;
        }
    } else if (!shift) {
        if (inc == 1) {
            c->func = bf_plus;
        } else if (inc == (unsigned char) -1) {
            c->func = bf_minus;
        } else {
            c->func = bf_inc;
        }
    } else {
        c->func = bf_inc_shift;
    }
    return c;
}

void print_repeat(FILE* f, int num, char up, char down) {
    unsigned char ch = num < 0 ? down : up;
    signed char dir = num < 0 ? 1 : -1;
    while (num != 0) {
        putc(ch, f);
        num += dir;
    }
}

// prints generated code
// set bf to true to print valid bf, or false to print bytecode
void print_code(FILE* f, Code* opt, size_t len, bool bf) {
    size_t i;
    for (i = 0; i < len; i++) {
        Code* c = opt + i;
        if (c->func == bf_end) {
            break;
        } else if (c->func == bf_lb) {
            fputc('[', f);
        } else if (c->func == bf_rb || c->func == bf_rb_nop) {
            fputc(']', f);
        } else if (c->func == bf_putc) {
            fputc('.', f);
        } else if (c->func == bf_getc_unc || c->func == bf_getc_val) {
            fputc(',', f);
        } else if (c->func == bf_left) {
            fputc('<', f);
        } else if (c->func == bf_right) {
            fputc('>', f);
        } else if (c->func == bf_plus) {
            fputc('+', f);
        } else if (c->func == bf_minus) {
            fputc('-', f);
        } else if (c->func == bf_nop) {
            // nop
        } else if (c->func == bf_zero) {
            if (bf) fprintf(f, "[-]");
            else putc('z', f);
        } else if (c->func == bf_add) {
            int inc = (signed char) c->inc;
            if (bf) {
                fprintf(f, "[-");
                print_repeat(f, c->shift, '>', '<');
                print_repeat(f, inc, '+', '-');
                print_repeat(f, -c->shift, '>', '<');
                putc(']', f);
            } else {
                fprintf(f, "a(%d,%d)", inc, c->shift);
            }
        } else if (c->func == bf_inc) {
            int inc = (signed char) c->inc;
            if (bf) {
                print_repeat(f, inc, '+', '-');
            } else fprintf(f, "i(%d)", inc);
        } else if (c->func == bf_shift) {
            if (bf) {
                print_repeat(f, c->shift, '>', '<');
            } else fprintf(f, "s(%d)", c->shift);
        } else if (c->func == bf_inc_shift) {
            int inc = (signed char) c->inc;
            if (bf) {
                print_repeat(f, inc, '+', '-');
                print_repeat(f, c->shift, '>', '<');
            } else fprintf(f, "c(%d,%d)", inc, c->shift);
        } else if (c->func == bf_inc_shift_lb) {
            int inc = (signed char) c->inc;
            if (bf) {
                print_repeat(f, inc, '+', '-');
                print_repeat(f, c->shift, '>', '<');
                fputc('[', f);
            } else fprintf(f, "c(%d,%d)[", inc, c->shift);
        } else if (c->func == bf_inc_shift_rb) {
            int inc = (signed char) c->inc;
            if (bf) {
                print_repeat(f, inc, '+', '-');
                print_repeat(f, c->shift, '>', '<');
                fputc(']', f);
            } else fprintf(f, "c(%d,%d)]", inc, c->shift);
        } else {
            fputc('?', f);
        }
    }
    fputc('\n', f);
}

void print_usage() {
    fprintf(stderr, "usage: bf [-e eof_value] [-t tape_size] [-" BOUNDS_ARGS "hpv] file\n"
                    "    -e eof_value: integer value of EOF (omit option to leave cell unchanged)\n"
                    "    -t tape_size: length of tape (default: 30000)\n"
#ifndef NO_CHECK_BOUNDS
                    "    -b: enable bounds checking\n"
                    "    -c: enable circular tape (implies -b)\n"
                    "    -f: enable infinite tape (implies -b)\n"
#endif
                    "    -h: print this help\n"
                    "    -p: print minified code to stdout instead of running it\n"
                    "    -v: show verbose messages\n");
}

int main(int argc, char* argv[]) {
    // deal with args
    if (argc < 2) {
        print_usage();
        return 1;
    }
    int c;
    unsigned char ch;
    bool onlyPrint = false, verbose = false;
    opterr = 0;
    while ((c = getopt(argc, argv, ":e:t:" BOUNDS_ARGS "hpv")) != -1) {
        ch = (unsigned char) c;
        if (c == 'e') {
            onEof = EOF_VALUE;
            eofValue = (unsigned char) atoi(optarg);
        } else if (c == 't') {
            int len = atoi(optarg);
            if (len <= 0) {
                fprintf(stderr, "err: tape size must be positive integer\n");
                return 1;
            }
            tapeLength = (size_t) len;
#ifndef NO_CHECK_BOUNDS
        } else if (c == 'b') {
            check = true;
        } else if (c == 'c') {
            check = circular = true;
            if (infinite) {
                fprintf(stderr, "err: options 'c', 'f' mutually exclusive\n");
                return 1;
            }
        } else if (c == 'f') {
            check = infinite = true;
            if (circular) {
                fprintf(stderr, "err: options 'c', 'f' mutually exclusive\n");
                return 1;
            }
#endif
        } else if (c == 'h') {
            print_usage();
            return 0;
        } else if (c == 'p') {
            onlyPrint = true;
        } else if (c == 'v') {
            verbose = true;
        } else if (c == '?') {
            fprintf(stderr, "err: unknown option '%c'\n", optopt);
            return 1;
        } else if (c == ':') {
            fprintf(stderr, "err: option '%c' requires argument\n", optopt);
            return 1;
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "err: no file provided\n");
        return 1;
    }
    // read entire file
    FILE* f = fopen(argv[optind], "r");
    if (!f) {
        fprintf(stderr, "err: unable to open file '%s'\n", argv[1]);
        return 1;
    }
    size_t i, length = 0, maxLength = 1024, depth = 0, maxDepth = 1, line = 1, col = 0;
    unsigned char* code = malloc(maxLength * sizeof(char));
    if (!code) {
        fprintf(stderr, "err: failed to allocate file read buffer\n");
        fclose(f);
        return 1;
    }
    while ((c = fgetc(f)) != EOF) {
        ch = (unsigned char) c;
        if (ch == '\n') {
            line++;
            col = 0;
        }
        col++;
        if (ch != '+' && ch != '-' && ch != '<' && ch != '>' && ch != '[' && ch != ']' && ch != '.' && ch != ',')
            continue;
        if (ch == '[') {
            depth++;
            if (depth > maxDepth) {
                // overestimate of loop depth b/c later optimizations, should be good enough
                maxDepth = depth;
            }
        } else if (ch == ']') {
            if (depth == 0) {
                fprintf(stderr, "err: closing bracket with no opening bracket at line %zu col %zu\n", line, col);
                fclose(f);
                free(code);
                return 1;
            }
            depth--;
        }
        length++;
        if (length > maxLength) {
            maxLength *= 2;
            unsigned char* newCode = realloc(code, maxLength * sizeof(char));
            if (!newCode) {
                fprintf(stderr, "err: failed to allocate file read buffer\n");
                fclose(f);
                free(code);
                return 1;
            }
            code = newCode;
        }
        code[length - 1] = ch;
    }
    fclose(f);
    if (depth != 0) {
        fprintf(stderr, "err: opening bracket with no closing bracket (need %zu at end)\n", depth);
        free(code);
        return 1;
    }
    if (verbose) fprintf(stderr, "read %zu bf instructions\n", length);
    // convert bf into optimized bytecode-like structure
    size_t instrCt;
    // true only if cell currently pointed to during execution must be zero (program start, end of loop)
    bool definiteZero = true;
    unsigned char deltaInc;
    int deltaShift;
    i = instrCt = depth = deltaInc = deltaShift = 0;
    size_t *stk = malloc(maxDepth * sizeof(size_t));
    if (!stk) {
        fprintf(stderr, "err: failed to allocate stack\n");
        free(code);
        return 1;
    }
    // quite a bit more memory that we will probably end up using...
    // can't easily realloc later without moving ptrs around
    Code* bytecode = calloc(length + 1, sizeof(Code));
    if (!bytecode) {
        fprintf(stderr, "err: failed to allocate bytecode\n");
        free(code);
        free(stk);
        return 1;
    }
    bf_func bf_getc = onEof == EOF_UNCHANGED ? bf_getc_unc : bf_getc_val;
    while (i < length) {
        ch = code[i];
        // optimize simple loops that set cell to 0 ([-], [+])
        if (!definiteZero && i < length - 2 && ch == '[' && (code[i + 1] == '+' || code[i + 1] == '-') && code[i + 2] == ']') {
            // definiteZero == true case handled below along with other loops
            if (deltaShift) {
                emit_code(&bytecode[instrCt], deltaInc, deltaShift);
                deltaInc = deltaShift = 0;
                instrCt++;
            }
            deltaInc = 0;
            bytecode[instrCt].func = bf_zero;
            i += 2;
            instrCt++;
            definiteZero = true;
        } else if (ch == '<') {
            deltaShift--;
            definiteZero = false;
        } else if (ch == '>') {
            deltaShift++;
            definiteZero = false;
        } else if (ch == '-') {
            if (deltaShift) {
                emit_code(&bytecode[instrCt], deltaInc, deltaShift);
                deltaInc = deltaShift = 0;
                instrCt++;
            }
            deltaInc--;
            definiteZero = false;
        } else if (ch == '+') {
            if (deltaShift) {
                emit_code(&bytecode[instrCt], deltaInc, deltaShift);
                deltaInc = deltaShift = 0;
                instrCt++;
            }
            deltaInc++;
            definiteZero = false;
        } else if (ch == '[') {
            bool emitted = false;
            if (deltaInc || deltaShift) {
                emit_code(&bytecode[instrCt], deltaInc, deltaShift);
                deltaInc = deltaShift = 0;
                //instrCt++;    // don't increment instrction count yet
                definiteZero = false;
                emitted = true;
            }
            // prune dead code
            if (definiteZero) {
                size_t currentDepth = depth;
                depth++;
                while (depth > currentDepth) {
                    i++;
                    if (code[i] == '[') depth++;
                    else if (code[i] == ']') depth--;
                    // i now pts to closing bracket, but incremented again at bottom of loop
                }
            } else {
                bytecode[instrCt].func = emitted ? bf_inc_shift_lb : bf_lb;
                stk[depth++] = instrCt;
                // now increment ct here
                instrCt++;
            }
        } else if (ch == ']') {
            bool emitted = false;
            if (deltaInc || deltaShift) {
                emit_code(&bytecode[instrCt], deltaInc, deltaShift);
                deltaInc = deltaShift = 0;
                //instrCt++;    // similar to above
                definiteZero = false;
                emitted = true;
            }
            bytecode[instrCt].func = emitted ? bf_inc_shift_rb : definiteZero ? bf_rb_nop : bf_rb;
            size_t open = stk[--depth];
            // point at each other for now, will get changed later
            bytecode[open].branch = &bytecode[instrCt];
            bytecode[instrCt].branch = &bytecode[open];
            instrCt++;
            definiteZero = true;
        } else if (ch == '.') {
            if (deltaInc || deltaShift) {
                emit_code(&bytecode[instrCt], deltaInc, deltaShift);
                deltaInc = deltaShift = 0;
                instrCt++;
                definiteZero = false;
            }
            bytecode[instrCt].func = bf_putc;
            instrCt++;
        } else if (ch == ',') {
            if (deltaInc || deltaShift) {
                emit_code(&bytecode[instrCt], deltaInc, deltaShift);
                deltaInc = deltaShift = 0;
                instrCt++;
            }
            deltaInc = 0;
            bytecode[instrCt].func = bf_getc;
            instrCt++;
            definiteZero = false;
        }
        i++;
    }
    if (deltaInc || deltaShift) {
        emit_code(&bytecode[instrCt], deltaInc, deltaShift);
        deltaInc = deltaShift = 0;
        instrCt++;
    }
    free(code);
    free(stk);
    if (onlyPrint) {
        print_code(stdout, bytecode, instrCt, true);
        free(bytecode);
        return 0;
    }
    // set next ptrs
    size_t realCt = 0;
    for (i = 0; i < instrCt; i++) {
        // addition loop: left bracket + inc_shift + inc_shift_rb, where the shifts are opposite
        if (i < instrCt - 2) {
            if ((bytecode[i].func == bf_lb || bytecode[i].func == bf_inc_shift_lb)
                && bytecode[i + 1].func == bf_inc_shift && bytecode[i + 2].func == bf_inc_shift_rb) {
                if (bytecode[i + 1].inc == (unsigned char) -1 && bytecode[i + 1].shift == -bytecode[i + 2].shift) {
                    bytecode[i + 1].func = bf_nop;
                    bytecode[i + 2].func = bf_nop;
                    size_t change;
                    if (bytecode[i].func == bf_inc_shift_lb) {
                        bytecode[i].func = bf_inc_shift;
                        change = i + 1;
                    } else {
                        change = i;
                    }
                    bytecode[change].func = bf_add;
                    bytecode[change].inc = bytecode[i + 2].inc;
                    bytecode[change].shift = bytecode[i + 1].shift;
                }
            }
        }
        // micro-opt: skip nops
        if (bytecode[i].func == bf_rb_nop) {
            (*(bytecode[i].branch - 1)).branch = &bytecode[realCt];
        } else if (bytecode[i].func != bf_nop) {
            if (i != realCt) {
                memcpy(&bytecode[realCt], &bytecode[i], sizeof(Code));
            }
            if (bytecode[realCt].func == bf_lb || bytecode[realCt].func == bf_inc_shift_lb) {
                bytecode[realCt].branch->branch = &bytecode[realCt + 1];
            } else if (bytecode[realCt].func == bf_rb || bytecode[realCt].func == bf_inc_shift_rb) {
                (*(bytecode[realCt].branch - 1)).branch = &bytecode[realCt + 1];
            }
            bytecode[realCt].next = &bytecode[realCt + 1];
            realCt++;
        }
    }
    // emit last instruction
    // realCt == number of instructions not counting this last one
    bytecode[realCt].func = bf_end;
    // run
    if (verbose) fprintf(stderr, "translated to %zu bytecode instructions\n", realCt);
    tape = tapePtr = calloc((size_t) tapeLength, sizeof(unsigned char));
    if (!tape) {
        fprintf(stderr, "err: failed to allocate tape\n");
        free(bytecode);
        return 1;
    }
#ifndef NO_CHECK_BOUNDS
    tapeEnd = tape + tapeLength;
#endif
    // ip
    Code* ptr = bytecode;
    while (ptr) {
        ptr = ptr->func(ptr);
    }
    free(bytecode);
    free(tape);
    return 0;
}