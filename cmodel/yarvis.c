#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "mem.h"
#include "riscv.h"

#define ASSERT_LEGAL(condition, info) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: Illegal instruction %08x at address %08x: %s\n", \
                __FILE__, __LINE__, ir.raw, pc, info); \
        exit(1); \
    } \
} while(0)

// Single-steps and returns the next program counter value.
memword_t yarvis_step(mem_t *mem, regfile_t *regs, memword_t pc) {
    instruction_t ir = { .raw = mem_read(mem, pc, 4) };
    base_opcode_t opcode = (ir.raw >> 2) & 0x1f;

    // Section 2.3 "Immediate Encoding Variants"
    bool imm_sign = ir.raw & (1 << 31);
    memword_t imm;
    switch (opcode) {
        // R-type
        case OP_OP:
            break;
        // I-type
        case OP_OPIMM:
        case OP_JALR:
        case OP_LOAD:
        case OP_MISCMEM:
        case OP_SYSTEM:
            imm = (ir.i.imm11_0 << 0)
                | (imm_sign ? 0xfffff000 : 0);
            break;
        // S-type
        case OP_STORE:
            imm = (ir.s.imm4_0 << 0)
                | (ir.s.imm11_5 << 5)
                | (imm_sign ? 0xfffff000 : 0);
            break;
        // B-type
        case OP_BRANCH:
            imm = (ir.b.imm4_1 << 1)
                | (ir.b.imm10_5 << 5)
                | (ir.b.imm11 << 11)
                | (imm_sign ? 0xfffff000 : 0);
            break;
        // U-type
        case OP_LUI:
        case OP_AUIPC:
            imm = (ir.u.imm31_12 << 12);
            break;
        // J-type
        case OP_JAL:
            imm = (ir.j.imm10_1 << 1)
                | (ir.j.imm11 << 11)
                | (ir.j.imm19_12 << 12)
                | (imm_sign ? 0xfff00000 : 0);
            break;
        default:
            ASSERT_LEGAL(false, "unsupported opcode");
    }

    bool taken, altfunc;
    memword_t addr, data, operand1, operand2, result;
    switch (opcode) {
        case OP_OP: // Section 2.4.2 "Integer Register-Register Operations"
            switch (ir.r.funct3) {
                case F3_ADD_SUB:
                case F3_SRL_SRA:
                    ASSERT_LEGAL(!(ir.r.funct7 & 0x5f), "invalid funct7");
                    break;
                default:
                    ASSERT_LEGAL(!ir.r.funct7, "invalid funct7");
                    break;
            }
            // fallthrough
        case OP_OPIMM: // Section 2.4.1 "Integer Register-Immediate Instructions"
            altfunc = ir.r.funct7 & 0x20;
            operand1 = reg_read(regs, ir.r.rs1);
            operand2 = (opcode == OP_OP) ? reg_read(regs, ir.r.rs2) : imm;
            switch (ir.r.funct3) {
                case F3_ADD_SUB:
                    if ((opcode == OP_OP) && altfunc) {
                        result = operand1 - operand2;
                    } else {
                        result = operand1 + operand2;
                    }
                    break;
                case F3_SLT:
                    result = (int32_t)operand1 < (int32_t)operand2;
                    break;
                case F3_SLTU:
                    result = operand1 < operand2;
                    break;
                case F3_AND:
                    result = operand1 & operand2;
                    break;
                case F3_OR:
                    result = operand1 | operand2;
                    break;
                case F3_XOR:
                    result = operand1 ^ operand2;
                    break;
                case F3_SLL:
                    ASSERT_LEGAL(!ir.r.funct7, "invalid funct7");
                    result = operand1 << (operand2 & 0x1f);
                    break;
                case F3_SRL_SRA:
                    ASSERT_LEGAL(!(ir.r.funct7 & 0x5f), "invalid funct7");
                    result = operand1 >> (operand2 & 0x1f);
                    if ((operand1 & 0x80000000) && (operand2 & 0x1f) && altfunc) {
                        result |= 0xffffffff << (32 - (operand2 & 0x1f));
                    }
                    break;
                default:
                    ASSERT_LEGAL(false, "unreachable");
            }
            reg_write(regs, ir.i.rd, result);
            break;
        case OP_LUI:
            reg_write(regs, ir.u.rd, imm);
            break;
        case OP_AUIPC:
            reg_write(regs, ir.u.rd, imm + pc);
            break;
        // Section 2.5.1 "Unconditional Jumps"
        case OP_JAL:
            reg_write(regs, ir.j.rd, pc + 4);
            return pc + imm;
        case OP_JALR:
            ASSERT_LEGAL(ir.i.funct3 == F3_JALR, "invalid funct3");
            addr = (reg_read(regs, ir.i.rs1) + imm) & 0xfffffffe;
            reg_write(regs, ir.i.rd, pc + 4);
            return addr;
        // Section 2.5.2 "Conditional Branches"
        case OP_BRANCH:
            operand1 = reg_read(regs, ir.b.rs1);
            operand2 = reg_read(regs, ir.b.rs2);
            switch (ir.b.funct3) {
                case F3_BEQ:
                    taken = operand1 == operand2;
                    break;
                case F3_BNE:
                    taken = operand1 != operand2;
                    break;
                case F3_BLT:
                    taken = (int32_t)operand1 < (int32_t)operand2;
                    break;
                case F3_BLTU:
                    taken = operand1 < operand2;
                    break;
                case F3_BGE:
                    taken = (int32_t)operand1 >= (int32_t)operand2;
                    break;
                case F3_BGEU:
                    taken = operand1 >= operand2;
                    break;
                default:
                    ASSERT_LEGAL(false, "invalid funct3");
            }
            if (taken) {
                return pc + imm;
            }
            break;
        // Section 2.6 "Load and Store Instructions"
        case OP_LOAD:
            addr = reg_read(regs, ir.i.rs1) + imm;
            switch (ir.i.funct3) {
                case F3_BYTE:
                case F3_BYTEU:
                    data = mem_read(mem, addr, 1) & 0xff;
                    if ((ir.i.funct3 == F3_BYTE) && (data & 0x80)) data |= 0xffffff00;
                    break;
                case F3_HWORD:
                case F3_HWORDU:
                    data = mem_read(mem, addr, 2) & 0xffff;
                    if ((ir.i.funct3 == F3_HWORD) && (data & 0x8000)) data |= 0xffff0000;
                    break;
                case F3_WORD:
                    data = mem_read(mem, addr, 4);
                    break;
                default:
                    ASSERT_LEGAL(false, "invalid funct3");
            }
            reg_write(regs, ir.i.rd, data);
            break;
        case OP_STORE:
            addr = reg_read(regs, ir.s.rs1) + imm;
            data = reg_read(regs, ir.s.rs2);
            switch (ir.s.funct3) {
                case F3_BYTE:
                    mem_write(mem, addr, 1, data & 0xff);
                    break;
                case F3_HWORD:
                    mem_write(mem, addr, 2, data & 0xffff);
                    break;
                case F3_WORD:
                    mem_write(mem, addr, 4, data);
                    break;
                default:
                    ASSERT_LEGAL(false, "invalid funct3");
            }
            break;
        // Section 2.7 "Memory Ordering Instructions"
        case OP_MISCMEM:
            ASSERT_LEGAL((ir.i.funct3 == F3_FENCE) || (ir.i.funct3 == F3_FENCEI), "invalid funct3");
            // FENCE and FENCE.I are no-ops for now
            break;
        // Section 2.8 "Environment Call and Breakpoints"
        case OP_SYSTEM:
            ASSERT_LEGAL(ir.i.funct3 == F3_PRIV, "invalid funct3"); // Zicsr is not implemented
            ASSERT_LEGAL((imm == F12_ECALL) || (imm == F12_EBREAK), "invalid funct12");
            // ECALL and EBREAK are no-ops for now
            break;
        default:
            ASSERT_LEGAL(false, "unreachable");
    }
    return pc + 4;
}
