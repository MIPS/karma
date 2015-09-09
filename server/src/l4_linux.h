/*
 * l4_linux.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include <l4/re/dataspace>
#include <l4/sys/vm>
#include <l4/rtc/rtc.h>

#include <string.h>

#include "l4_exceptions.h"
#include "l4_os.h"
#include "l4_mem.h"
#include "params.h"
#include "vmcs.h"

class L4_linux:public L4_os {
public:
    L4_linux(unsigned int mem_size,
             L4_mem *l4_mem,
             L4::Cap<L4::Task> vm_cap);
    ~L4_linux() {}
    void setup_os();
    void backtrace(l4_umword_t eip, l4_umword_t ebp);
    l4_addr_t get_ramdisk_addr(); //returns the address to which the ramdisk dataspace is attached
    l4_addr_t get_guest_ramdisk_addr();
    l4_addr_t get_ramdisk_size(); 
    void dump();
private:
    void setup_memory();
    void setup_gdt(l4_addr_t _gdt);
    unsigned counter;
    unsigned int _mem_size;
    l4_addr_t _lx_ramdisk_addr; //the ramdisk datapsace allocated by us, is attached to this address
    l4_addr_t _lx_ramdisk_size;
    l4_addr_t guest_ramdisk_addr; // the start of the ramdisk as what the guest sees
    L4_mem *_memory;
    L4::Cap<L4Re::Dataspace> lx_image_ds, lx_ramdisk_ds, lx_ramdisk_write_ds;
    L4::Cap<L4::Task> _vm_cap;
};

