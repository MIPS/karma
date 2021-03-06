/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * l4_gic.h - TODO enter description
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

#include <l4/sys/types.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/irq>
#include <l4/re/util/cap_alloc>
#include <l4/io/io.h>
#include <l4/re/env>
#include <l4/sys/debugger.h>
#include <l4/sys/types.h>
#include <l4/sys/scheduler>
#include <l4/sys/factory>

#include "device_interface.h"
#include "devices_list.h"

#include <list>

class L4_apic;
class L4_cpu;

#define NR_INTR 16
class Gic
{
private:
    class postponed_attach
    {
    public:
        bool cap_type;
        bool level_triggered;
        unsigned as;
        unsigned irq;
        L4::Cap<L4::Irq> cap;
    };

    std::list<postponed_attach> _postponed_attach_list;
    karma_gic_shared_state * _shared_state;
    l4_addr_t _shared_state_gpa;

private:
    L4::Cap<L4::Irq> _irqs[NR_INTR];
    L4_cpu * _cpu;
    bool _hasVCPU, _enable_uirq;

public:
    Gic();
    virtual ~Gic();
private:
    Gic(const Gic& rhs);            // empty copy constructor
    Gic& operator=(const Gic& rhs); // empty assignment constructor
public:
    void init();
    void set_cpu(L4_cpu & cpu);

    void attach(L4::Cap<L4::Irq> & lirq, l4_umword_t as, bool level = false);
    void attach(l4_umword_t irq_no, l4_umword_t as = -1, bool level = false);
    void detach(l4_umword_t label);
    void mask(l4_umword_t label);
    void unmask(l4_umword_t label);
    bool isMasked(l4_umword_t label) const;
    void checkIRQ(l4_umword_t label);
#if 1 // KYMA GIC_PENDING
    void pending(l4_umword_t label);
    l4_umword_t pending();
    void ack(l4_umword_t label);
#endif
    unsigned long enabled() {
        return _shared_state->_enabled;
    }
    l4_addr_t shared_state_gpa() {
        return _shared_state_gpa;
    }
};

class GicDevice : public device::IDevice {
public:
    GicDevice();

    virtual void hypercall(HypercallPayload &);

    void init(Gic *gic);
    l4_umword_t read(l4_umword_t addr);
    void write(l4_umword_t addr, l4_umword_t val);

private:
    Gic *_gic;
};

