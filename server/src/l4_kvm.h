/*
 * l4_kvm.h - TODO enter description
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

#include "devices/device_interface.h"

#define L4_KVM_VMRUN    0x2
#define L4_KVM_MAP      0x4
#define L4_KVM_CREATE   0x8
#define L4_KVM_DESTROY  0x10
#define L4_KVM_UNMAP    0x20

class L4_vm;

class L4_kvm: public device::IDevice {
public:
    L4::Cap<L4::Vm> _vm_cap;
    unsigned in_use;
    int _phys_intr;
    int _last_id;
    bool flushtlb;
    l4_uint32_t osec;
public:
    L4_kvm();
    ~L4_kvm() {
    }
    virtual void hypercall(HypercallPayload &);
    int phys_intr() {
        int t = _phys_intr;
        _phys_intr = 0;
        return t;
    }
    void id(int id) {
        if (_last_id != id) {
            flushtlb = true;
            _last_id = id;
        } else
            flushtlb = false;
    }
private:
    l4_umword_t read(l4_umword_t addr);
    void write(l4_umword_t addr, l4_umword_t value);
};

