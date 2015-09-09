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

#include "l4_linux.h"
#include <l4/re/c/util/cap_alloc.h>
#include "params.h"
#include <malloc.h>
#include "l4_vm.h"
#include "l4_cpu.h"
#include "guest_linux.hpp"

#include "vm_driver/vm_svm_instruction_intercept.hpp"
#include "vm_driver/vm_vmx_instruction_intercept.hpp"
#include "vm_driver/gpregs.hpp"
#include "vm_driver/vmcx_svm.hpp"
#include "vm_driver/vmcx_vmx.hpp"
#include "vm_driver/vm_svm_interrupt.hpp"
#include "vm_driver/vm_vmx_interrupt.hpp"
#include "vm_driver/tlb_handling_npt.hpp"
#include "vm_driver/tlb_handling_vtlb2.hpp"
#include "vm_driver/tlb_handling_vtlb.hpp"
#include "vm_driver/vm_driver.hpp"
#include "util/debug.h"

L4_vm_driver * L4_vm_driver::create(
            unsigned id)
{
    switch (cpu_option){
    case AMD_cpu:
        if(params.vtlb){
            return new Tvm_driver<arch::x86, arch::x86::svm, VTLB>(id, guest::Linux());
        } else {
            return new Tvm_driver<arch::x86, arch::x86::svm, NPT>(id, guest::Linux());
        }
    case Intel_cpu:
        return new Tvm_driver<arch::x86, arch::x86::vmx, VTLB>(id, guest::Linux());
    default:
        // just make sure this variant gets built.
        if (params.vtlb)
            return new Tvm_driver<arch::x86, arch::x86::svm, VTLB2>(id, guest::Linux());
    }
    return NULL;
}
