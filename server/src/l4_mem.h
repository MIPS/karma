/*
 * l4_mem.h - TODO enter description
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
#pragma once

#include "devices/device_interface.h"
#include "params.h"
#include <list>
#include <l4/re/env>
#include <l4/sys/task>
#include <l4/sys/types.h>
#include <l4/re/dataspace>
#include <l4/sys/err.h>
#include <l4/sys/compiler.h>
#include <l4/sys/kip.h>
#include <l4/sigma0/sigma0.h>

#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3
#define E820_NVS	4
#define E820_UNUSABLE	5
#define BPO_E820_ENTRIES 0x1e8
#define BPO_E820_MAP 0x2d0

#define L4_MEM_IO_ENTRIES 20
#define L4_MEM_IO_SIZE 25*1024*1024
#define MAX_PCI_DEVICES 20

#define UNKNOWN_DEV  0
#define PCI_DEV      1
#define FB_DEV       2
#define EV_DEV       3
#define NET_DEV_SEND 4
#define NET_DEV_RECV 5

struct e820entry {
    l4_uint64_t addr; // start of segment
    l4_uint64_t size; // size
    l4_uint32_t type; // type
} __attribute__((packed));


struct iomem {
    l4_addr_t addr;
    unsigned int size;
    l4_addr_t source;
};

class L4_mem : public device::IDevice {
private:
    void *_base;
    unsigned int _size;

    L4::Cap<L4::Task> _vm_cap;

    L4::Cap<L4Re::Dataspace> _ds_cap;
    l4_addr_t _mem_top;
    l4_umword_t _ontop_size;
    l4_umword_t _ontop_source;

    struct iomem iomap[L4_MEM_IO_ENTRIES];
    std::list<struct iomem> _iomap_hard;
public:
    L4_mem(void);
    ~L4_mem(void) {}
    void init(unsigned int size, L4::Cap<L4::Task> vm_cap);

    virtual void hypercall(HypercallPayload &);

    unsigned int get_mem_size(void);
    l4_addr_t base_ptr(void);
    l4_addr_t top_ptr(void);
    bool copyin(l4_addr_t source, l4_addr_t start, unsigned int size);
    l4_addr_t mapontop(l4_addr_t source, unsigned int size, int write);
    bool register_iomem(l4_addr_t *addr, unsigned int size);
    l4_addr_t map_iomem(l4_addr_t addr, l4_addr_t source, int write);
    l4_addr_t map_iomem_hard(l4_addr_t addr, l4_addr_t source, unsigned size, int write);
    void make_e820_map();
    // l4_device

    int phys_to_karma(const l4_addr_t addr, l4_addr_t & real_addr);
    l4_addr_t map(l4_addr_t dest, l4_addr_t source, unsigned int size, int write, int io=0);

    void unmap_all();

    void dump();
private:
    void write(l4_umword_t addr, l4_umword_t val);
    l4_umword_t read(l4_umword_t addr);
};

