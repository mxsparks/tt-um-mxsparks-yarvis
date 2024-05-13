#ifndef _riscv_h_
#define _riscv_h_

typedef enum {
    // Table 74 "RISC-V base opcode map, inst[1:0] = 11"
    OP_LOAD,    OP_LOADFP,  OP_CUSTOM0, OP_MISCMEM, OP_OPIMM,   OP_AUIPC,   OP_OPIMM32, OP_48BIT0,
    OP_STORE,   OP_STOREFP, OP_CUSTOM1, OP_AMO,     OP_OP,      OP_LUI,     OP_OP32,    OP_64BIT,
    OP_MADD,    OP_MSUB,    OP_NMSUB,   OP_NMADD,   OP_OPFP,    OP_OPV,     OP_CUSTOM2, OP_48BIT2,
    OP_BRANCH,  OP_JALR,    OP_RSVD,    OP_JAL,     OP_SYSTEM,  OP_OPVE,    OP_CUSTOM3, OP_80BIT,
} base_opcode_t;

typedef enum {
    F3_ADD_SUB, F3_SLL,     F3_SLT,     F3_SLTU,    F3_XOR,     F3_SRL_SRA, F3_OR,      F3_AND,
} funct3_op_t;

typedef enum {
    F3_BEQ,     F3_BNE,     /* rsvd */  /* rsvd */  F3_BLT = 4, F3_BGE,     F3_BLTU,    F3_BGEU,
} funct3_branch_t;

typedef enum {
    F3_BYTE,    F3_HWORD,   F3_WORD,    F3_DWORD,   F3_BYTEU,   F3_HWORDU,  F3_WORDU,   /* rsvd */
} funct3_loadstore_t;

typedef enum {
    F3_FENCE,   F3_FENCEI,  /* rsvd */  /* rsvd */  /* rsvd */  /* rsvd */  /* rsvd */  /* rsvd */
} funct3_miscmem_t;

typedef enum {
    F3_PRIV,    F3_CSRRW,   F3_CSRRS,   F3_CSRRC,   /* rsvd */  F3_CSRRWI=5,F3_CSRRSI,  F3_CSRRCI,
} funct3_system_t;

typedef enum {
    F3_JALR,    /* rsvd */  /* rsvd */  /* rsvd */  /* rsvd */  /* rsvd */  /* rsvd */  /* rsvd */
} funct3_jalr_t;

typedef enum {
    F12_ECALL,
    F12_EBREAK,
    F12_MRET = 0x302,
    F12_WFI = 0x105,
} funct12_priv_t;

typedef union {
    struct {
        unsigned int quadrant:2;
        unsigned int opcode : 5;
        unsigned int rd     : 5;
        unsigned int funct3 : 3;
        unsigned int rs1    : 5;
        unsigned int rs2    : 5;
        unsigned int funct7 : 7;
    } r;
    struct {
        unsigned int quadrant:2;
        unsigned int opcode : 5;
        unsigned int rd     : 5;
        unsigned int funct3 : 3;
        unsigned int rs1    : 5;
        unsigned int imm11_0: 12;
    } i;
    struct {
        unsigned int quadrant:2;
        unsigned int opcode : 5;
        unsigned int imm4_0 : 5;
        unsigned int funct3 : 3;
        unsigned int rs1    : 5;
        unsigned int rs2    : 5;
        unsigned int imm11_5: 7;
    } s;
    struct {
        unsigned int quadrant:2;
        unsigned int opcode : 5;
        unsigned int imm11  : 1;
        unsigned int imm4_1 : 4;
        unsigned int funct3 : 3;
        unsigned int rs1    : 5;
        unsigned int rs2    : 5;
        unsigned int imm10_5: 6;
        unsigned int imm12  : 1;
    } b;
    struct {
        unsigned int quadrant:2;
        unsigned int opcode : 5;
        unsigned int rd     : 5;
        unsigned int imm31_12:20;
    } u;
    struct {
        unsigned int quadrant:2;
        unsigned int opcode : 5;
        unsigned int rd     : 5;
        unsigned int imm19_12:8;
        unsigned int imm11  : 1;
        unsigned int imm10_1: 10;
        unsigned int imm20  : 1;
    } j;
    uint32_t raw;
} instruction_t;

#endif // _riscv_h_
