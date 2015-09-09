/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#ifndef VM_VZ_INTERRUPT_HPP_
#define VM_VZ_INTERRUPT_HPP_

#include <l4/sys/cp0_op.h>
#include "../l4_vm.h"
#include "vmm.h"
#include "mipsregs.h"
#include "mipsvzregs.h"
#include "../util/debug.h"
#include "irqchip.h"

class INTERRUPT_HANDLING {

private:
    enum {
        CLEAR_INTERRUPT = -1
    };

public:

    // set irq = -1 to clear interrupts
    inline void handle(l4_vm_vz_vmcb_t & vmcb, int irq) {
        (void)vmcb;
        if (params.paravirt_guest)
            mipsvz_karma_irq_line(static_cast<enum karma_device_irqs>(irq), 1);
#if 1 // KYMA GIC_PENDING
        else
        {
            if (irq != CLEAR_INTERRUPT) {
                // set pending bit
                GET_VM.gic().pending(irq);
            }

            vz_vmm_t * vmm = reinterpret_cast<vz_vmm_t *>(&vmcb);
            vmm->vmstate->set_injected_ipx = 1;
            if (irq == CLEAR_INTERRUPT)
                vmm->vmstate->injected_ipx = 0;
            else
                vmm->vmstate->injected_ipx = CAUSEF_IP2;
        }
#endif
    }

    inline void update(l4_vm_vz_vmcb_t & vmcb) {
        if (params.paravirt_guest)
            (void)vmcb;
#if 1 // KYMA GIC_PENDING
        else
        {
            // check pending irqs and clear injected interrupts if necessary
            if (!GET_VM.gic().pending())
                handle(vmcb, CLEAR_INTERRUPT);
        }
#endif
    }
};

#endif /* VM_VZ_INTERRUPT_HPP_ */

