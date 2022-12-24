bf
=====

An optimizing brainf*ck interpreter written in C.

## Usage
    usage: bf [-e eof_value] [-t tape_size] [-bcfhpv] file
        -e eof_value: integer value of EOF (omit option to leave cell unchanged)
        -t tape_size: length of tape (default: 30000)
        -b: enable bounds checking
        -c: enable circular tape (implies -b)
        -f: enable infinite tape (implies -b)
        -h: print this help
        -p: print minified code to stdout instead of running it
        -v: show verbose messages

Program input is taken from stdin and output on stdout.

## Build
    make

## Optimizations
 * Recognizes simple loops to set current cell to 0 (`[-]`, `[+]`)
 * Lumps sequences of `+`/`-`, `<`/`>`, `[`/`]` into one instruction
 * Compute jumps ahead of time
 * Skips loops that cannot be entered