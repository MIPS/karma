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
//#define DEBUG_LEVEL INFO

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

#include "devices/devices_list.h"
#include "util/debug.h"

L4_mem::L4_mem()
{
    _size = 0;
    for(int i=0; i<L4_MEM_IO_ENTRIES; i++)
    {
        iomap[i].size = 0;
        iomap[i].addr = 0;
        iomap[i].source = 0;
    }
    _ontop_size = 0;
    _ontop_source = 0;
}

void L4_mem::init(unsigned int size, L4::Cap<L4::Task> vm_cap)
{
    int r;
    void *base = 0;

    if(!vm_cap.is_valid())
        throw L4_CAPABILITY_EXCEPTION;

    _ds_cap = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if(!_ds_cap.is_valid())
    {
        printf("L4_mem: error allocating capability\n");
        return;
    }

    size = l4_trunc_page(size);
    if((r = L4Re::Env::env()->mem_alloc()->alloc((unsigned long)size, _ds_cap,
        L4Re::Mem_alloc::Continuous|L4Re::Mem_alloc::Pinned|L4Re::Mem_alloc::Super_pages)))
    {
        printf("L4_mem: Error allocating enough memory for vm\n");
        throw L4_EXCEPTION("allocate vm memory");
    }
    karma_log(DEBUG, "L4_mem: allocated memory\n");

    if((r = L4Re::Env::env()->rm()->attach(&base, size,
                                            L4Re::Rm::Search_addr,
                                            _ds_cap,
                                            0,
                                            L4_SUPERPAGESHIFT)))
    {
        printf("L4_mem: Error attaching vm memory ds\n");
        throw L4_EXCEPTION("attach vm memory");
    }

    karma_log(DEBUG, "[mem] attached memory address %p, size=%d\n", base, size);
    assert(((l4_umword_t)base & 0xfff) == 0); // make sure base is page aligned
    _base = base;

    _vm_cap = vm_cap;

    _mem_top = map(0, (l4_addr_t)_base, size, 1);

    _mem_top += L4_MEM_IO_SIZE;
    _size = size;
    karma_log(DEBUG, "L4_mem: init done.\n");
    l4_addr_t linuximage_phy_addr;
    l4_size_t continuous_region_size;  
    _ds_cap->phys(0, linuximage_phy_addr, continuous_region_size);
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
        if(((size - mapped_size) >= (1 << L4_SUPERPAGESHIFT)) &&
        		!(source & L4_SUPERPAGEMASK) && !(dest & L4_SUPERPAGEMASK))
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
        iomap[i].addr = (iomap[i-1].addr + iomap[i-1].size + 0xfff) & ~0xfff;
    *addr = iomap[i].addr;
    return true;
}

l4_addr_t L4_mem::map_iomem(l4_addr_t addr, l4_addr_t source, int write)
{
    karma_log(DEBUG, "map_iomem(addr = %lx, source = %lx, write = %d)\n",
         (unsigned long)addr, (unsigned long)source, write);
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

void L4_mem::make_e820_map()
{
    struct e820entry *e820_memmap = (struct e820entry*)(base_ptr() + BPO_E820_MAP);
    struct e820entry *e820_iomap = e820_memmap + 1;

    /* first entry: all available physical memory */
    e820_memmap->addr = 0UL;
    e820_memmap->size = _size;
    e820_memmap->type = E820_RAM;

    /* second entry: memory mappable I/O space */
    e820_iomap->addr = _size;
    e820_iomap->size = L4_MEM_IO_SIZE;
    e820_iomap->type = E820_RESERVED;

    *(l4_uint8_t*)(base_ptr() + BPO_E820_ENTRIES) = 2;
}

void L4_mem::hypercall(HypercallPayload & payload)
{
    l4_addr_t tmp = 0, ptr = 0;
    l4_size_t size = 0;
    l4_umword_t val = payload.reg(0);
    switch(payload.address())
    {
        case karma_mem_df_guest_phys_to_host_phys:
            ptr = (l4_addr_t)(base_ptr()+val);
            if(!ptr || !*(int*)ptr)
                return;
            _ds_cap->phys(*(l4_addr_t*)ptr, tmp, size);
            karma_log(DEBUG, "[mem] karma_mem_df_guest_phys_to_host_phys val=%p, guest phys=%p, host_phys=%p size=%d\n",
                    (void*)val, (void*)*(l4_addr_t*)ptr, (void*)tmp, size);
            *(l4_addr_t*)ptr = tmp;
            break;
        case karma_mem_df_dma_base:
            ptr = (l4_addr_t)(base_ptr()+val);
            _ds_cap->phys((l4_addr_t)0, tmp, size);
            karma_log(DEBUG, "[mem] karma_mem_df_dma_base val=%p, guest phys=%p, host_phys=%p size=%d\n",
                    (void*)val, (void*)*(l4_addr_t*)ptr, (void*)tmp, size);
            *(l4_addr_t*)ptr = tmp;
            break;
        default:
            break;
    }
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

int L4_mem::phys_to_karma(const l4_addr_t addr, l4_addr_t & real_addr)
{
    int i;

    /* address points to whatever was mapped "on top" */
    if (addr >= top_ptr() && addr < (top_ptr() + _ontop_size))
    {
        real_addr = (_ontop_source + addr - top_ptr());
        //printf("%s: address is part of the ramdisk real_addr = %x\n", __func__, *real_addr);
        return 0;
    }

    /* address may point into regular memory */
    if (addr < _size)
    {
        real_addr = (base_ptr() + addr);
       return 0;
    }

    /* address may point into IO memory */
    for (i=0; i<L4_MEM_IO_ENTRIES; i++)
    {
        if (addr >= iomap[i].addr && addr < (iomap[i].addr + iomap[i].size))
        {
            real_addr = (addr - iomap[i].addr + iomap[i].source);
            return 0;
        }
    }

    std::list<struct iomem>::iterator iter = _iomap_hard.begin();
    for(; iter != _iomap_hard.end(); iter++)
    {
        if(addr >= iter->addr && addr < (iter->addr + iter->size))
        {
            real_addr = (addr - iter->addr + iter->source);
            return 0;
        }
    }
    /* I don't know this address */
    return 1;
}


