/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * l4_linux.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "l4_linux.h"

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>

#include <l4/util/util.h>
#include <l4/sys/cache.h>

#include <stdlib.h>

#include "mipsregs.h"
#include "loader.h"
#include "linux_prom.h"
#include "vmm.h"
#include "util/debug.h"
#include "l4_vm.h"

L4_linux::L4_linux(L4_mem *l4_mem) : _guest_entrypoint(0), _memory(l4_mem),
    _guest_argc(0), _guest_argv(0)
{
    setup_os();
}

L4_linux::~L4_linux() {}

void L4_linux::setup_os()
{
    setup_memory();
    setup_prom();
}

void L4_linux::backtrace(l4_umword_t ip, l4_umword_t stackframe)
{
    karma_log(INFO, "Backtrace Linux:\n%x, %x\n", (unsigned int)ip, (unsigned int)stackframe);
    // KYMA TODO backtrace not implemented
    //karma_log(INFO, "End of backtrace\n");
}

void L4_linux::setup_memory()
{
    l4_addr_t lx_image_addr;
    l4_addr_t entrypoint;
    l4_addr_t kernelhigh;
    const l4_addr_t offset_into_guestspace = 0;
    int err = 0;

    // get bzImage and ramdisk ready
    lx_image_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    lx_ramdisk_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if(!lx_image_ds.is_valid()
        || !lx_ramdisk_ds.is_valid() )
    {
        karma_log(ERROR, "[linux] Fatal error, could not allocate Linux image or ramdisk capabilities. Terminating.\n");
        exit(1);
    }

    L4::Cap<L4Re::Namespace> rom = L4Re::Env::env()->get_cap<L4Re::Namespace>("rom");

    if (!rom)
    {
        karma_log(ERROR, "[linux] Fatal error, could not find rom. Terminating.\n");
        exit(1);
    }


    // get dataspace capablities of lx image and ramdisk
    if(rom->query(params.kernel_name, lx_image_ds))
    {
        karma_log(ERROR, "[linux] Fatal error, could not find Linux kernel image \"rom/%s\". Terminating.\n", params.kernel_name);
        exit(1);
    }

    // attach both lx image and ramdisk
    if((err = L4Re::Env::env()->rm()->attach((void**)&lx_image_addr,
                                      lx_image_ds->size(),
                                      L4Re::Rm::Read_only |
                                      L4Re::Rm::Search_addr,
                                      lx_image_ds)))
    {
        karma_log(ERROR, "[linux] Fatal error, could not attach Linux image dataspace (error = %d). Terminating.\n", err);
        exit(1);
    }

    karma_log(INFO, "[linux] Loading guest image...\n");

    /* load guest image into guest memory */
    if((err = loader_elf_load((l4_uint8_t*)lx_image_addr, lx_image_ds->size(),
                             (l4_uint8_t*)_memory->base_ptr(), offset_into_guestspace,
                             &entrypoint, &kernelhigh)))
    {
        karma_log(ERROR, "[linux] Error parsing Linux ELF image %s (error = %d, %s). Terminating.\n",
                params.kernel_name, err, l4sys_errtostr(err));
        exit(1);
    }

    _guest_entrypoint = entrypoint;

    karma_log(INFO, "[linux] the guest OS entrypoint is %#08lx\n", entrypoint);

    // make changes to instruction stream effective
    // call syncICache using root address
    syncICache(_memory->base_ptr(), _memory->get_mem_size());

    if(rom->query(params.ramdisk_name, lx_ramdisk_ds))
    {
        karma_log(ERROR, "[linux] Fatal error, could not find Linux ramdisk \"rom/%s\". Terminating.\n", params.ramdisk_name);
        exit(1);
    }

    if((err = L4Re::Env::env()->rm()->attach((void**)&_lx_ramdisk_addr,
                                      lx_ramdisk_ds->size(),
                                      L4Re::Rm::Search_addr,
                                      lx_ramdisk_ds)))
    {
        karma_log(ERROR, "[linux] Fatal error, could not attach ramdisk dataspace (error = %d). Terminating.\n", err);
        exit(1);
    }

    _lx_ramdisk_size = lx_ramdisk_ds->size();

    // copy ramdisk into vm memory after kernel image
    _guest_ramdisk_addr = l4_round_page(kernelhigh);
    _memory->copyin(_lx_ramdisk_addr, MIPS_KSEG0_TO_PHYS(_guest_ramdisk_addr), _lx_ramdisk_size);

    karma_log(INFO, "[linux] the guest OS ramdisk is loaded at %#lx:%lx\n",
            _guest_ramdisk_addr, _guest_ramdisk_addr + _lx_ramdisk_size);
}

l4_addr_t L4_linux::get_guest_entrypoint()
{
    return _guest_entrypoint;
}

void L4_linux::setup_prom() {
    if (params.paravirt_guest)
        setup_prom_paravirt();
    else
        setup_prom_machvirt();
}

