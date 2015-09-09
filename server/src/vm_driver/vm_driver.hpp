/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * vm_driver.hpp - TODO enter description
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
 * vm_driver.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef VM_DRIVER_HPP_
#define VM_DRIVER_HPP_
#include "../l4_vm_driver.h"
#include "../util/statistics.hpp"
#include "../util/local_statistics.hpp"
#include "../devices/device_call.hpp"

class Vm_driver : public L4_vm_driver {
private:

    VMCX::type * _vmcx;
    GP_REGS _gpregs;
    INTERRUPT_HANDLING _int_handling;

    unsigned _id;
    L4::Cap<L4::Vm> _vm_cap;

public:
    Vm_driver(unsigned id) : _id(id), _vm_cap(GET_VM.vm_cap()) {
        _vmcx = VMCX::create(id);
    }

    void post_vmexit() {
        _gpregs.fromVcpuRegs(MY_VCPU_STATE_PTR->r);
    }

    void pre_vmresume() {
        _gpregs.toVcpuRegs(MY_VCPU_STATE_PTR->r);
    }

    void vmresume() {
        current_vcpu().resumeUser(_vm_cap);
    }

    void handle_interrupt(int irq) {
        _int_handling.handle(*_vmcx, irq);

        GET_LSTATS_ITEM(handle_interrupt).addMeasurement(irq, current_vcpu().tsc_ticks_since_last_entry());
    }

    void set_ip_and_sp(l4_umword_t ip, l4_umword_t sp) {
        VMCX::set_ip_and_sp(*_vmcx, ip, sp);
    }

    void handle_hypercall() {
        HypercallPayload hp;

        _gpregs.toHypercallPayload(hp);

        if (hp.valid()) {

            DO_HYPERCALL(hp);
            _gpregs.fromHypercallPayload(hp);

            // update any acknowledged interrupts
            _int_handling.update(*_vmcx);

        } else {
            karma_log(ERROR, "Invalid hypercall issued.\n");
            hp.dump();
            GET_VM.os().backtrace(VMCX::current_ip(*_vmcx), _gpregs.stackFrame());
        }
    }

    void handle_vmexit() {
        post_vmexit();

        L4_vm_driver::Exitcode code;

        code = INSTR_INTERCEPT::intercept(*_vmcx);

        switch (code) {
        case L4_vm_driver::Vmmcall:
            handle_hypercall();
            break;
        case L4_vm_driver::Hlt:
            //karma_log(INFO, "HLT\n");
            while (VMCX::hlt_condition(*_vmcx)) {
                current_vcpu().super_halt();
            }
            break;
        case L4_vm_driver::Unhandled:
            karma_log(TRACE, "INTERCEPT\n");
            karma_log(ERROR, "Unhandled vmexit @0x%lx! (exitcode : 0x%lx)\n", (unsigned long)VMCX::current_ip(*_vmcx), VMCX::exit_code(*_vmcx));
            throw L4_PANIC_EXCEPTION;
            break;
        case L4_vm_driver::Handled:
            break;
        case L4_vm_driver::Error:
            _gpregs.dump();
        default:
            karma_log(ERROR, "Fatal error. exitcode=%d\n", code);
            GET_VM.os().backtrace(VMCX::current_ip(*_vmcx), _gpregs.stackFrame());
            throw L4_PANIC_EXCEPTION;
            break;
        }

        pre_vmresume();

        GET_LSTATS_ITEM(vmexit_reason).addMeasurement(VMCX::exit_code(*_vmcx), current_vcpu().tsc_ticks_since_last_entry());

    }

    void dump_state() {
    }

    void dump() {
    }

    l4_umword_t current_ip(){
        return VMCX::current_ip(*_vmcx);
    }
};

#endif
