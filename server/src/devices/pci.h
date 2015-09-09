/*
 * l4_pci.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include "device_interface.h"

#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_pci.h>
#include <l4/sys/capability>

#define NR_PCI_IRQ_THREADS 16

class L4_pci : public device::IDevice
{
public:
    virtual void hypercall(HypercallPayload &);
    void init();
private:
    void write(l4_umword_t addr, l4_umword_t val);
    l4_umword_t read(l4_umword_t addr);

    L4::Cap<void> _vbus;
    l4vbus_device_handle_t _root_bridge;
};

