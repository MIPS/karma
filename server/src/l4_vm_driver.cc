/*
 * l4_vm_driver.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "l4_vm_driver.h"

#include "vm_driver/gpregs.hpp"
#include "vm_driver/vm_vz_instruction_intercept.hpp"
#include "vm_driver/vm_vz_interrupt.hpp"
#include "vm_driver/vmcx_vz.hpp"
#include "vm_driver/vm_driver.hpp"

L4_vm_driver * L4_vm_driver::create(
            unsigned id)
{
    return new Vm_driver(id);
}

L4_vm_driver::~L4_vm_driver() {}
