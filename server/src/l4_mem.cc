/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * l4_mem.cc - Handle guest memory.
 *
 *  Guest physical memory layout:
 *  Guest phys.         Host virt.
 *  0                   _base
 *  _size               _base + _size
 *  io memory is allocated on top of _base + _size
 * 
 * (c) 2011 Jan Nordholz <jnordholz@sec.t-labs.tu-berlin.de>
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#include "l4_mem.h"
#include "l4_exceptions.h"
#include "l4_vm.h"

#include <l4/re/mem_alloc>
#include <l4/re/env>
#include <l4/re/rm>
#include <l4/re/util/cap_alloc>
#include <l4/util/util.h>
#include <l4/re/dataspace>

#include <l4/sys/vm>
#include <l4/sys/consts.h>
#include <l4/sys/kdebug.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#include "util/debug.h"

L4_mem::L4_mem() : _size(0), _ontop_size(0), _ontop_source(0)
{
    for(int i=0; i<L4_MEM_IO_ENTRIES; i++)
    {
        iomap[i].size = 0;
        iomap[i].addr = 0;
        iomap[i].source = 0;
    }
}

L4_mem::~L4_mem() {}

void L4_mem::init(unsigned int size, L4::Cap<L4::Task> vm_cap)
{
    int r;
    void *base = 0;

    _size = l4_trunc_page(size);
    _vm_cap = vm_cap;

    if(!_vm_cap.is_valid())
        throw L4_CAPABILITY_EXCEPTION;

    _ds_cap = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if(!_ds_cap.is_valid())
    {
        printf("L4_mem: error allocating capability\n");
        return;
    }

    if((r = L4Re::Env::env()->mem_alloc()->alloc((unsigned long)_size, _ds_cap,
        L4Re::Mem_alloc::Continuous|L4Re::Mem_alloc::Pinned|L4Re::Mem_alloc::Super_pages)))
    {
        printf("L4_mem: Error allocating enough memory for vm\n");
        throw L4_EXCEPTION("allocate vm memory");
    }
    karma_log(DEBUG, "[mem] allocated memory\n");

    if((r = L4Re::Env::env()->rm()->attach(&base, _size,
                                            L4Re::Rm::Search_addr,
                                            _ds_cap,
                                            0,
                                            L4_SUPERPAGESHIFT)))
    {
        printf("L4_mem: Error attaching vm memory ds\n");
        throw L4_EXCEPTION("attach vm memory");
    }

    karma_log(DEBUG, "[mem] attached memory address %p, size=%#x\n", base, _size);
    assert(((l4_umword_t)base & (L4_PAGESIZE - 1)) == 0); // make sure base is page aligned
    _base = base;

    _mem_top = map(0, (l4_addr_t)_base, _size, 1);

    _mem_top += L4_MEM_IO_SIZE;
    karma_log(DEBUG, "[mem] init done.\n");

    if (LOG_LEVEL(DEBUG)) {
        l4_addr_t vmimage_phy_addr;
        l4_size_t continuous_region_size;
        _ds_cap->phys(0, vmimage_phy_addr, continuous_region_size);
        karma_log(DEBUG, "[mem] vm image mapped to physical address %p, contiguous size=%#x\n",
                (void*)vmimage_phy_addr, continuous_region_size);
    }
}

bool L4_mem::copyin(l4_addr_t source, l4_addr_t start, unsigned int size)
{
    //TODO: check constraints
    memcpy((void*)((l4_addr_t)_base + start), (void*)source, size);
    return true;
}

l4_addr_t L4_mem::mapontop(l4_addr_t source, unsigned int size, int write)
{
    karma_log(DEBUG, "mapontop: map(dest='%x', source='%x', size='%x', write='%x')\n",
         (unsigned int)_mem_top, (unsigned int)source, size, write);

    map((l4_addr_t)_mem_top, source, size, write);
    _ontop_size = size;
    _ontop_source = source;
    return _mem_top;
}

unsigned int L4_mem::get_mem_size(void)
{
  return _size;
}

l4_addr_t L4_mem::base_ptr()
{
    return (l4_addr_t)_base;
}

l4_addr_t L4_mem::top_ptr()
{
    return (l4_addr_t)_mem_top;
}

l4_addr_t L4_mem::map(l4_addr_t dest, l4_addr_t source, unsigned int size, int write, int io)
{
    l4_msgtag_t msg;
    unsigned int mapped_size = 0;
    int pageshift = 0;
    int rights;

    if(write && !io)
        l4_touch_rw((void*)source, size);
    else
        l4_touch_ro((void*)source, size);
    if(write || io)
        rights = L4_FPAGE_RW;
    else
        rights = L4_FPAGE_RO;
    karma_log(TRACE, "touched memory!\n");

    while(mapped_size < size)
    {
        /* map superpages, if possible */
        if(((size - mapped_size) >= L4_SUPERPAGESIZE) &&
                !(source & (L4_SUPERPAGESIZE - 1)) &&
                !(dest & (L4_SUPERPAGESIZE - 1)))
            pageshift = L4_SUPERPAGESHIFT;
        else
            pageshift = L4_PAGESHIFT;

        msg = _vm_cap->map(L4Re::Env::env()->task(),
                           l4_fpage(source, pageshift, rights),
                           dest);

        if(l4_error(msg))
        {
            printf("Error mapping main memory.\n");
            exit(1);
        }

        source +=   (1UL << pageshift);
        dest +=     (1UL << pageshift);
        mapped_size += (1UL << pageshift);
    }

    return dest+=(1UL << pageshift);
}

