/*
 * l4_ahci.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "ahci.h"
#include "../l4_vm.h"
#include "../util/debug.h"

#define L4_AHCI_CONFIGURE_IRQ 0x4

void L4_ahci::hypercall(HypercallPayload & payload)
{
    int *irq = NULL;
    switch(payload.address())
    {
        case L4_AHCI_CONFIGURE_IRQ:
            irq = (int*)(GET_VM.mem().base_ptr() + payload.reg(0));
            karma_log(INFO, "real irq is %d\n", *irq);
            _rl_int = *irq;
            *irq = _intr;
            GET_VM.gic().attach(_rl_int, _intr, true);
            GET_VM.gic().unmask(_intr);
            break;
        default:
            karma_log(INFO, "ahci: received unknown hypercall\n");
            payload.dump();
    }
}

