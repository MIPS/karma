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

#include <stdlib.h>

#include "guest_linux.hpp"

#include "util/debug.h"

L4_linux::L4_linux(unsigned int mem_size,
                   L4_mem *l4_mem,
                   L4::Cap<L4::Task> vm_cap)
  : _mem_size(mem_size), _memory(l4_mem), _vm_cap(vm_cap)
{
    setup_os();
    counter=0;
}

void L4_linux::setup_gdt(l4_addr_t _gdt)
{
    // _gdt is supposed to point into guest memory (but in hosts address space)
    
    // According to Documentation/x86/boot.txt
    // We need a gdt with two descriptors: __BOOT_CS(0x10) and __BOOT_DS(0x18)
    
    // Template segment descriptor for flat memory model
    // (segment base = 0, limit = 4Gb):
    //       0xffff, 0xcf9b00
    
    l4_addr_t *gdt = (l4_addr_t*)_gdt;
    
    // __BOOT_CD(0x10) segment descriptor (rx):
    *(gdt + 4) = 0xffff;
    *(gdt + 5) = 0xcf9b00;
    
    // __BOOT_DS(0x18) segment descriptor (rw):
    *(gdt + 6) = 0xffff;
    *(gdt + 7) = 0xcf9300;
};

void L4_linux::setup_os()
{
    struct setup_header *lx_setup_header;
    setup_memory();

    //screen_info->orig_video_mode = 0
    *(l4_uint8_t*)(_memory->base_ptr() + 0x6) = 0;
    //screen_info->orig_video_lines = 0
    *(l4_uint8_t*)(_memory->base_ptr() + 0xe) = 0;
    //screen_info->orig_video_cols = 0
    *(l4_uint8_t*)(_memory->base_ptr() + 0x7) = 0;

    *(l4_uint8_t*)(_memory->base_ptr() + 0xf) = 0; //invalid VGA type

    // now setup setup header
    // hint: ramdisk_addr and ramdisk_size were set in setup_memory already!
    lx_setup_header = (struct setup_header*)(_memory->base_ptr() + BPO_HDR);

    // we are a "unknown" bootloader
    lx_setup_header->type_of_loader = 0xff;

    // setup e820 entry
    _memory->make_e820_map();

    // setup command line
	_memory->copyin((l4_addr_t)params.kernel_opts, 0x10000, strlen(params.kernel_opts)); 
    lx_setup_header->cmd_line_ptr = 0x10000;
    lx_setup_header->version = 0x207;

    // do not reload segments!
    //lx_setup_header->loadflags |= KEEP_SEGMENTS;
    // we were loaded to 1Mb, which resides in high memory
    lx_setup_header->loadflags |= LOADED_HIGH;
    // I do not provide a valid heap_end ptr
    //lx_setup_header->loadflags |= CAN_USE_HEAP;
    lx_setup_header->loadflags |= QUIET_FLAG;
    
    //lx_setup_header->heap_end_ptr = 0x200000;
    lx_setup_header->hardware_subarch = 0;

    // We deliberatly choose to have idt and gdt in the uppermost 2 pages,
    // but below IO memory. Linux does not touch this memory before it sets its
    // own gdt and idt.

    // idt remains empty
    _idt = _memory->get_mem_size() - 0x2000;

    // now setup gdt
    _gdt = _memory->get_mem_size() - 0x1000;
    setup_gdt(_gdt + _memory->base_ptr());
}

void L4_linux::backtrace(l4_umword_t eip, l4_umword_t ebp)
{
    karma_log(INFO, "Backtrace Linux:\n%x\n", (unsigned int)eip);
    int i = 0;
    l4_uint32_t _ebp = ebp, ra = 1;
    if(_ebp < 0xc0000000) {
        karma_log(ERROR, "Backtrace aborted. Can only backtrace linux kernel.\n");
        return;
    }
    try {
    while (i<10 && ebp > 0 && ra!=0) {
        if(ebp < 0xc0000000)
            return;
        ra = *(l4_uint32_t*)(_memory->base_ptr()+(_ebp-0xc0000000 + 4));
        printf("%x\n", (unsigned int)ra);
        _ebp = *(l4_uint32_t*)(_memory->base_ptr() + (_ebp-0xc0000000));
        i++;
        }
    }catch(...){} // nullpointers...
    karma_log(INFO, "End of backtrace\n");
}

