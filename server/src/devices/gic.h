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
#include "../l4_exceptions.h"

#include <stdlib.h>
#include <cstdio>
#include <string.h>
#include <map>
#include <list>

class L4_apic;
class L4_cpu;

struct l4_gic_shared_state
{
    l4_uint32_t _enabled;
    l4_uint32_t _level_trigered;
};

#define NR_INTR 16
class Gic : public device::IDevice
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
    l4_gic_shared_state * _shared_state;
    l4_addr_t _shared_state_gpa;

private:
    L4::Cap<L4::Irq> _irqs[NR_INTR];
    L4_cpu * _cpu;
    bool _hasVCPU, _enable_uirq;

    l4_umword_t read(l4_umword_t addr);
    void write(l4_umword_t addr, l4_umword_t val);

public:
    Gic();
    virtual ~Gic();
    void init();
    void set_cpu(L4_cpu & cpu);

    virtual void hypercall(HypercallPayload &);

    void attach(L4::Cap<L4::Irq> & lirq, l4_umword_t as, bool level = false);
    void attach(l4_umword_t irq_no, l4_umword_t as = -1, bool level = false);
    void detach(l4_umword_t label);
    void mask(l4_umword_t label);
    void unmask(l4_umword_t label);
    bool isMasked(l4_umword_t label) const;
    void checkIRQ(l4_umword_t label);
};

