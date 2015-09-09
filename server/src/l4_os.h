/*
 * l4_os.h - TODO enter description
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

class L4_os
{
protected:
    // _idt and _gdt are given as guest physical addresses!
    l4_addr_t _gdt, _idt;
public:
    l4_addr_t get_gdt() { return _gdt; }
    l4_addr_t get_idt() { return _idt; }
    virtual void setup_os() =0;
    virtual void backtrace(l4_umword_t addr, l4_umword_t val) =0;
    virtual l4_addr_t get_ramdisk_addr() = 0; //returns the address to which the ramdisk dataspace is attached
    virtual l4_addr_t get_guest_ramdisk_addr() = 0;
    virtual l4_addr_t get_ramdisk_size() = 0;
};

