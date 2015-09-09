/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * l4_fiasco.cc - setup Fiasco as the Karma guest OS
 */

#include "l4_fiasco.h"

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>

#include <l4/util/util.h>
#include <l4/sys/cache.h>

#include <stdlib.h>

#include "mipsregs.h"
#include "loader.h"
#include "vmm.h"
#include "util/debug.h"

L4_fiasco::L4_fiasco(L4_mem *l4_mem) : _guest_entrypoint(0), _memory(l4_mem)
{
    setup_os();
}

L4_fiasco::~L4_fiasco() {}

void L4_fiasco::setup_os()
{
    setup_memory();
}

void L4_fiasco::backtrace(l4_umword_t ip, l4_umword_t stackframe)
{
    karma_log(INFO, "Backtrace:\n%x, %x\n", (unsigned int)ip, (unsigned int)stackframe);
    // KYMA TODO backtrace not implemented
    //karma_log(INFO, "End of backtrace\n");
}

void L4_fiasco::setup_memory()
{
    l4_addr_t os_image_addr;
    l4_addr_t entrypoint;
    l4_addr_t kernelhigh;
    const l4_addr_t offset_into_guestspace = 0;
    int err = 0;

    // get OS image ready
    os_image_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if(!os_image_ds.is_valid())
    {
        karma_log(ERROR, "[fiasco] Fatal error, could not allocate Fiasco image. Terminating.\n");
        exit(1);
    }

    L4::Cap<L4Re::Namespace> rom = L4Re::Env::env()->get_cap<L4Re::Namespace>("rom");

    if (!rom)
    {
        karma_log(ERROR, "[fiasco] Fatal error, could not find rom. Terminating.\n");
        exit(1);
    }


    // get dataspace capablities of OS image
    if(rom->query(params.kernel_name, os_image_ds))
    {
        karma_log(ERROR, "[fiasco] Fatal error, could not find Fiasco kernel image \"rom/%s\". Terminating.\n", params.kernel_name);
        exit(1);
    }

    // attach OS image
    if((err = L4Re::Env::env()->rm()->attach((void**)&os_image_addr,
                                      os_image_ds->size(),
                                      L4Re::Rm::Read_only |
                                      L4Re::Rm::Search_addr,
                                      os_image_ds)))
    {
        karma_log(ERROR, "[fiasco] Fatal error, could not attach Fiasco image dataspace (error = %d). Terminating.\n", err);
        exit(1);
    }

    karma_log(INFO, "[fiasco] Loading guest image...\n");

    /* load guest image into guest memory */
    if((err = loader_elf_load((l4_uint8_t*)os_image_addr, os_image_ds->size(),
                             (l4_uint8_t*)_memory->base_ptr(), offset_into_guestspace,
                             &entrypoint, &kernelhigh)))
    {
        karma_log(ERROR, "[fiasco] Error parsing Fiasco ELF image %s (error = %d, %s). Terminating.\n",
                params.kernel_name, err, l4sys_errtostr(err));
        exit(1);
    }

    _guest_entrypoint = entrypoint;

    karma_log(INFO, "[fiasco] the guest OS entrypoint is %#08lx\n", entrypoint);

    // make changes to instruction stream effective
    // call syncICache using root address
    syncICache(_memory->base_ptr(), _memory->get_mem_size());
}

l4_addr_t L4_fiasco::get_guest_entrypoint()
{
    return _guest_entrypoint;
}

void L4_fiasco::guest_os_init(void *context)
{
    l4_vm_vz_vmcb_t * vmm = reinterpret_cast<l4_vm_vz_vmcb_t*>(context);

    karma_log(INFO, "Starting VM Guest kernel_name=%s @ 0x%lx\n",
        params.kernel_name, vmm->vm_guest_entrypoint);

    vmm_regs(vmm).ip = vmm->vm_guest_entrypoint;

}

// Select Fiasco as the OS
L4_os * L4_os::create_os(L4_mem *l4_mem)
{
    return new L4_fiasco(l4_mem);
}
