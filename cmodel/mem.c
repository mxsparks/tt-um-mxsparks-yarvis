#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf.h"
#include "mem.h"

#undef NDEBUG // FIXME: HACK: assertions in this file are load-bearing
#include <assert.h>

static const char elf32le_magic[EI_NIDENT] = {
    0x7f, 'E', 'L', 'F', ELFCLASS32, ELFDATA2LSB, EV_CURRENT, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};

static const char *const symbol_names[NUM_SYMS] = {
    "begin_signature",
    "end_signature",
    "fromhost",
    "tohost",
};

mem_t *mem_loadelf(FILE *fh) {
    mem_t *mem;
    Elf32_Ehdr ehdr;

    assert(!fseek(fh, 0, SEEK_SET));
    assert(fread(&ehdr, sizeof(ehdr), 1, fh));
    assert(!memcmp(ehdr.e_ident, elf32le_magic, EI_NIDENT));
    assert(ehdr.e_type == ET_EXEC);
    assert(ehdr.e_machine == EM_RISCV);
    assert(ehdr.e_version == EV_CURRENT);
    assert(ehdr.e_ehsize == sizeof(ehdr));
    assert(ehdr.e_phentsize == sizeof(Elf32_Phdr));
    assert(ehdr.e_shentsize == sizeof(Elf32_Shdr));
    mem = calloc(1, sizeof(mem_t) + ehdr.e_phnum * sizeof(memregion_t));
    assert(mem);
    assert(ehdr.e_entry);
    mem->entry_point = ehdr.e_entry;
    assert(mem->entry_point);

    for (int segment = 0; segment < ehdr.e_phnum; segment++) {
        Elf32_Phdr phdr;
        memaddr_t alignment;
        memregion_t *region = mem->regions + mem->num_regions;

        assert(!fseek(fh, ehdr.e_phoff + segment * ehdr.e_phentsize, SEEK_SET));
        assert(fread(&phdr, sizeof(phdr), 1, fh));
        if ((phdr.p_type != PT_LOAD) || (phdr.p_memsz == 0)) {
            continue;
        }
        if (mem->num_regions) {
            assert(phdr.p_vaddr >= (region - 1)->address + (region - 1)->size);
        }
        region->address = phdr.p_vaddr;
        assert(region->address);
        assert(phdr.p_memsz >= phdr.p_filesz);
        assert(!(phdr.p_align & (phdr.p_align - 1)));
        alignment = (phdr.p_align > sizeof(void *)) ? phdr.p_align : sizeof(void *);
        region->size = phdr.p_align
            * ((phdr.p_memsz / alignment) + ((phdr.p_memsz % alignment) ? 1 : 0));
        region->data = aligned_alloc(alignment, region->size);
        assert(region->data);
        assert(!fseek(fh, phdr.p_offset, SEEK_SET));
        assert(fread(region->data, phdr.p_filesz, 1, fh));
        memset((uint8_t *)region->data + phdr.p_filesz, 0, region->size - phdr.p_filesz);
        mem->num_regions++;
    }
    assert(mem->num_regions);

    for (int section = 0; section < ehdr.e_shnum; section++) {
        Elf32_Shdr symtab, strtab;
        Elf32_Sym sym;
        char *strings;
        int num_symbols;

        assert(!fseek(fh, ehdr.e_shoff + section * ehdr.e_shentsize, SEEK_SET));
        assert(fread(&symtab, sizeof(symtab), 1, fh));
        if (symtab.sh_type != SHT_SYMTAB) {
            continue;
        }
        assert(symtab.sh_entsize == sizeof(sym));
        assert(symtab.sh_link && (symtab.sh_link < ehdr.e_shnum));
        assert(!fseek(fh, ehdr.e_shoff + symtab.sh_link * ehdr.e_shentsize, SEEK_SET));
        assert(fread(&strtab, sizeof(strtab), 1, fh));
        assert(strtab.sh_type == SHT_STRTAB);
        assert(!(symtab.sh_size % symtab.sh_entsize));
        strings = malloc(strtab.sh_size);
        assert(strings);
        assert(!fseek(fh, strtab.sh_offset, SEEK_SET));
        assert(fread(strings, strtab.sh_size, 1, fh));

        num_symbols = symtab.sh_size / symtab.sh_entsize;
        for (int symbol = 0; symbol < num_symbols; symbol++) {
            int maxlength;

            assert(!fseek(fh, symtab.sh_offset + symbol * symtab.sh_entsize, SEEK_SET));
            assert(fread(&sym, sizeof(sym), 1, fh));
            if (ELF32_ST_BIND(sym.st_info) != STB_GLOBAL) {
                continue;
            }
            maxlength = strtab.sh_size - sym.st_name;
            assert(maxlength);
            for (int match = 0; match < NUM_SYMS; match++) {
                if (!strncmp(strings + sym.st_name, symbol_names[match], maxlength)) {
                    mem->symbols[match] = sym.st_value;
                    break;
                }
            }
        }
        free(strings);
        break;
    }
    return mem;
}