void L4_linux::setup_memory()
{
    struct setup_header *lx_setup_header;
    l4_addr_t lx_image_addr, lx_ramdisk_addr;
    int err = 0;

    // allocate enough writeable memory for linux
    _memory->init(_mem_size, _vm_cap); // 1Gb
    
    // get bzImage and ramdisk ready
    lx_image_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    lx_ramdisk_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    lx_ramdisk_write_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if(!lx_image_ds.is_valid()
        || !lx_ramdisk_ds.is_valid() 
        || !lx_ramdisk_write_ds.is_valid())
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

	if(rom->query(params.ramdisk_name, lx_ramdisk_ds))
    {
        karma_log(ERROR, "[linux] Fatal error, could not find Linux ramdisk \"rom/%s\". Terminating.\n", params.ramdisk_name);
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

    if((err = L4Re::Env::env()->rm()->attach((void**)&lx_ramdisk_addr,
                                      lx_ramdisk_ds->size(),
                                      L4Re::Rm::Read_only |
                                      L4Re::Rm::Search_addr,
                                      lx_ramdisk_ds)))
    {
        karma_log(ERROR, "[linux] Fatal error, could not attach ramdisk dataspace (error = %d). Terminating.\n", err);
        exit(1);
    }

    _lx_ramdisk_size = lx_ramdisk_ds->size();

    // allocate space for the ramdisk_write capability
    if((err = L4Re::Env::env()->mem_alloc()->alloc((unsigned long)_lx_ramdisk_size, lx_ramdisk_write_ds,
						 L4Re::Mem_alloc::Continuous|L4Re::Mem_alloc::Pinned|L4Re::Mem_alloc::Super_pages)))
    {
        karma_log(ERROR, "[linux] Error allocating enough memory for r/w ramdisk (error = %d). Terminating.\n", err);
        exit(1);
    }
    
    if((err = L4Re::Env::env()->rm()->attach(&_lx_ramdisk_addr, _lx_ramdisk_size,
					   L4Re::Rm::Search_addr,
					   lx_ramdisk_write_ds,
					   0,
					   L4_SUPERPAGESHIFT)))
    {
        karma_log(ERROR, "[linux] Error attaching r/w ramdisk dataspace (error = %d). Terminating.\n", err);
        exit(1);
    }

    memcpy((void*)_lx_ramdisk_addr, (void*)lx_ramdisk_addr, _lx_ramdisk_size);
   
    karma_log(INFO, "[linux] the ramdisk size is %lx\n", lx_ramdisk_ds->size());

    // find the linux boot header
    lx_setup_header = (struct setup_header*)(lx_image_addr + BPO_HDR);
    // check if this really is a bzImage
    if(memcmp(&lx_setup_header->header, "HdrS", 4) != 0)
    {
        karma_log(ERROR, "[linux] Error reading boot header. This does not look like a bzImage. Terminating.\n");
        exit(1);
    }

    // it is. Copy the header to page zero:
    _memory->copyin((l4_addr_t)lx_image_addr, 0, (lx_setup_header->setup_sects)*512);

    // now find the start of linux binary image
    lx_image_addr += (lx_setup_header->setup_sects+1)*512;

    lx_setup_header = (struct setup_header*)(_memory->base_ptr()+BPO_HDR);

    // copy image to address 1Mb in vm address space
    _memory->copyin((l4_addr_t)lx_image_addr,
                0x100000,
                lx_image_ds->size() - ((lx_setup_header->setup_sects+1)*512));

    // map ramdisk to the top of vm memory
    lx_ramdisk_addr = _memory->mapontop(lx_ramdisk_addr, lx_ramdisk_ds->size(), 0);
    guest_ramdisk_addr = lx_ramdisk_addr;
    
    karma_log(INFO, "[linux] the guest lx ramdisk is %lx \n", lx_ramdisk_addr);

    // setup these fields here
    lx_setup_header->ramdisk_image = lx_ramdisk_addr;
    lx_setup_header->ramdisk_size = lx_ramdisk_ds->size();
}

l4_addr_t L4_linux::get_ramdisk_addr()
{
    return _lx_ramdisk_addr;
}

l4_addr_t L4_linux::get_guest_ramdisk_addr()
{
    return guest_ramdisk_addr;
}

l4_addr_t L4_linux::get_ramdisk_size()
{
    return _lx_ramdisk_size;
}

void L4_linux::dump() {
    printf("L4_linux: %d\n", counter);
    counter = 0;
}

