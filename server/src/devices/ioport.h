/*
 * x86_ioports.hpp - Implements ioport pass-through.
 *
 * (c) 2012 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING for details.
 */

#ifndef X86_IOPORTS_
#define X86_IOPORTS_

#include "device_interface.h"

#include <string.h>
#include <l4/sys/types.h>

class Ioport: public device::IDevice
{
public:
    virtual void hypercall(HypercallPayload &);
    void init();
private:
    // keep track of the ioports that we are allowed to access
    l4_uint32_t ioport_map[(1<<16)/sizeof(l4_uint32_t)];
    inline void init_map(void)
    {
        memset(ioport_map, 0, sizeof(ioport_map));
    }

    // from Linux bitops.h
    inline void set_bit(int nr, volatile l4_uint32_t *addr)
    {
        asm volatile("bts %1,%0" : "+m" (*(volatile l4_uint32_t *)addr) : "Ir" (nr) : "memory");
    }

    // from Linux bitops.h
    inline int test_bit(int nr, const void *addr)
    {
        const l4_uint32_t *p = (const l4_uint32_t *)addr;
        return ((1UL << (nr & 31)) & (p[nr >> 5])) != 0;
    }


    bool ioport_allowed(unsigned int port);
};


/** 
 *  Obsolete: keeping this for reference
 *  This code can was used to analyse ioport intercepts on svm.
 */
#if 0
inline void handle_svm_ioio(l4_uint32_t info, l4_uint32_t rip)
{
    karma_log(INFO, "IO Port Access: port='%xh' type='%s' str='%d'\n", 
            (info & 0xffff0000)>>16,
            (info & 0x1) ? "IN" : "OUT",
            (info & (1<<2)) ? 1: 0);
    if(info & (1 << 9))
        karma_log(INFO, "64bit address\n");
    if(info & (1 << 8))
        karma_log(INFO, "32bit address\n");
    if(info & (1 << 7))
        karma_log(INFO, "16bit address\n");
    if(info & (1 << 6))
        karma_log(INFO, "32bit operand\n");
    if(info & (1 << 5))
        karma_log(INFO, "16bit operand\n");
    if(info & (1 << 4))
        karma_log(INFO, "8bit operand\n");
    if(info & (1 << 3))
        karma_log(INFO, "Repeated port access\n");
    if(info & 1)
        karma_log(INFO, "io port write\n");
    else
        karma_log(INFO, "io port read\n");
    karma_log(INFO, "Instruction: rip=0x%X\n", rip);
}
#endif
#endif // X86_IOPORTS_
