#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mem.h"
#include "riscv.h"

memword_t yarvis_imm_extend(instruction_t ir) {
    bool imm_sign = ir.raw & (1 << 31);
    if (ir.r.quadrant != 3) {
        assert(false);
    }
    switch (ir.r.opcode) {
        case OP_OPIMM:
        case OP_JALR:
        case OP_LOAD:
        case OP_MISCMEM:
        case OP_SYSTEM:
            return (ir.i.imm11_0 << 0)
                   | (imm_sign ? ~((1 << 12) - 1) : 0);
        case OP_STORE:
            return (ir.s.imm4_0 << 0)
                   | (ir.s.imm11_5 << 5)
                   | (imm_sign ? ~((1 << 12) - 1) : 0);
        case OP_BRANCH:
            return (ir.b.imm4_1 << 1)
                   | (ir.b.imm10_5 << 5)
                   | (ir.b.imm11 << 11)
                   | (imm_sign ? ~((1 << 12) - 1) : 0);
        case OP_LUI:
        case OP_AUIPC:
            return (ir.u.imm31_12 << 12);
        case OP_JAL:
            return (ir.j.imm10_1 << 1)
                   | (ir.j.imm11 << 11)
                   | (ir.j.imm19_12 << 12)
                   | (imm_sign ? ~((1 << 20) - 1) : 0);
        default: // R-type or illegal instruction
            assert(false);
    }
}

memword_t yarvis_alu(
    memword_t operand1,
    memword_t operand2,
    unsigned int funct3,
    bool isBranch,
    bool isRegReg,
    bool isRegImm,
    bool isAltFunc
) {
    unsigned int shamt = operand2 & (XLEN - 1);
    bool operand1_sign = operand1 & (1 << (XLEN - 1));
    if (isBranch) {
        switch (funct3) {
            case F3_BEQ:
                return operand1 == operand2;
            case F3_BNE:
                return operand1 != operand2;
            case F3_BLT:
                return (smemword_t)operand1 < (smemword_t)operand2;
            case F3_BLTU:
                return operand1 < operand2;
            case F3_BGE:
                return (smemword_t)operand1 >= (smemword_t)operand2;
            case F3_BGEU:
                return operand1 >= operand2;
            default: // illegal instruction
                assert(false);
        }
    }
    if (!isRegReg && !isRegImm) {
        return operand1 + operand2;
    }
    switch (funct3) {
        case F3_ADD_SUB:
            if (isRegReg && isAltFunc) {
                return operand1 - operand2;
            } else {
                return operand1 + operand2;
            }
        case F3_SLT:
            return (smemword_t)operand1 < (smemword_t)operand2;
        case F3_SLTU:
            return operand1 < operand2;
        case F3_AND:
            return operand1 & operand2;
        case F3_OR:
            return operand1 | operand2;
        case F3_XOR:
            return operand1 ^ operand2;
        case F3_SLL:
            return operand1 << shamt;
        case F3_SRL_SRA:
            if (isAltFunc && shamt && operand1_sign) {
                return (operand1 >> shamt) | (-1UL << (XLEN - shamt));
            } else {
                return operand1 >> shamt;
            }
        default: // unreachable
            assert(false);
    }
}

memword_t yarvis_step(mem_t *mem, regfile_t *regs, memword_t pc) {
    static enum {
        ST_IFETCH,
        ST_DECODE,
        ST_EXECUTE,
        ST_BRANCH,
        NUM_STATES,
    } state = ST_IFETCH;
    static instruction_t ir;
    static memword_t operand1, operand2, mem_data;

    memword_t pcPlus4 = pc + 4;
    memword_t result = yarvis_alu(operand1, operand2, ir.r.funct3,
                                  ir.r.opcode == OP_BRANCH && state != ST_BRANCH,
                                  ir.r.opcode == OP_OP,
                                  ir.r.opcode == OP_OPIMM,
                                  ir.r.funct7 & 0x20);
    unsigned int mem_size;
    switch (ir.r.funct3) {
        case F3_BYTE:
        case F3_BYTEU:
            mem_size = 1;
            break;
        case F3_HWORD:
        case F3_HWORDU:
            mem_size = 2;
            break;
        case F3_WORD:
            mem_size = 4;
            break;
        default: // don't care / illegal instruction
            assert(ir.r.opcode != OP_LOAD && ir.r.opcode != OP_STORE);
            break;
    }

    switch (state) {
        case ST_IFETCH:
            ir.raw = mem_read(mem, pc, 4);
            state = ST_DECODE;
            return pc;
        case ST_DECODE:
            switch (ir.r.opcode) {
                case OP_LUI:
                    operand1 = 0;
                    break;
                case OP_AUIPC:
                case OP_JAL:
                    operand1 = pc;
                    break;
                default:
                    operand1 = reg_read(regs, ir.r.rs1);
                    break;
            }
            switch (ir.r.opcode) {
                case OP_OP:
                case OP_BRANCH:
                    operand2 = reg_read(regs, ir.r.rs2);
                    break;
                default:
                    operand2 = yarvis_imm_extend(ir);
                    break;
            }
            state = ST_EXECUTE;
            return pc;
        case ST_EXECUTE:
            switch (ir.r.opcode) {
                case OP_OP:
                case OP_OPIMM:
                case OP_LUI:
                case OP_AUIPC:
                    reg_write(regs, ir.r.rd, result);
                    state = ST_IFETCH;
                    return pcPlus4;
                case OP_JAL:
                case OP_JALR:
                    reg_write(regs, ir.r.rd, pcPlus4);
                    state = ST_IFETCH;
                    return result & ~(1UL);
                case OP_BRANCH:
                    if (result) {
                        operand1 = pc;
                        operand2 = yarvis_imm_extend(ir);
                        state = ST_BRANCH;
                        return pc;
                    } else {
                        state = ST_IFETCH;
                        return pcPlus4;
                    }
                case OP_LOAD:
                    mem_data = mem_read(mem, result, mem_size);
                    if (ir.r.funct3 == F3_BYTE && (mem_data & (1 << 7))) {
                        mem_data |= (-1UL) << 8;
                    }
                    if (ir.r.funct3 == F3_HWORD && (mem_data & (1 << 15))) {
                        mem_data |= (-1UL) << 16;
                    }
                    reg_write(regs, ir.r.rd, mem_data);
                    state = ST_IFETCH;
                    return pcPlus4;
                case OP_STORE:
                    mem_write(mem, result, mem_size, reg_read(regs, ir.r.rs2));
                    state = ST_IFETCH;
                    return pcPlus4;
                case OP_MISCMEM:
                case OP_SYSTEM:
                    state = ST_IFETCH;
                    return pcPlus4;
                default: // illegal instruction
                    assert(false);
            }
        case ST_BRANCH:
            state = ST_IFETCH;
            return result;
        default: // invalid state
            assert(false);
    }
}
