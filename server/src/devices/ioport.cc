/*
 * ioport.cc - Implements io port passthrough as pseudo-device (using
 *             hypercalls)
 * 
 * (c) 2012 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "ioport.h"
#include "devices_list.h"

#include <l4/io/io.h>

#include "../util/debug.h"
#include "../l4_vm.h"

void Ioport::init()
{
    init_map();    
}

void Ioport::hypercall(HypercallPayload & payload)
{
    int port = (int)payload.reg(0);
    karma_log(TRACE, "io port = 0x%lx\n", payload.reg(0));
    bool io_port = ioport_allowed(port);
    void *addr;
    switch(payload.address())
    {
        case karma_ioport_inb:
            if(!io_port)
            {
                payload.reg(1) = -1;
                break;
            }
            asm volatile("inb %w1, %b0" : "=a" (payload.reg(1)) : "Nd"(port));
            break;
        case karma_ioport_inw:
            if(!io_port)
            {
                payload.reg(1) = -1;
                break;
            }
            asm volatile("inw %w1, %w0" : "=a" (payload.reg(1)) : "Nd"(port));
            break;
        case karma_ioport_inl:
            if(!io_port)
            {
                payload.reg(1) = -1;
                break;
            }
            asm volatile("inl %w1, %0" : "=a" (payload.reg(1)) : "Nd"(port));
            break;
        case karma_ioport_outb:
            if(!io_port)
                break;
            asm volatile("outb %b0, %w1" : : "a" (payload.reg(1)) , "Nd"(port));
            break;
        case karma_ioport_outw:
            if(!io_port)
                break;
            asm volatile("outw %w0, %w1" : : "a" (payload.reg(1)) , "Nd"(port));
            break;
        case karma_ioport_outl:
            if(!io_port)
                break;
            asm volatile("outl %0, %w1" : : "a" (payload.reg(1)) , "Nd"(port));
            break;
        case karma_ioport_insb:
            addr = (void*)(GET_VM.mem().base_ptr() + payload.reg(1));
            karma_log(TRACE, "addr=%p\n", addr);
            if(!io_port)
                break;
            asm volatile("rep; insb" : "+D" (addr), "+c"(payload.reg(2)): "d"(port));
            break;
        case karma_ioport_insw:
            addr = (void*)(GET_VM.mem().base_ptr() + payload.reg(1));
            karma_log(TRACE, "addr=%p\n", addr);
            if(!io_port)
                break;
            asm volatile("rep; insw" : "+D" (addr), "+c"(payload.reg(2)): "d"(port));
            break;
        case karma_ioport_insl:
            addr = (void*)(GET_VM.mem().base_ptr() + payload.reg(1));
            karma_log(TRACE, "addr=%p\n", addr);
            if(!io_port)
                break;
            asm volatile("rep; insl" : "+D" (addr), "+c"(payload.reg(2)): "d"(port));
            break;
        case karma_ioport_outsb:
            addr = (void*)(GET_VM.mem().base_ptr() + payload.reg(1));
            karma_log(TRACE, "addr=%p\n", addr);
            if(!io_port)
                break;
            asm volatile("rep; outsb" : "+S" (addr), "+c"(payload.reg(2)) : "d"(port));
            break;
        case karma_ioport_outsw:
            addr = (void*)(GET_VM.mem().base_ptr() + payload.reg(1));
            karma_log(TRACE, "addr=%p\n", addr);
            if(!io_port)
                break;
            asm volatile("rep; outsw" : "+S" (addr), "+c"(payload.reg(2)) : "d"(port));
            break;
        case karma_ioport_outsl:
            addr = (void*)(GET_VM.mem().base_ptr() + payload.reg(1));
            karma_log(TRACE, "addr=%p\n", addr);
            if(!io_port)
                break;
            asm volatile("rep; outsl" : "+S" (addr), "+c"(payload.reg(2)) : "d"(port));
        break;
        default:
            break;
    }
}

bool Ioport::ioport_allowed(unsigned int port)
{
    // range check
    if(port > 65556)
        return false;
    // Is the io port already mapped?
    if(test_bit(port, ioport_map))
        return true;
    // Io port not mapped.
    // Try to map it.
    if(l4io_request_ioport(port, 1) < 0)
    {
        karma_log(WARN, "Could not map io port 0x%x. Maybe you forgot to configure it in *.vbus and *.io files?\n", port);
        return false;
    }
    set_bit(port, ioport_map);
    return true;
}