bool L4_mem::register_iomem(l4_addr_t *addr, unsigned int size)
{
    unsigned int avail = L4_MEM_IO_SIZE;
    int i;
    for(i=0; i<L4_MEM_IO_ENTRIES; i++)
        avail -= iomap[i].size;
    if(avail < size)
        return false;
    for(i=0; i<L4_MEM_IO_ENTRIES; i++)
        if(iomap[i].size == 0)
            break;
    if (i == L4_MEM_IO_ENTRIES)
        return false;
    iomap[i].size = size;
    if(i==0)
        // _size is the size of the guest physical memory starting
        // at 0
        // so we can start mapping io memory at this position.
        iomap[i].addr = _size;
    else
        // addresses need to be page aligned...
        iomap[i].addr = l4_round_page(iomap[i-1].addr + iomap[i-1].size);
    *addr = iomap[i].addr;
    return true;
}

l4_addr_t L4_mem::map_iomem(l4_addr_t addr, l4_addr_t source, int write)
{
    karma_log(DEBUG, "map_iomem(addr = %lx, source = %lx, write = %d)\n",
         (unsigned long)addr, (unsigned long)source, write);

    if (source != (source & L4_PAGEMASK)) {
        karma_log(ERROR, "map_iomem source is not page aligned.\n");
        return 0;
    }

    for(int i=0; i<L4_MEM_IO_ENTRIES; i++){
        if(iomap[i].addr == addr){
            iomap[i].source = source;
            return map(iomap[i].addr, source, iomap[i].size, write);
        }
    }
    return 0;
}

l4_addr_t L4_mem::map_iomem_hard(l4_addr_t addr, l4_addr_t source, unsigned size, int write)
{
    struct iomem iomem_;
    iomem_.addr = addr;
    iomem_.source = source;
    iomem_.size = size;
    _iomap_hard.push_back(iomem_);
    return map(addr, source, size, write, 1);
}

void L4_mem::unmap_all()
{
    l4_msgtag_t msg;
    msg = GET_VM.vm_cap()->unmap(l4_fpage(0, 63, L4_FPAGE_RWX), L4_FP_ALL_SPACES);
    if (l4_error(msg)) {
        printf("L4_mem::reset_cr3 => Unmapping error.\n");
        exit(1);
    }
}

/**
 * \brief Translate a guest physical address to the local address space.
 *
 * \param  addr       Guest physical address.
 * \retval local_addr Address in the current address-space.
 *
 * \return 0 on success, 1 on error
 */
int L4_mem::phys_to_local(const l4_addr_t addr, l4_addr_t * local_addr)
{
    int i;

    /* address points to whatever was mapped "on top" */
    if (addr >= top_ptr() && addr < (top_ptr() + _ontop_size))
    {
        *local_addr = (_ontop_source + addr - top_ptr());
        return 0;
    }

    /* address may point into regular memory */
    if (addr < _size)
    {
        *local_addr = (base_ptr() + addr);
       return 0;
    }

    /* address may point into IO memory */
    for (i=0; i<L4_MEM_IO_ENTRIES; i++)
    {
        if (addr >= iomap[i].addr && addr < (iomap[i].addr + iomap[i].size))
        {
            *local_addr = (addr - iomap[i].addr + iomap[i].source);
            return 0;
        }
    }

    std::list<struct iomem>::iterator iter = _iomap_hard.begin();
    for(; iter != _iomap_hard.end(); iter++)
    {
        if(addr >= iter->addr && addr < (iter->addr + iter->size))
        {
            *local_addr = (addr - iter->addr + iter->source);
            return 0;
        }
    }
    /* I don't know this address */
    return 1;
}

/**
 * \brief Translate a guest physical address to the host physical address.
 *
 * \param  guest_phys Guest physical address.
 * \retval host_phys  Host physical address.
 * \retval phys_size  Size of largest physically contiguous region in the
 *                    data space (in bytes).
 *
 * \return 0 on success, 1 on error
 */
int L4_mem::phys_to_host(const l4_addr_t guest_phys, l4_addr_t * host_phys, l4_size_t * phys_size)
{
    long ret = _ds_cap->phys(guest_phys, *host_phys, *phys_size);
    if (ret == 0)
        return 0;

    /* I don't know this address */
    return 1;
}
