/*
 * Karma VMM
 * Copyright (C) 2010  Janis Danisevskis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later versd have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#pragma once

#include <l4/drivers/hpet>
#include <set>
#include "device_interface.h"
#include "../l4_lirq.h"
#include "../l4_vm.h"
#include "../l4_cpu.h"
#include "../util/cas.hpp"
#include "../karma_timer_interface.h"

#define _TO_STRING(x) #x
#define TO_STRING(x) _TO_STRING(x)
#define LATENCY_TRACING
class L4_hpet : public KarmaTimer
{
private:
    L4::Driver::Hpet * _hpet_driver;
    L4::Driver::Hpet::Timer * _hpet_timer;
    L4_lirq _lirq;
    bool _periodic;
    unsigned int _irq_no;
    l4_uint32_t _delta;
    l4_uint32_t _next;
    l4_uint32_t _timer_freq_kHz;

public:
    L4_hpet();
    virtual ~L4_hpet();

    void init();
    inline bool scheduleNext();
    virtual bool nextEvent(l4_uint32_t delta, l4_uint64_t programming_timestamp);
    virtual void setPeriodic(bool periodic);
    virtual bool isPeriodic() const;
    virtual l4_uint32_t getFreqKHz() const;

    virtual void stop();

    bool fire();
    virtual L4::Cap<L4::Irq> & irq_cap();

    virtual void dump(){
        _hpet_driver->print_state();
    }
};

