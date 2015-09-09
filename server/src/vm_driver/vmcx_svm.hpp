/*
 * vmcx_svm.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * vmcx_svm.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef VMCX_SVM_HPP_
#define VMCX_SVM_HPP_

#include <l4/sys/vm.h>
#include "../l4_cpu.h"

template <>
class VMCX<arch::x86, arch::x86::svm>{
public:
    typedef l4_vm_svm_vmcb_t type;

    inline static l4_vm_svm_vmcb_t * create() { return (l4_vm_svm_vmcb_t *) current_vcpu().getExtState(); }

    inline static bool hlt_condition(l4_vm_svm_vmcb_t & vmcb){
        // hlt condition is: no event to inject AND no VINTR to inject
        return !(vmcb.control_area.eventinj & 0x80000000) && !(vmcb.control_area.interrupt_ctl & (1 << 8));
    }
    inline static void set_ip_and_sp(l4_vm_svm_vmcb_t & vmcb, const l4_umword_t ip, const l4_umword_t sp) {
        vmcb.state_save_area.rip = ip;
        vmcb.state_save_area.rsp = sp;
    }

    inline static l4_fpage_t fpage(l4_vm_svm_vmcb_t & vmcb){
        return l4_fpage((unsigned long) &vmcb, 12, 0);
    }

    inline static l4_umword_t current_ip(l4_vm_svm_vmcb_t & vmcb) {
        return vmcb.state_save_area.rip;
    }
    inline static l4_umword_t exit_code(l4_vm_svm_vmcb_t & vmcb) {
        return vmcb.control_area.exitcode;
    }
};

#endif /* VMCX_SVM_HPP_ */
