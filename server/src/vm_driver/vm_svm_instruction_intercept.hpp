/*
 * vm_svm_instruction_intercept.hpp - TODO enter description
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
 * vm_svm_instruction_intercept.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef VM_SVM_INSTRUCTION_INTERCEPT_HPP_
#define VM_SVM_INSTRUCTION_INTERCEPT_HPP_

#include "../arch_list.h"
#include "vm_instruction_intercept.hpp"
#include "x86_cpuid.hpp"
#include "../devices/ioport.h"
#include "../devices/devices_list.h"

#include "../util/debug.h"

template <>
class INSTR_INTERCEPT<arch::x86, arch::x86::svm>{
public:

    typedef GP_REGS<arch::x86, arch::x86::svm> gpregs_t;

    inline static L4_vm_driver::Exitcode intercept(l4_vm_svm_vmcb_t & vmcb, gpregs_t & gpregs){
        l4_uint32_t info;
        HypercallPayload hp;
        switch(vmcb.control_area.exitcode){
        case 0x81: // VMMCALL
            gpregs.toHypercallPayload(hp);
            if(hp.deviceID() == KARMA_DEVICE_ID(arch)){
                switch(hp.address()){
                case karma_arch_df_wrmsr:
                    if (hp.reg(0) == 0x174) {
                         karma_log(TRACE, "Set sysenter_cs to %lx\n", hp.reg(1));
                         vmcb.state_save_area.sysenter_cs = hp.reg(1);
                     } else if (hp.reg(0) == 0x175) {
                         karma_log(TRACE, "Set sysenter_esp to %lx\n", hp.reg(1));
                         vmcb.state_save_area.sysenter_esp = hp.reg(1);
                     } else if (hp.reg(0) == 0x176) {
                         karma_log(TRACE, "Set sysenter_eip to %lx\n", hp.reg(1));
                         vmcb.state_save_area.sysenter_eip = hp.reg(1);
                     } else {
                         karma_log(ERROR, "Unknown msr (%lx).\n", hp.reg(0));
                         //return L4_vm_driver::Error;
                     }
                     finalizeHyperCall(vmcb);
                }
                return L4_vm_driver::Handled;
            } else {
                return L4_vm_driver::Vmmcall;
            }
        case 0x46:
            karma_log(ERROR, "invalid opcode \n");
            return L4_vm_driver::Error;

        // device not available
        case 0x47:
            karma_log(ERROR, "Encountered a device not available exception.\n");
            return L4_vm_driver::Error;

        // double fault
        case 0x48:
            karma_log(ERROR, "Encountered a double fault.\n");
            karma_log(ERROR, "exitinfo1 0x%llx  exitinfo2 0x%llx  exitintinfo  0x%llx\n",
                vmcb.control_area.exitinfo1, vmcb.control_area.exitinfo2, vmcb.control_area.exitintinfo);
            return L4_vm_driver::Error;

        case 0x4d:
            karma_log(ERROR, "Encountered a general protection fault.\n");
            return L4_vm_driver::Error;

        // smc mode intercept
        case 0x62:
            karma_log(TRACE, "smc mode intercept");
            // intercepts[3]++;
            //return L4_vm_driver::Handled;

        case 0x6a: // idt write
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x400;
            //return L4_vm_driver::Handled;

        case 0x6b: // gdtr write
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x800;
            //return L4_vm_driver::Handled;

        case 0x6c: // ldtr write
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x1000;
            //return L4_vm_driver::Handled;

        case 0x6d: // tr write
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x2000;
            //return L4_vm_driver::Handled;

        case 0x6e: // rdtsc
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x4000;
            //return L4_vm_driver::Handled;

        case 0x70: // pushf
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x10000;
            //return L4_vm_driver::Handled;

        case 0x71: // popf
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x20000;
            return L4_vm_driver::Handled;

        case 0x72: // cpuid
            karma_log(TRACE, "cpuid");
            handle_cpuid(gpregs);
            vmcb.state_save_area.rip += 2;
            // intercepts[4]++;
            return L4_vm_driver::Handled;

        case 0x75: // intN
            //vmcb.control_area.intercept_instruction0 =
            //    vmcb.control_area.intercept_instruction0 &~ 0x200000;
            //printf("software interrupt encountered \n");
            return L4_vm_driver::Handled;

        case 0x77: // pause
            // intercepts[5]++;
            // pause: f3 90 nop: 90 so skipping one byte is perfect
            vmcb.state_save_area.rip += 1;
            return L4_vm_driver::Handled;

        case 0x78: // hlt
            // intercepts[6]++;
            //dump_call_chain();
            //throw L4_PANIC_EXCEPTION;
            vmcb.state_save_area.rip++;
            return L4_vm_driver::Hlt;

        case 0x7b: // IOIO
            info = vmcb.control_area.exitinfo1;
            //handle_svm_ioio(info, vmcb.state_save_area.rip);
            if (info & 0x1) {
                karma_log(INFO, "Write to IO port ignored.\n");
                vmcb.state_save_area.rip++;
                return L4_vm_driver::Handled;
            } else {
                karma_log(INFO, "Read IO port. Returning -1\n");
                gpregs.ax = -1;
                vmcb.state_save_area.rip++;
                return L4_vm_driver::Handled;
            }
            return L4_vm_driver::Error;

        case 0x7c: // rdmsr/rwmsr
            karma_log(TRACE, "rdmsr/rwmsr encountered.\n");
            return L4_vm_driver::Error;

        case 0x7f: // shutdown
            karma_log(INFO, "shutdown encountered\n");
            return L4_vm_driver::Error;

        case 0x80: // VMRUN
            // FIXME BUG vmrun is 3 bytes long
            vmcb.state_save_area.rip++;
            return L4_vm_driver::Handled;

        case 0x82: // VMLOAD
            // FIXME BUG vmload is 3 bytes long
            vmcb.state_save_area.rip++;
            return L4_vm_driver::Handled;

        case 0x83: // VMSAVE
            // FIXME BUG vmsave is 3 bytes long
            vmcb.state_save_area.rip++;
            return L4_vm_driver::Handled;

        case 0x84: // STGI
            // FIXME BUG stgi is 3 bytes long
            vmcb.state_save_area.rip++;
            return L4_vm_driver::Handled;

        case 0x85: // CLGI
            // FIXME BUG clgi is 3 bytes long
            vmcb.state_save_area.rip++;
            return L4_vm_driver::Handled;

        // write back caches (ignoring)
        case 0x89: // wbinvd
            // FIXME BUG wbinvd is 2 bytes long
            vmcb.state_save_area.rip++;
            return L4_vm_driver::Handled;

        // case 0x8b: // mwait FIXME why is this not handled
        // pagefault for nested paging
        case 0x400:
            karma_log(ERROR, "Encountered a page fault at guest physical address '%lx'\n"
                "Error code = '%lx'\n",
                    (unsigned long) vmcb.control_area.exitinfo2,
                    (unsigned long) vmcb.control_area.exitinfo1);
            return L4_vm_driver::Error;
        }

        // found an intercept that we do not currently handle
        return L4_vm_driver::Unhandled;
    }

    inline static void finalizeHyperCall(l4_vm_svm_vmcb_t & vmcb){
        vmcb.state_save_area.rip += 3;
    }
};


#endif /* VM_SVM_INSTRUCTION_INTERCEPT_HPP_ */
