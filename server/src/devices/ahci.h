/*
 * l4_ahci.h - TODO enter description
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
#include "../l4_mem.h"

class L4_cpu;

class L4_ahci : public device::IDevice
{
public:
    virtual void hypercall(HypercallPayload &);
    void setIntr(int intr)
    { 
        _intr = intr;
    }
private:
    unsigned int _intr, _rl_int;
};