void L4_linux::setup_prom_paravirt()
{
    l4_uint32_t prom_index = 0;
    l4_uint32_t prom_size;
    l4_uint32_t* prom_buf;
    L4_vm &vm = GET_VM;

    // Setup Linux kernel prom parameters
    prom_size = ENVP_NB_ENTRIES * (sizeof(l4_int32_t) + ENVP_ENTRY_SIZE);
    prom_buf = (l4_uint32_t*)malloc(prom_size);

    // Setup kernel arguments table
    prom_set(prom_buf, prom_index++, "%s", params.kernel_name);
    prom_set(prom_buf, prom_index++, "mem=%i", _memory->get_mem_size());
    if (params.ramdisk_name)
        prom_set(prom_buf, prom_index++, "rd_start=0x%lx rd_size=%li",
                 MIPS_PHYS_TO_KSEG0(_guest_ramdisk_addr), _lx_ramdisk_size);
    if (params.virtual_cpus)
        prom_set(prom_buf, prom_index++, "numcpus=%i", vm.nr_available_cpus());
    prom_set(prom_buf, prom_index++, "%s", params.kernel_opts);
    prom_set(prom_buf, prom_index++, NULL);

    _memory->copyin((l4_addr_t)prom_buf, MIPS_KSEG0_TO_PHYS(ENVP_ADDR), prom_size);
    _guest_argc = prom_nb_entries(prom_buf);
    _guest_argv = ENVP_ADDR;

    prom_dump(prom_buf);

    free(prom_buf);
}

void L4_linux::setup_prom_machvirt()
{
    l4_uint32_t prom_index = 0;
    l4_uint32_t prom_size;
    l4_uint32_t* prom_buf;

    // Setup Linux kernel prom parameters
    prom_size = ENVP_NB_ENTRIES * (sizeof(l4_int32_t) + ENVP_ENTRY_SIZE);
    prom_buf = (l4_uint32_t*)malloc(prom_size);

    // Setup kernel arguments table with two args: kernel name and command line
    prom_set(prom_buf, prom_index++, "%s", params.kernel_name);
    if (params.ramdisk_name) {
        prom_set(prom_buf, prom_index++, "rd_start=0x%lx rd_size=%li %s",
                 MIPS_PHYS_TO_KSEG0(_guest_ramdisk_addr), _lx_ramdisk_size,
                 params.kernel_opts);
    } else {
        prom_set(prom_buf, prom_index++, "%s", params.kernel_opts);
    }

    // Setup environment table
    prom_set(prom_buf, prom_index++, "memsize");
    prom_set(prom_buf, prom_index++, "%i", _memory->get_mem_size());
    prom_set(prom_buf, prom_index++, "modetty0");
    prom_set(prom_buf, prom_index++, "38400n8r");
    prom_set(prom_buf, prom_index++, NULL);

    _memory->copyin((l4_addr_t)prom_buf, MIPS_KSEG0_TO_PHYS(ENVP_ADDR), prom_size);

    prom_dump(prom_buf);

    free(prom_buf);
}

void L4_linux::guest_os_init(void *context)
{
    l4_vm_vz_vmcb_t * vmm = reinterpret_cast<l4_vm_vz_vmcb_t*>(context);

    karma_log(INFO, "Starting VM Guest kernel_name=%s @ 0x%lx\n",
        params.kernel_name, vmm->vm_guest_entrypoint);

    vmm_regs(vmm).ip = vmm->vm_guest_entrypoint;

    if (params.paravirt_guest)
    {
        // Setup registers a0 to a3 to contain:
        //   a0 - number of kernel arguments
        //   a1 - 32-bit address of the kernel arguments table
        //   a2 - 0
        //   a3 - 0
        vmm_regs(vmm).r[4] = _guest_argc;
        vmm_regs(vmm).r[5] = _guest_argv;
        vmm_regs(vmm).r[6] = 0;
        vmm_regs(vmm).r[7] = 0;
    } else {
        // Setup registers a0 to a3 to contain:
        //   a0 - number of kernel arguments
        //   a1 - 32-bit address of the kernel arguments table
        //   a2 - 32-bit address of the environment variables table
        //   a3 - RAM size in bytes
        vmm_regs(vmm).r[4] = 2;
        vmm_regs(vmm).r[5] = ENVP_ADDR;
        vmm_regs(vmm).r[6] = ENVP_ADDR + 8;
        vmm_regs(vmm).r[7] = vmm->vm_guest_mem_size;
    }

    karma_log(INFO, "VM Guest params a0 %lx a1 %lx a2 %lx a3 %lx\n",
        vmm_regs(vmm).r[4],
        vmm_regs(vmm).r[5],
        vmm_regs(vmm).r[6],
        vmm_regs(vmm).r[7]);
}

// Select Linux as the OS
L4_os * L4_os::create_os(L4_mem *l4_mem)
{
    return new L4_linux(l4_mem);
}
