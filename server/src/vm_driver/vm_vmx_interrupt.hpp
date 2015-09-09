/*
 * vm_vmx_interrupt.hpp - TODO enter description
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
 * vm_vmx_interrupt.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef VM_VMX_INTERRUPT_HPP_
#define VM_VMX_INTERRUPT_HPP_

#include "vm_interrupt.hpp"

template <>
class INTERRUPT_HANDLING<arch::x86, arch::x86::vmx>{
public:

    inline bool can_inject_interrupt(Vmcs & vmcs){
        return (((vmcs.vmread<VMX_GUEST_RFLAGS> ()) & (1 << 9)) == (0x200)
                && !(vmcs.vmread<VMX_GUEST_INTERRUPTIBILITY_STATE>() & 0xf));
    }


    inline void inject_interrupt(L4_apic & apic, Vmcs & vmcs) {
        // if interrupts can be injected and there was no other event (e.g. PF) injected
        if (can_inject_interrupt(vmcs) && !(vmcs.vmread<VMX_ENTRY_INTERRUPT_INFO>() & 0x80000000)) {
            int intr;
            if (((intr = apic.signal_vcpu())) >= 0) {
//              interrupts[intr]++;
                l4_uint32_t eve_inj = 0;

                eve_inj = intr;
                eve_inj |= (1 << 31);
                vmcs.vmwrite<VMX_ENTRY_INTERRUPT_INFO> (eve_inj);
            }
        }
        l4_uint32_t pec = vmcs.vmread<VMX_PRIMARY_EXEC_CTRL>();

        // if more interrupts are pending intercetp interrupt window
        if (apic.nr_pending() == 0) {
            // do not intercept
            vmcs.vmwrite<VMX_PRIMARY_EXEC_CTRL>(pec & ~(0x4));
        } else {
            // intercept
            vmcs.vmwrite<VMX_PRIMARY_EXEC_CTRL>(pec | 0x4);
        }
    }

    inline void handle(L4_apic & apic, Vmcs & vmcs, int irq){
        if (irq != VCPU_IPI_LABEL) {
            apic.trigger_irq_vcpu(irq + 0x30);
        }
        inject_interrupt(apic, vmcs);
    }

    // not needed by vmx. Supplying dummy implementation.
    L4_vm_driver::Exitcode vmexit_hook(L4_apic & apic, Vmcs & vmcs){
        if(vmcs.vmread<VMX_EXIT_REASON>() == 0x7){ // interrupt window intercept
            inject_interrupt(apic, vmcs);
            return L4_vm_driver::Handled;
        } else
            return L4_vm_driver::Unhandled;
    }
};

#endif /* VM_VMX_INTERRUPT_HPP_ */
