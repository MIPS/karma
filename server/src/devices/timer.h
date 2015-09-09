/*
 * l4_timer.h - TODO enter description
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

#include <l4/util/util.h>

#include "device_interface.h"
#include "../l4_lirq.h"
#include "../util/debug.h"
#include "../thread.hpp"

class L4_timer : public device::IDevice, private karma::Thread
{
private:
    volatile bool _enabled;
    volatile bool _run;
    volatile bool _reset;
    volatile l4_umword_t _ticks;
    volatile bool _periodic;
    l4_umword_t _ms_sleep;

    L4_lirq _timer_irq;
    L4_lirq _thread_notifier;
    l4_timeout_t _timeout;

    enum {
        L4_timer_init  = 0x0,
        L4_timer_enable = 0x4,
        L4_timer_cycles = 0x8,
    };

    enum {
        L4_timer_enable_periodic = 1,
        L4_timer_enable_oneshot  = 2,
    };

public:
	L4_timer();
	void init(unsigned int irq);
    ~L4_timer() {} 

    virtual void hypercall(HypercallPayload & payload);

    void reset();
    void shutdown();
private:
    l4_umword_t read(l4_umword_t addr);
    void write(l4_umword_t addr, l4_umword_t val);
    virtual void run(L4::Cap<L4::Thread> & thread);

};

