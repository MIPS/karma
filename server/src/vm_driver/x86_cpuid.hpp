/*
 * x86_cpuid.hpp - We intercept cpuid to replace the cpu vendorid.
 *                 This has only one purpose: effects for technical demonstrations
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * x86_cpuid.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef X86_CPUID_HPP_
#define X86_CPUID_HPP_

#include "gpregs.hpp"
#include <l4/util/cpu.h>

inline void handle_cpuid(GP_REGS<arch::x86, arch::x86::svm> & gpregs) {
    unsigned int rax = 0;
    rax = gpregs.ax;
    l4util_cpu_cpuid(rax, &(gpregs.ax), &(gpregs.bx), &(gpregs.cx), &(gpregs.dx));

    if (rax == 0x0) {
        // WickedFiasco
        gpregs.bx = 0x6B636957;
        gpregs.dx = 0x69466465;
        gpregs.cx = 0x6f637361;
    }
}

inline void handle_cpuid(GP_REGS<arch::x86, arch::x86::vmx> & gpregs){
    unsigned int rax = 0;
    rax = gpregs.ax;
    l4util_cpu_cpuid(rax, &(gpregs.ax), &(gpregs.bx), &(gpregs.cx), &(gpregs.dx));

    if (rax == 0x0) {
        // FiascoWicked (INTEL)
        gpregs.bx = 0x73616946;
        gpregs.dx = 0x69576f63;
        gpregs.cx = 0x64656b63;
    }
}

#endif /* X86_CPUID_HPP_ */
