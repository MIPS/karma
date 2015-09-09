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
#include "../devices/device_call.hpp"

template <typename ARCH, typename EXTENSION, template<typename _ARCH, typename _EXTENSION> class TLB_HANDLING>
class Tvm_driver : public L4_vm_driver{
private:
    typedef VMCX<ARCH, EXTENSION> vmcx_traits;
//    typedef  vmcx_g_traits;
    typedef GP_REGS<ARCH, EXTENSION> gpregs_t;
    typedef INTERRUPT_HANDLING<ARCH, EXTENSION> int_handling_t;
    typedef TLB_HANDLING<ARCH, EXTENSION> tlb_handling_t;
    typedef INSTR_INTERCEPT<ARCH, EXTENSION> instruction_handling_t;

    tlb_handling_t _tlb_handling;

    int intercepts[8];


    int_handling_t _int_handling;

    unsigned _id;

    typename vmcx_traits::type * _vmcx;
    gpregs_t _gpregs;

    L4::Cap<L4::Vm> _vm_cap;


public:
    template<typename GUEST>
    Tvm_driver(
            unsigned id, GUEST)
    : _id(id), _vm_cap(GET_VM.vm_cap())
    {
        _vmcx = vmcx_traits::create();
        guest::VMCX<GUEST, ARCH, EXTENSION>::init(_vmcx);
        guest::GP_REGS<GUEST, ARCH, EXTENSION>::init(_gpregs);
        for(int i=0; i<8; i++)
            intercepts[i] = 0;

        _tlb_handling.init(*_vmcx);
    }
private:

public:
    L4_vm_driver::Exitcode post_vmexit() {
        L4_vm_driver::Exitcode code = L4_vm_driver::Error;
        l4_vcpu_state_t * vcpu_state = MY_VCPU_STATE_PTR;

        // copy vcpu_regs to gpregs
        _gpregs.fromVcpuRegs(vcpu_state->r, *_vmcx);

        // interrupt
        return code;
    }

    void pre_vmresume() {
        l4_vcpu_state_t * vcpu_state = MY_VCPU_STATE_PTR;

        _tlb_handling.sanitize_vmcx(*_vmcx);
        // copy gpregs to vcpu state
        _gpregs.toVcpuRegs(vcpu_state->r, *_vmcx);
    }

    void vmresume() {
        // if vtlb is used and the guest has enabled paging resume does not return
        _tlb_handling.resume();

        current_vcpu().resumeUser(_vm_cap);
    }

    void handle_interrupt(L4_apic & apic, int irq) {
        _int_handling.handle(apic, *_vmcx, irq);

        GET_LSTATS_ITEM(handle_interrupt).addMeasurement(irq, current_vcpu().tsc_ticks_since_last_entry());
        //vmresume();
    }
    void set_ip_and_sp(l4_umword_t ip, l4_umword_t sp){
        vmcx_traits::set_ip_and_sp(*_vmcx, ip, sp);
    }

    void handle_vmexit(L4_apic & apic) {
        post_vmexit();
        L4_vm_driver::Exitcode code = L4_vm_driver::Unhandled;
        code = _int_handling.vmexit_hook(apic, *_vmcx);
        if(code == L4_vm_driver::Unhandled)
            // TODO move to svm specific
            code = instruction_handling_t::intercept(*_vmcx, _gpregs);

        _tlb_handling.intercept(*_vmcx, _gpregs, code);

        HypercallPayload hp;
        switch (code) {
        case L4_vm_driver::Vmmcall:
            if(_gpregs.cx & 0x80000000){ // marker for new hypercall scheme
                _gpregs.toHypercallPayload(hp);
                DO_HYPERCALL(hp);
                _gpregs.fromHypercallPayload(hp);
            } else {
                karma_log(ERROR, "legacy vmmcall issued. Not supported anymore.\n");
                GET_VM.os().backtrace(vmcx_traits::current_ip(*_vmcx), _gpregs.bp);
            }
            instruction_handling_t::finalizeHyperCall(*_vmcx);
            break;
        case L4_vm_driver::Hlt:
            karma_log(TRACE, "HLT\n");
            while (vmcx_traits::hlt_condition(*_vmcx)) {
                current_vcpu().super_halt();
            }
            break;
        case L4_vm_driver::Unhandled:
            karma_log(TRACE, "INTERCEPT\n");
            karma_log(ERROR, "Unhandled vmexit @0x%lx! (exitcode : 0x%lx)\n", (unsigned long)vmcx_traits::current_ip(*_vmcx), vmcx_traits::exit_code(*_vmcx));
            enter_kdebug();
            break;
        case L4_vm_driver::Handled:
            break;
        case L4_vm_driver::Error:
            _gpregs.dump();
        default:
            karma_log(ERROR, "Fatal error. exitcode=%d\n", code);
            GET_VM.os().backtrace(vmcx_traits::current_ip(*_vmcx), _gpregs.bp);
            throw L4_PANIC_EXCEPTION;
            break;
        }

        pre_vmresume();

        GET_LSTATS_ITEM(vmexit_reason).addMeasurement(vmcx_traits::exit_code(*_vmcx), current_vcpu().tsc_ticks_since_last_entry());

    }
private:

    void stop() {
    }



public:
    void dump_call_chain() {

    }

public:
    void dump_state() {
    }

    void dump() {
        printf(
                "Intercepts: IOPORT: %8d MSR %8d PGFAULT %8d SMC %8d CPUID %8d PAUSE %8d HLT %8d\n",
                intercepts[0], intercepts[1], intercepts[2], intercepts[3],
                intercepts[4], intercepts[5], intercepts[6]);
        for (int i = 0; i < 8; i++)
            intercepts[i] = 0;
    }

    l4_umword_t current_ip(){
        return vmcx_traits::current_ip(*_vmcx);
    }
};

#endif
