/*
 * l4_timer.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#include "timer.h"
#include "../l4_vm.h"

L4_timer::L4_timer() :
    karma::Thread(2, "Karma Timer"), _enabled(false), _ticks(0),
            _periodic(false), _ms_sleep(0)
{
}

void L4_timer::init(unsigned int irq) 
{
    _run = true;
    GET_VM.gic().attach(_timer_irq.lirq(), irq);
    start();
}

void L4_timer::hypercall(HypercallPayload & payload)
{
    if(payload.address() & 1)
        write(payload.address() & ~1UL, payload.reg(0));
    else
        payload.reg(0) = read(payload.address());
}

l4_umword_t L4_timer::read(l4_umword_t addr)
{
    karma_log(TRACE, "L4_timer: read(addr=<%lx>)\n", addr);
    switch (addr)
    {
        case L4_timer_init:
            return _ms_sleep;
        case L4_timer_cycles:
            return _ticks;
    }
    return ~0UL;
}

void L4_timer::write(l4_umword_t addr, l4_umword_t val)
{
    karma_log(TRACE, "L4_timer: write(addr=%lx, val=%lx)\n", addr, val);
    switch (addr)
    {
        case L4_timer_init:
            printf("Initialize Timer!\n");
            _ms_sleep = val;
            _enabled = false;
            _timeout = l4_timeout(L4_IPC_TIMEOUT_NEVER,
                    l4util_micros2l4to(_ms_sleep * 1000));
            reset();
            break;
        case L4_timer_enable:
            if (val == 1) //only periodic timer is enabled, one-shot disabled for now
            {
                printf("Enable Timer!\n");
                _periodic = val == 1 ? true : false;
                _ms_sleep = val;
                _timeout = l4_timeout(L4_IPC_TIMEOUT_NEVER,
                        l4util_micros2l4to(_ms_sleep * 1000));
                _enabled = true;
                reset();
            } else if (val == 0) {
                printf("Disable timer!\n");
                _enabled = false;
                reset();
            }
            break;
    }
}

void L4_timer::reset()
{
    _reset = true;
    _thread_notifier.lirq()->trigger();
}

void L4_timer::shutdown()
{
    _reset = true;
    _run = false;
    _enabled = false;
    _periodic = false;
    _thread_notifier.lirq()->trigger();
}

void L4_timer::run(L4::Cap<L4::Thread> & thread)
{
    karma_log(DEBUG, "timer UTCB %p\n", l4_utcb());
    _thread_notifier.lirq()->attach(12, thread);
    static unsigned int timer_tick_val = 0;
    karma_log(INFO, "Timer running %ldms\n", _ms_sleep);
    _ms_sleep = 10;
    _timeout = l4_timeout(L4_IPC_TIMEOUT_NEVER,
            l4util_micros2l4to(_ms_sleep * 1000));
    karma_log(INFO, "Timer running %ldms\n", _ms_sleep);
    while (_run) 
    {
        if (!_reset || !_enabled)
            _thread_notifier.lirq()->receive();
        do //while (_enabled)
        {
            _reset = false;
            timer_tick_val++;
            _thread_notifier.lirq()->receive(_timeout);
            _ticks++;
            if (_enabled && !_reset)
                _timer_irq.lirq()->trigger();
        } while (_periodic && !_reset);
    }
}

