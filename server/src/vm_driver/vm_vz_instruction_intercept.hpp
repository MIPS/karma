/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#ifndef VM_VZ_INSTRUCTION_INTERCEPT_HPP_
#define VM_VZ_INSTRUCTION_INTERCEPT_HPP_

#include "vmm.h"

class INSTR_INTERCEPT {
public:

    inline static L4_vm_driver::Exitcode intercept(l4_vm_vz_vmcb_t & vmcb) {

        L4_vm_driver::Exitcode ret = L4_vm_driver::Handled;

        enum emulation_result er = vz_trap_handler(&vmcb);

        switch (er) {
        case EMULATE_DONE:
            ret = L4_vm_driver::Handled;
            break;
        case EMULATE_FAIL:
            ret = L4_vm_driver::Unhandled;
            break;
        case EMULATE_WAIT:
            ret = L4_vm_driver::Hlt;
            break;
        case EMULATE_HYPCALL:
            ret = L4_vm_driver::Vmmcall;
            break;
        case EMULATE_FATAL:
            ret = L4_vm_driver::Error;
            break;
        case EMULATE_EXIT:
            ret = L4_vm_driver::Hlt;
            break;
        default:
            ret = L4_vm_driver::Unhandled;
            break;
        }

        return ret;
    }

};
#endif /* VM_VZ_INSTRUCTION_INTERCEPT_HPP_ */