void mem_describe(mem_t *mem, FILE *fh) {
    assert(mem);
    fprintf(fh, "Entry point: %08x\n", mem->entry_point);
    for (int i = 0; i < NUM_SYMS; i++) {
        if (mem->symbols[i]) {
            fprintf(fh, "Symbol %s = %08x\n", symbol_names[i], mem->symbols[i]);
        }
    }
    for (unsigned int i = 0; i < mem->num_regions; i++) {
        memregion_t *region = mem->regions + i;
        uint8_t *data = (uint8_t *)(region->data);
        fprintf(fh, "Region %d: addr %08x, size %08x, data %02x%02x%02x%02x...\n",
                i, region->address, region->size, data[0], data[1], data[2], data[3]);
    }
}

static int memregion_compar(const void *address, const void *region) {
    memaddr_t addr = *(const memaddr_t *)address;
    memaddr_t min_addr = ((const memregion_t *)region)->address;
    memaddr_t max_addr = ((const memregion_t *)region)->size + min_addr;
    return (addr < min_addr) ? -1 : (addr < max_addr) ? 0 : 1;
}

static void *mem_search(const mem_t *mem, const memaddr_t address, memaddr_t size) {
    memregion_t *region;
    assert(mem);
    assert((size > 0) && !(size & (size - 1)) && (size <= sizeof(memword_t)));
    assert(!(address % size));
    assert((region = bsearch(&address, mem->regions, mem->num_regions, sizeof(memregion_t), memregion_compar)));
    assert(address + size <= region->address + region->size);
    return ((uint8_t *)region->data) + (address - region->address);
}

memword_t mem_read(const mem_t *mem, memaddr_t address, memaddr_t size) {
    void *memdata = mem_search(mem, address, size);
    switch (size) {
        case 1:
            return *(uint8_t *)memdata;
        case 2:
            return *(uint16_t *)memdata;
        case 4:
            return *(uint32_t *)memdata;
        default:
            assert(0); // unreachable
    }
}

void mem_write(mem_t *mem, memaddr_t address, memaddr_t size, memword_t data) {
    void *memdata = mem_search(mem, address, size);
    switch (size) {
        case 1:
            *(uint8_t *)memdata = data;
            break;
        case 2:
            *(uint16_t *)memdata = data;
            break;
        case 4:
            *(uint32_t *)memdata = data;
            break;
        default:
            assert(0); // unreachable
    }
}

void mem_destroy(mem_t *mem) {
    assert(mem);
    for (unsigned int i = 0; i < mem->num_regions; i++) {
        free(mem->regions[i].data);
    }
    free(mem);
}

void mem_dump_signature(mem_t *mem, FILE *fh, unsigned int granularity) {
    assert(mem && fh);
    for (memaddr_t address = mem->symbols[SYM_BEGIN_SIGNATURE];
         address < mem->symbols[SYM_END_SIGNATURE];
         address += granularity)
    {
        fprintf(fh, "%0*x\n", granularity * 2, mem_read(mem, address, granularity));
    }
}

memword_t reg_read(const regfile_t *regs, unsigned int i) {
    assert(regs && *regs);
    assert(i >= 0 && i < NUM_REGS);
    return i ? *regs[i] : 0;
}

void reg_write(regfile_t *regs, unsigned int i, memword_t value) {
    assert(regs && *regs);
    assert(i >= 0 && i < NUM_REGS);
    if (i) {
        *regs[i] = value;
    }
}

void reg_describe(regfile_t *regs) {
    assert(regs && *regs);
    for (int i = 0; i < NUM_REGS; i++) {
        fprintf(stderr, "x%02d = %08x%c", i, *regs[i], (i % 4 == 3) ? '\n' : '\t');
    }
}

void reg_destroy(regfile_t *regs) {
    assert(regs && *regs);
    free(*regs);
}
