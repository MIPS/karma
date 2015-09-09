/*
 * vm_svm_interrupt.hpp - TODO enter description
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
 * vm_svm_interrupt.hpp
 *
 *  Created on: 29.10.2010
 */

#ifndef VM_SVM_INTERRUPT_HPP_
#define VM_SVM_INTERRUPT_HPP_

#include "vm_interrupt.hpp"
#include "../util/statistics.hpp"

template <>
class INTERRUPT_HANDLING<arch::x86, arch::x86::svm>{
    int _latest_injected_vintr;
public:
    INTERRUPT_HANDLING() : _latest_injected_vintr(-1){}

    inline bool can_inject_interrupt(l4_vm_svm_vmcb_t & vmcb){
        return (vmcb.state_save_area.rflags & (0x200)
                && !(vmcb.control_area.interrupt_shadow & 0x1));
    }

    inline void inject_event(int & intr, l4_vm_svm_vmcb_t & vmcb) {
        if (intr < 0)
            return;
        unsigned int event = 0;

        if (!can_inject_interrupt(vmcb)) {
            printf("FATAL! About to inject event but VM not ready.\n");
            return;
        }
        event |= (unsigned) intr;
        event |= (1 << 31);
        vmcb.control_area.eventinj = event;
    }

    void inject_interrupt(int & intr, l4_vm_svm_vmcb_t & vmcb) {
        l4_uint64_t field = 0;
        field = (unsigned) intr;
        field <<= 32;
        field |= (1 << 8);
        field |= (0xf << 16);
        vmcb.control_area.interrupt_ctl = field;
        intr = -1;
    }

    inline void handle(L4_apic & apic, l4_vm_svm_vmcb_t & vmcb, int irq){
        // this funktion is closely related to L4_cpu::intercept_VINTR()
        // read both to understand how they work together


        if (_latest_injected_vintr >= 0 && !(vmcb.control_area.interrupt_ctl
                & 0x100)) {
            //        printf("HANDLER INJECTION VINTR %d has been taken (vmexit=%u).\n", _latest_injected_vintr, _vmexit_count);
            GET_STATS_ITEM(interrupt_latency).decpending();
            _latest_injected_vintr = -1;
        }



        GET_STATS_ITEM(interrupt_latency).incpending();


        // if we get an ipi the corresponding vector was already triggered
        // see trigger_ipi()
        if (irq != VCPU_IPI_LABEL) {
            apic.trigger_irq_vcpu(irq + 0x30);
        }

        int pending = apic.nr_pending();

//        // if intercepting VINTR (see AMD Manual Vol. 2 Appendix C)
//        if (vmcb.control_area.exitcode == 0x64) {
//            // if we are currently intercepting a VINTR we leave the
//            // event injection to the main thread. We just make
//            // sure intercepting VINTR is set accordingly (see below)
//        } else {
//            // else we can just inject the next interrupt as an event
//            // if that is possible right now.
//            // Notion: checking for pending interrupts here could be
//            // omitted be cause we have just triggered one (see above)
//            // and because as we are an event handler we could not have
//            // bin interrupted in a way that would have cleared any
//            // pending interrupts.
//            if (pending && can_inject_interrupt(vmcb)) {
//                int intr = apic.signal_vcpu();
//                --pending;
//                // if an event is pending check whether the new event might
//                // have a higher priority and inject the higher one
//                if ((vmcb.control_area.eventinj & 0x80000000)) {
//                    int intr_pending = vmcb.control_area.eventinj & 0xff;
//                    // if the pending interrupt is higher or an other event type is pending
//                    // e.g. page fault we stand back
//                    if ((intr_pending >= intr) || (vmcb.control_area.eventinj & 0x700)) {
//                        apic.trigger_irq_vcpu(intr);
//                        // push one back so one more pending
//                        ++pending;
//                        intr = -1;
//                    } else {
//                        apic.trigger_irq_vcpu(intr_pending);
//                        // push one back so one more pending
//                        ++pending;
//                        //                    --interrupts[intr_pending];
//                    }
//                } else {
//                    GET_STATS_ITEM(interrupt_latency).decpending();
//                }
//                if (intr >= 0) {
//                    //                ++interrupts[intr];
//                    //            printf("HANDLER INJECTION Inject %d as event (vmexit=%u, pending=%d).\n", intr, _vmexit_count, pending);
//                    inject_event(intr, vmcb);
//                }
//            }
//            //        else if(_vmcb->control_area.eventinj & 0x80000000){
//            //            printf("can not inject event. Event pending.\n");
//            //        }
//        }
        if (pending) {
            if (!(vmcb.control_area.interrupt_ctl & 0x100)) {
                // if there threre are still interrupts pending and
                // no VINTR is pending we can inject one
                // VINTR may already be pending because:
                // 1. we where called multiple times
                // 2. the main thread handled VINTR intercept and has
                //    injected a new VINTR
                // 3. the last pending VINTR was just not jet taken
                // 4. the main thread is about to handle VINTR and the flag
                //    was not yet cleared (in this case the main thread will
                //    take care of VINTR injection)
                int intr = apic.signal_vcpu();
                //            ++interrupts[intr];
                //            printf("HANDLER INJECTION Inject %d as VINTR (vmexit=%u, pending=%d).\n", intr, _vmexit_count, pending);
                _latest_injected_vintr = intr;
                inject_interrupt(intr, vmcb);
                --pending;
            }
        }
        //    else if(_vmcb->control_area.interrupt_ctl & 0x100){
        //        if(!(_vmcb->control_area.intercept_instruction0 & 0x10)){
        //            // this pending VINTR was not to be intercepted,
        //            // but as there are no more interrupts pending
        //            // it was taken care of during event injection.
        //            // j
        //        }
        //        _vmcb->control_area.interrupt_ctl = 0;
        //    }

        // if at this point threre are still interrupts pending
        // must also be a pending VINTR. Now we decide whether
        // to intercept it or not.

        if (pending) {
            // if there are still interrupts pending
            if (!(vmcb.control_area.intercept_instruction0 & 0x10)) {
                // and we don not yet intercept VINTR make sure
                // VINTR gets intercepted
                vmcb.control_area.intercept_instruction0 |= 0x10; // intercept vintr
                // and pull back the last injected interrupt for it will not be taken
                // but will be handled by intercept_VINTR()
                int intr = (vmcb.control_area.interrupt_ctl >> 32) & 0xff;
                //            --interrupts[intr];
                // and put it back in the queue
                //printf("HANDLER INJECTION Pulling %d back (vmexit=%u, pending=%d).\n", intr, _vmexit_count, pending);
                apic.trigger_irq_vcpu(intr);
                _latest_injected_vintr = -1;
            }
        } else {
            // there is no need to intercept VINTR as there are no more interrupts pending
            // if a VINTR is pending it will be taken by the vm.
            vmcb.control_area.intercept_instruction0 &= ~0x10; // do not intercept vintr
        }

    }

