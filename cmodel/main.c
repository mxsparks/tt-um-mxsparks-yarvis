#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mem.h"

extern memword_t yarvis_step(mem_t *mem, regfile_t *regs, memword_t pc);

static inline void usage(void) {
    fprintf(stderr, "Usage: yarvis [-h] [-v] "
                    "[-s output.signature] "
                    "[-g signature_granularity] "
                    "[-n num_cycles] "
                    "-e input.elf\n");
}

int main(int argc, char *argv[]) {
    int ch, verbose = 0;
    FILE *elffile = NULL;
    FILE *sigfile = NULL;
    unsigned int signature_granularity = 4;
    unsigned long num_cycles = 0;
    mem_t *mem;
    regfile_t *regs;
    memword_t pc;


    while ((ch = getopt(argc, argv, "e:g:hn:s:v")) != -1) {
        switch (ch) {
            case 'e':
                if (!(elffile = fopen(optarg, "r"))) {
                    perror(optarg);
                    return 1;
                }
                break;
            case 'g':
                signature_granularity = strtoul(optarg, NULL, 0);
                break;
            case 'h':
                usage();
                return 0;
            case 'n':
                num_cycles = strtoul(optarg, NULL, 0);
                break;
            case 's':
                if (!(sigfile = fopen(optarg, "w"))) {
                    perror(optarg);
                    return 1;
                }
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                usage();
                return 1;
        }
    }

    if (!elffile) {
        usage();
        return 1;
    }

    mem = mem_loadelf(elffile);
    fclose(elffile);
    regs = calloc(1, sizeof(regfile_t));
    pc = mem->entry_point;
    unsigned long time;

    for (time = 0; (num_cycles == 0) || (num_cycles > time); time++) {
        pc = yarvis_step(mem, regs, pc);
        if ((ch = mem_read(mem, mem->symbols[SYM_TOHOST], 4))) {
            break;
        }
    }

    if (verbose) {
        fprintf(stderr, "Finished: t=%lu pc=%#x .tohost=%#x\n", time, pc - 4, ch);
        reg_describe(regs);
    }
    if (sigfile) {
        mem_dump_signature(mem, sigfile, signature_granularity);
        fclose(sigfile);
    }
    return 0;
}
