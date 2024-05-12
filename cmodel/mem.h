#ifndef _mem_h_
#define _mem_h_

#if RV64I
typedef uint64_t memaddr_t, memword_t;
#else
typedef uint32_t memaddr_t, memword_t;
#endif

typedef struct {
    memaddr_t address;
    memaddr_t size;
    void *data;
} memregion_t;

enum {
    SYM_BEGIN_SIGNATURE,
    SYM_END_SIGNATURE,
    SYM_FROMHOST,
    SYM_TOHOST,
    NUM_SYMS,
};

typedef struct {
    memaddr_t entry_point;
    unsigned int num_regions;
    memaddr_t symbols[NUM_SYMS];
    memregion_t regions[];
} mem_t;

mem_t *mem_loadelf(FILE *fh);
void mem_describe(mem_t *mem, FILE *fh);
memword_t mem_read(const mem_t *mem, memaddr_t address, memaddr_t size);
void mem_write(mem_t *mem, memaddr_t address, memaddr_t size, memword_t data);
void mem_dump_signature(mem_t *mem, FILE *fh, unsigned int granularity);
void mem_destroy(mem_t *mem);

#if RV32E
#define NUM_REGS 16
#else
#define NUM_REGS 32
#endif

typedef memword_t regfile_t[NUM_REGS];

memword_t reg_read(const regfile_t *regs, unsigned int i);
void reg_write(regfile_t *regs, unsigned int i, memword_t value);
void reg_describe(regfile_t *regs);
void reg_destroy(regfile_t *regs);

#endif // _mem_h_
