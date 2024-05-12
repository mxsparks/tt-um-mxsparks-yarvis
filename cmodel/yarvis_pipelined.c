#include <stdbool.h>
#include "yarvis.h"

typedef struct {
    // to AHB
    uint32_t haddr;
    bool htrans;

    // to next pipeline stage
    uint32_t pc;
    uint32_t imm;
    unsigned int opcode;
    unsigned int funct3;
    unsigned int rd;
    unsigned int rs1;
    unsigned int rs2;
    unsigned int funct7;
    bool exc_instruction_access_fault;
    bool valid;
} ifetch_flops_t;


void yarvis_stage1_ifetch_idecode(
    const ifetch_flops_t *prev,
    ifetch_flops_t *next,

    uint32_t hrdata,
    bool hready,
    bool hresp,

    bool branch_taken,
    address_t branch_target,
    bool stall,
) {
    if (branch_taken) {
        next->valid = false;
        next->imem_addr = branch_target;
        next->imem_addr_valid = true;
    }

    if (imem_data_valid && !stall) {


}