    L4_vm_driver::Exitcode vmexit_hook(L4_apic & apic, l4_vm_svm_vmcb_t & vmcb){
        if(vmcb.control_area.exitcode == 0x64){
            intercept_VINTR(apic, vmcb);
            return L4_vm_driver::Handled;
        }
        return L4_vm_driver::Unhandled;
    }

    inline void intercept_VINTR(L4_apic & apic, l4_vm_svm_vmcb_t & vmcb) {
        // this disables irq for the scope of this function
        // Lock_IRQs __disableIRQs(*this);

        if (_latest_injected_vintr >= 0) {
            printf(
                    "VINTR FATAL intercepting VINTR %d that should have been taken\n",
                    _latest_injected_vintr);
            exit(1);
        }
        // this function is closely related to L4_cpu::handle_irqs()
        // read both to understand how they work together

        // if we are intercepting VINTR we don't have to check for pending
        // interrupts as we did not have to intercept if there was not one pending.
        // So we just go ahead and inject the next interrupt as an event

        if ((vmcb.control_area.eventinj & 0x80000000)) {
            printf("VINTR event already injected\n");
            exit(1);
        }
        if (vmcb.control_area.interrupt_shadow & 0x1) {
            printf("VINTR in interrupt_shadow\n");
            exit(1);
        }
        if (!(vmcb.state_save_area.rflags & 0x200)) {
            printf("VINTR but interrupts disabled\n");
            exit(1);
        }
        int intr = apic.signal_vcpu();
        if (intr == -1) {
            printf("VINTR fatal intercepting but no interrupt pending\n");
            exit(1);
        }
        //    interrupts[intr]++;
        //printf("VINTR INJECTION Injecting %d as event (vmexit=%u).\n", intr, _vmexit_count);
        inject_event(intr, vmcb);
        GET_STATS_ITEM(interrupt_latency).decpending();

        // now we check for pending interrupts and decide
        // whether to inject or even intercept more VINTRs
        int pending = apic.nr_pending();

        if (pending) {
            // if there there are still interrupts pending and
            // we can simply inject a VINTR
            int intr = apic.signal_vcpu();
            //        ++interrupts[intr];
            //        printf("VINTR INJECTION Injecting %d as VINTR (vmexit=%u, pending%d).\n", intr, _vmexit_count, pending);
            _latest_injected_vintr = intr;
            inject_interrupt(intr, vmcb);
            --pending;
        } else {
            // if there are no more interrupts pending clear the
            // pending VINTR. If by any chance an other interrupt
            // arrives the event handler will inject a VINTR and sets
            // VINTR interception accordingly
            vmcb.control_area.interrupt_ctl = 0;
            _latest_injected_vintr = -1;
        }

        // if at this point there are still interrupts pending
        // must also be a pending VINTR. Now we decide whether
        // to intercept it or not.

        if (pending) {
            // if there are still interrupts pending make sure
            // VINTR gets intercepted
            vmcb.control_area.intercept_instruction0 |= 0x10; // intercept vintr
            // and pull back the last injected interrupt for it will not be taken
            // but will be intercepted and handled above
            int intr = (vmcb.control_area.interrupt_ctl >> 32) & 0xff;
            //        interrupts[intr] -= 1;
            // and put it back in the queue
            //        printf("VINTR INJECTION Pulling %d back (vmexit=%u, pending=%d).\n", intr, _vmexit_count, pending);
            apic.trigger_irq_vcpu(intr);
            _latest_injected_vintr = -1;
        } else {
            // there is no need to intercept VINTR as there are no more interrupts pending
            // if a VINTR is pending it will be taken by the vm.
            vmcb.control_area.intercept_instruction0 &= ~0x10; // do not intercept vintr
        }
    }

};

#endif /* VM_SVM_INTERRUPT_HPP_ */
