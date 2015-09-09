/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License 2.  See the file "COPYING-GPL-2" in the main directory of this
 * archive for more details.
 *
 * Copyright (C) 2013  MIPS Technologies, Inc.  All rights reserved.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 */

#include <sys/types.h>
#include <elf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "../util/debug.h"
#include "loader.h"
#include "mipsregs.h"

//#define LOADER_DEBUG

#define OUR_ELF_ENDIAN      ELFDATA2LSB
#define ELF_OUR_MACHINE     EM_MIPS

#define SYM_TEXT    1
#define SYM_DATA    2

#define ELF_SYMTAB      ".symtab"       /* symbol table */
#define ELF_TEXT        ".text"         /* code */
#define ELF_STRTAB      ".strtab"

#define IS_ELF(ehdr) ((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
                      (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
                      (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
                      (ehdr).e_ident[EI_MAG3] == ELFMAG3)



struct sym_ent
{
    l4_int8_t *name;               /* name of symbol entry */
    l4_uint8_t type;               /* type - either text or data for now */
    l4_addr_t  addr;               /* address of symbol */
};
typedef struct sym_ent Sym_ent;

struct sym_table
{
    Sym_ent    *list;              /* unordered */
    l4_uint32_t num;
};
typedef struct sym_table Sym_table;

#define ELF_VADDR_TO_OFFSET(base,addr,offset) ((base) + MIPS_KSEG0_TO_PHYS((addr)) - (offset))


bool
loader_elf_is_exec(l4_uint8_t * load, l4_uint32_t loadlen)
{
    Elf32_Ehdr *hdr = (Elf32_Ehdr *) load;

    if (loadlen >= sizeof *hdr &&
        IS_ELF(*hdr) &&
        hdr->e_ident[EI_CLASS] == ELFCLASS32 &&
        hdr->e_ident[EI_DATA] == OUR_ELF_ENDIAN &&
        hdr->e_type == ET_EXEC && hdr->e_machine == ELF_OUR_MACHINE)
        return true;

    return false;
}

/* prepare to run the ELF image
 */
int
loader_elf_load(l4_uint8_t * load, l4_uint32_t loadlen, l4_uint8_t *mem,
              l4_addr_t vaddr_offset, l4_addr_t *p_entrypoint, l4_addr_t *p_highaddr)
{
    Elf32_Ehdr *hdr = (Elf32_Ehdr *) load;
    Elf32_Phdr *phdr;
    /*Elf32_Shdr *shdr; */
    int i;
    l4_addr_t highaddr = 0;

    if (!loader_elf_is_exec(load, loadlen))       /* sanity check */
        return -L4_EINVAL;

    /* copy our headers to temp memory to let us copy over our image */
    i = sizeof *hdr + hdr->e_phoff + sizeof *phdr * hdr->e_phnum;
#ifdef LOADER_DEBUG
    karma_log(DEBUG, "[elf loader] sizeof *hdr=%d sizeof *phdr=%d phnum=%d len=%d\n",
           sizeof *hdr, sizeof *phdr, hdr->e_phnum, i);
#endif

    hdr = (Elf32_Ehdr *) malloc(i);

    if (hdr == NULL) {
        karma_log(ERROR, "[elf loader] Cannot allocate enough memory for ELF headers?!?\n");
        return -L4_ENOMEM;
    }

    memcpy(hdr, load, i);

    phdr = (Elf32_Phdr *) ((char *) hdr + hdr->e_phoff);
    /*shdr = (Elf32_Shdr*)(load + hdr->e_shoff); */
    *p_entrypoint = hdr->e_entry; /* MUST return this */

    /* load all programs in this file */
    for (i = 0; i < hdr->e_phnum; i++, phdr++) {
        if (phdr->p_type != PT_LOAD)    /* only loadable types */
            continue;

#ifdef LOADER_DEBUG
        karma_log(DEBUG, "[elf loader] vaddr %#lx, paddr %#lx, load %#lx, load+off %#lx, msize %#x, fsize %#x\n",
               (l4_addr_t)phdr->p_vaddr, MIPS_KSEG0_TO_PHYS(phdr->p_vaddr),
               (l4_addr_t)load, (l4_addr_t)(load + phdr->p_offset), phdr->p_memsz, phdr->p_filesz);
#endif

        /* copy in the entire chunk of program */
        if (phdr->p_filesz > 0) {
#ifdef LOADER_DEBUG
            karma_log(DEBUG, "[elf loader] ELF_VADDR_TO_OFFSET(%p, 0x%08lx, 0x%08lx): 0x%08lx\n", mem,
                   (l4_addr_t)phdr->p_vaddr, (l4_addr_t)vaddr_offset,
                   (l4_addr_t)ELF_VADDR_TO_OFFSET(mem, phdr->p_vaddr, vaddr_offset));
#endif
            memmove((char *) ELF_VADDR_TO_OFFSET(mem, phdr->p_vaddr, vaddr_offset),
                    load + phdr->p_offset, phdr->p_filesz);
        }

        /* if in-memory size is larger, zero out the difference */
        if (phdr->p_memsz > phdr->p_filesz)
            memset((char *) ELF_VADDR_TO_OFFSET(mem, phdr->p_vaddr, vaddr_offset) +
                   phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);

        karma_log(INFO, "[elf loader]  ... loaded vaddr 0x%08lx:%08lx @ addr 0x%08lx:%08lx\n",
            (l4_addr_t)phdr->p_vaddr, (l4_addr_t)(phdr->p_vaddr+phdr->p_memsz),
            (l4_addr_t)ELF_VADDR_TO_OFFSET(mem, phdr->p_vaddr, vaddr_offset),
            (l4_addr_t)ELF_VADDR_TO_OFFSET(mem, phdr->p_vaddr+phdr->p_memsz, vaddr_offset));

        if ((phdr->p_vaddr + phdr->p_memsz) > highaddr)
            highaddr = phdr->p_vaddr + phdr->p_memsz;
    }

    *p_highaddr = highaddr;

    /*for (i = 0; i < hdr->e_shnum; i++, shdr++)
       load_section(shdr); */

    free(hdr);
    return 0;
}
