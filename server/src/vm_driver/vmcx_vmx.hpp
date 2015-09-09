/*
 * vmcx_vmx.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * vmcx_vmx.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef VMCX_VMX_HPP_
#define VMCX_VMX_HPP_

template <>
class VMCX<arch::x86, arch::x86::vmx>{
public:
    typedef Vmcs type;

    static Vmcs * create() { return new Vmcs; }

    inline static bool hlt_condition(Vmcs & vmcs){
        // hlt condition is: no event to be injected
        // and no interrupt window to be intercepted
        return (!(vmcs.vmread<VMX_ENTRY_INTERRUPT_INFO>() & 0x80000000)
                   && !(vmcs.vmread<VMX_PRIMARY_EXEC_CTRL>() & (0x4)));
    }
    inline static void set_ip_and_sp(Vmcs & vmcs, const l4_umword_t ip, const l4_umword_t sp) {
        vmcs.vmwrite<VMX_GUEST_RIP>(ip);
        vmcs.vmwrite<VMX_GUEST_RSP>(sp);
    }

    inline static l4_fpage_t fpage(Vmcs & vmcs){
        return l4_fpage((unsigned long) vmcs.vmcs(), 12, 0);
    }

    inline static l4_umword_t current_ip(Vmcs & vmcs) {
        return vmcs.vmread<VMX_GUEST_RIP>();
    }
    inline static l4_umword_t exit_code(Vmcs & vmcs) {
        return vmcs.vmread<VMX_EXIT_REASON>();
    }

};

#endif /* VMCX_VMX_HPP_ */
