/*
 * vm_vmx_instruction_intercept.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Matthias Lange <mlange@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * vm_vmx_instruction_intercept.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef VM_VMX_INSTRUCTION_INTERCEPT_HPP_
#define VM_VMX_INSTRUCTION_INTERCEPT_HPP_

#include "../devices/devices_list.h"

template<>
class INSTR_INTERCEPT<arch::x86, arch::x86::vmx> {
private:
    typedef GP_REGS<arch::x86, arch::x86::vmx> gpregs_t;

    inline static void advance_rIP(Vmcs & vmcs) {
        vmcs.vmwrite<VMX_GUEST_RIP> (vmcs.vmread<VMX_GUEST_RIP> ()
                + vmcs.vmread<VMX_EXIT_INSTRUCTION_LENGTH>());
    }

    inline static L4_vm_driver::Exitcode handle_vmcall(Vmcs & vmcs, gpregs_t & gpregs){
        HypercallPayload hp;
        gpregs.toHypercallPayload(hp);
        if(hp.deviceID() == KARMA_DEVICE_ID(arch)){
            switch(hp.address()){
            case karma_arch_df_wrmsr:
                if (hp.reg(0) == 0x174) {
                    karma_log(TRACE, "Set sysenter_cs to %lx\n", hp.reg(1));
                } else if (hp.reg(0) == 0x175) {
                    karma_log(TRACE, "Set sysenter_esp to %lx\n", hp.reg(1));
                    vmcs.vmwrite<VMX_GUEST_SYSENTER_ESP> (hp.reg(1));
                } else if (hp.reg(0) == 0x176) {
                    karma_log(TRACE, "Set sysenter_eip to %lx\n", hp.reg(1));
                    vmcs.vmwrite<VMX_GUEST_SYSENTER_EIP> (hp.reg(1));
                } else {
                    karma_log(ERROR, "Unknown msr (%lx).\n", hp.reg(1));
                    karma_log(ERROR, "the rip is %lx\n", vmcs.vmread<VMX_GUEST_RIP> ());
                }
                break;
            }
            advance_rIP(vmcs);
            return L4_vm_driver::Handled;
        } else {
            return L4_vm_driver::Vmmcall;
        }
    }

public:

    inline static L4_vm_driver::Exitcode intercept(Vmcs & vmcs, gpregs_t & gpregs) {
        l4_uint32_t exit_reason;

        L4_vm_driver::Exitcode ret = L4_vm_driver::Handled;

        exit_reason = vmcs.vmread<VMX_EXIT_REASON> ();

        switch (exit_reason) {
         case 2: // triple fault
            karma_log(ERROR, "Triple fault encountered\n");
            ret = L4_vm_driver::Error;
            break;
        case 9: // task switch
            advance_rIP(vmcs);
            break;
        case 10: // cpuid
            karma_log(TRACE, "cpuid");
            handle_cpuid(gpregs);
//            intercepts[4]++;
            advance_rIP(vmcs);
            break;
        case 12: // hlt
        case 36: // mwait
//            intercepts[6]++;
            advance_rIP(vmcs);
            ret = L4_vm_driver::Hlt;
            break;
        case 18: // vmmcall
            ret = handle_vmcall(vmcs, gpregs);
            break;
        case 20: // vmlaunch
            advance_rIP(vmcs);
            ret = L4_vm_driver::Handled;
            break;
		case 30:
		  if(vmcs.vmread<VMX_EXIT_QUALIFICATION>() & 0x8) // in
			gpregs.ax = -1;
		  advance_rIP(vmcs);
		  ret = L4_vm_driver::Handled;
		  break;
        case 31: // rdmsr
        case 32: // wrmsr
            karma_log(ERROR, "rdmsr/wrmsr encountered\n");
            ret = L4_vm_driver::Error;
            break;
        case 40: // pause
//            intercepts[5]++;
            advance_rIP(vmcs);
            break;
        case 48: // ept violation
            karma_log(ERROR, "EPT violation or misconfiguration\n");
            ret = L4_vm_driver::Error;
            break;
        case 54: // wbinvd
            advance_rIP(vmcs);
            break;
        default: // found an intercept that we do not currently handle
            ret = L4_vm_driver::Unhandled;
            break;
        }

        return ret;

    }

    inline static void finalizeHyperCall(Vmcs & vmcs){
        advance_rIP(vmcs);
    }

};
#endif /* VM_VMX_INSTRUCTION_INTERCEPT_HPP_ */
