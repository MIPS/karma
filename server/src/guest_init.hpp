/*
 * guest_init.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * guest_init.hpp
 *
 *  Created on: 26.10.2010
 */

#ifndef GUEST_INIT_HPP_
#define GUEST_INIT_HPP_

#include <l4/sys/vm.h>
#include <l4/sys/vcpu.h>
#include "vm_driver/gpregs.hpp"
#include "arch_list.h"
#include "guest_list.h"

namespace guest{

template <typename GUEST, typename ARCH, typename EXTENSION>
class GP_REGS{

};

template <typename GUEST, typename EXTENSION>
class GP_REGS<GUEST, arch::x86, EXTENSION>
{
public:
    static void init(::GP_REGS<arch::x86, EXTENSION> & gpregs)
    {
        gpregs.ax = 0;
        gpregs.dx = 0;
        gpregs.cx = 0;
        gpregs.bx = 0;
        gpregs.bp = 0;
        gpregs.si = 0;
        gpregs.di = 0;
//        gpregs.dr0 = 0;
//        gpregs.dr1 = 0;
//        gpregs.dr2 = 0;
//        gpregs.dr3 = 0;
    }
};

template <typename GUEST, typename ARCH, typename EXTENSION>
class VMCX
{

};

}


#endif /* GUEST_INIT_HPP_ */
