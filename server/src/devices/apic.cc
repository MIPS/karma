/*
 * l4_apic.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "apic.h"
#include "../l4_vm.h"
#include "../util/debug.h"
#include "../util/statistics.hpp"
#include "../util/cas.hpp"
#include "../karma_timer_interface.h"
#include "../l4_cpu.h"
#include "../l4_lirq.h"
#include "devices_list.h"
#include "hpet.h"

#include <l4/re/util/cap_alloc>
#include <l4/util/util.h>
#include <l4/sys/irq>
#include <l4/sys/ktrace.h>

#include <cstdio>
#include <stdlib.h>

class L4_ApicTimer : public KarmaTimer, private karma::Thread
{
private:
    L4_lirq _lirq;
    L4_lirq _lirq_intern;
    l4_uint64_t _next_tsc;
    l4_uint32_t _delta_tt;
    bool _periodic;
    bool _enabled;
    enum { _tick_freq = 1000 }; // 1 MHz or 1us per tick

    inline l4_uint32_t tsc2us(l4_uint64_t tsc)
    {
        return (tsc * 1000) / ((l4_uint64_t)l4re_kip()->frequency_cpu);
    }
    inline l4_uint32_t tsc2tt(l4_uint64_t tsc)
    {
        return tsc * _tick_freq / (l4_uint64_t)l4re_kip()->frequency_cpu;
    }
    inline l4_uint64_t tt2tsc(l4_uint32_t tt)
    {
        return (l4_uint64_t)tt * (l4_uint64_t)l4re_kip()->frequency_cpu / _tick_freq;
    }

public:
    L4_ApicTimer()
    :   karma::Thread(params.vcpu_prio + 1, "ApicTimer")
    ,   _next_tsc(0)
    ,   _delta_tt(0)
    ,   _periodic(false)
    ,   _enabled(false)
    { }

    virtual void init ()
    {
        start(current_cpu().id());
    }

    inline bool scheduleNext()
    {
        if(!_periodic && ((l4_int32_t)(_next_tsc - util::tsc())) <= (l4_int32_t)tt2tsc(1))
            return false;
        else 
        {
            _enabled = true;
            _lirq_intern.lirq()->trigger();
        }
        return true;
    }

    virtual bool nextEvent(l4_uint32_t delta, l4_uint64_t programming_timestamp)
    {
        _delta_tt = delta;
        _next_tsc = programming_timestamp + tt2tsc(_delta_tt);
        return scheduleNext();
    }

    virtual void setPeriodic(bool periodic)
    {
        _periodic = periodic;
    }

    virtual bool isPeriodic() const
    {
        return _periodic;
    }

    virtual l4_uint32_t getFreqKHz() const
    {
        return _tick_freq;
    }

    virtual void stop()
    {
        _enabled = false;
        _lirq_intern.lirq()->trigger();
    }

    virtual bool fire()
    {
        return true;
    }

    virtual L4::Cap<L4::Irq> & irq_cap()
    {
        return _lirq.lirq();
    }

    virtual void run(L4::Cap<L4::Thread> & thread)
    {
        _lirq_intern.lirq()->attach(1, thread);
        while(true)
        {
            if(_enabled)
            {
                if(_periodic)
                {
                    l4_uint64_t latency = (util::tsc() - _next_tsc) % tt2tsc(_delta_tt);
                    _next_tsc = util::tsc() + tt2tsc(_delta_tt) - latency;
                }
            } 
            else 
            {
                do
                {
                    _lirq_intern.lirq()->receive();
                } while(!_enabled);
            }
            l4_int64_t delta_tsc = _next_tsc - util::tsc();
            if(delta_tsc > 0)
            {
                l4_uint32_t timeout_us = tsc2us(delta_tsc);
                l4_timeout_t timeout = l4_timeout(L4_IPC_TIMEOUT_NEVER,
                    l4util_micros2l4to(timeout_us));
                l4_msgtag_t tag = _lirq_intern.lirq()->receive(timeout);
                if(tag.has_error() && l4_ipc_error(tag, l4_utcb()) == L4_IPC_RETIMEOUT)
                {
                    _enabled = _periodic;
                    _lirq.lirq()->trigger();
                } 
            }
            else 
            {
                _enabled = _periodic;
                _lirq.lirq()->trigger();
            }
        }
    }

};

/**
 *
 * Sending IPIs (inter processor interrupts):
 *
 *   - sending is triggered by writing to the interrupt command register (ICR)
 *   - it is important that the IPI is handled immediately after sending
 *     because the sender usually has to wait for the completion of the operation
 *      at the target (TLB shootdown, rescheduling etc pp)
 *   - therefore we cause the target CPU to vmexit in the following manner:
 *
 *      1) each virtual CPU is running on a different physical CPU (core)
 *      2) each virtual CPU has an IPI thread attached, that runs on the same
 *         physical CPU (core). The IPI thread waits for a user-irq.
 *      3) before sending the IPI, we trigger the target IPI thread's user-irq.
 *      4) fiasco delivers the user-irq by sending a (physical) IPI
 *      5) the IPI causes the target CPU to vmexit
 *      6) the target CPU can now handle the IPI
 *
 * Other solutions:
 * - sending an IPC (slower, because that requires a rendevouz of sender an receiver)
 */

L4_apic::L4_apic()
:   _enabled(1)
,   _pending(0)
,   _signaled(0)
,   _id(0)
,   _enable_uirq(false)
{
    _uirq = L4Re::Util::cap_alloc.alloc<L4::Irq>();
//    _apic_irq = L4Re::Util::cap_alloc.alloc<L4::Irq>();
    if(!_uirq.is_valid()){  // && !_apic_irq.is_valid()) {
        printf("L4_apic: error creating uirq.\n");
        exit(1);
    }
    l4_msgtag_t msg = L4Re::Env::env()->factory()->create_irq(_uirq);
    if(l4_error(msg)) {
        printf("L4_apic: error getting uirq.\n");
        exit(1);
    }

    regs.version = 0xf0007; // random between 0x2 and 0xff
    regs.tlvte = 0xff; // random
//    regs.id = (_id<<24);
    regs.li0vte = 0x00010000;
    regs.li1vte = 0x00010000;
    regs.ldr = 0;
    regs.dfr = ~0;
    regs.tpr = 0;
    for(int i=0; i<8; i++)
        regs.isr[i] = 0;
    regs.sivr = 0xff;
    regs.tdcr = 0;
    regs.ticr = 0;
    regs.tccr = 0;
    regs.tlvte = 0x10000;
    regs.rrr = 0;
    for(int i=0; i<8; i++)
        regs.tmr[i] = 0;
    for(int i=0; i<8; i++)
        regs.irr[i] = 0;
    regs.icrh = 0;
    regs.icrl = 0;

    // clear all irqs
    for(int i=0; i<256; i++)
        irq_queue[i] = 0;

    _apic_timer_intr = 0;
    _periodic=0;
    intr_counter=0;
    max_irq = -1;
    counter=0;
}

void L4_apic::setId(int id)
{
    _id = id;
    regs.id = _id << 24;
}

void L4_apic::init()
{
    if(params.hpet && _id == 0)
        _timer = new L4_hpet;
    else 
        _timer = new L4_ApicTimer;

    _timer->init();

    current_vcpu().registerInterrupt(*this);
}

L4::Cap<L4::Irq> & L4_apic::cap()
{
    return _timer->irq_cap();
}

void L4_apic::handle()
{
    if(_timer->fire())
        current_cpu().inject_irq(_apic_timer_intr - 0x30);
}

l4_umword_t L4_apic::read(l4_umword_t addr)
{
    l4_umword_t ret = 0;
    counter++;
    switch(addr)
    {
        case 0x20:
            // apic id
            ret = regs.id;
            break;
        case 0x30:
            // apic version
            ret = regs.version;
            break;
        case 0x80:
            // task priority register
            ret = regs.tpr;
            break;
        case 0xc0:
            // remote read register
            ret = regs.rrr;
            break;
        case 0xd0:
            // karma_logical destination register
            ret = regs.ldr;
            break;
        case 0xe0:
            // destination format register
            ret = regs.dfr;
            break;
        case 0xf0:
            // spurious interrupt vector register
            ret = regs.sivr;
            break;
        case 0x100:
            // interrupt service register read only
            ret = regs.isr[0];
            break;
        case 0x110:
            // interrupt service register read only
            ret = regs.isr[1];
            break;
        case 0x120:
            // interrupt service register read only
            ret = regs.isr[2];
            break;
        case 0x130:
            // interrupt service register read only
            ret = regs.isr[3];
            break;
        case 0x140:
            // interrupt service register read only
            ret = regs.isr[4];
            break;
        case 0x150:
            // interrupt service register read only
            ret = regs.isr[5];
            break;
        case 0x160:
            // interrupt service register read only
            ret = regs.isr[6];
            break;
        case 0x170:
            // interrupt service register 8
            ret = regs.isr[7];
            break;
        case 0x180:
            // trigger mode register
            ret = regs.tmr[0];
            break;
        case 0x190:
            // trigger mode register
            ret = regs.tmr[1];
            break;
        case 0x1a0:
            // trigger mode register
            ret = regs.tmr[3];
            break;
        case 0x1b0:
            // trigger mode register
            ret = regs.tmr[4];
        case 0x1c0:
            // trigger mode register
            ret = regs.tmr[5];
            break;
        case 0x1d0:
            // trigger mode register
            ret = regs.tmr[5];
        case 0x1e0:
            // trigger mode register
            ret = regs.tmr[6];
            break;
        case 0x1f0:
            // trigger mode register
            ret = regs.tmr[7];
            break;
        case 0x200:
            // interrupt service register read only
            ret = regs.irr[0];
            break;
        case 0x210:
            // interrupt service register read only
            ret = regs.irr[1];
            break;
        case 0x220:
            // interrupt service register read only
            ret = regs.irr[2];
            break;
        case 0x230:
            // interrupt service register read only
            ret = regs.irr[3];
            break;
        case 0x240:
            // interrupt service register read only
            ret = regs.irr[4];
            break;
        case 0x250:
            // interrupt service register read only
            ret = regs.irr[5];
            break;
        case 0x260:
            // interrupt service register read only
            ret = regs.irr[6];
            break;
        case 0x270:
            // interrupt service register 8
            ret = regs.irr[7];
            break;
        case 0x280:
            // error status register
            ret  = regs.esr;
            break;
        case 0x300:
            counter--;
            // interrupt command register low
            ret = regs.icrl;
            break;
        case 0x310:
            counter--;
            // interrupt command register high
            ret = regs.icrh;
            break;
        case 0x320:
            // timer local vector table entry
            ret = regs.tlvte;
            break;
        case 0x350:
            // local interrupt 0 vector table entry
            ret = regs.li0vte;
            break;
        case 0x360:
            // local interrupt 1 vector table entry
            ret = regs.li1vte;
            break;
        case 0x380:
            // timer initial count register
            ret = regs.ticr;
            break;
        case 0x390:
            // timer current count register
            ret = regs.tccr;
            break;
        case 0x3e0:
            // timer divide configuration register
            ret = regs.tdcr;
            break;
        default:
            printf("L4_apic: read(addr = %x)\n", (unsigned int)addr);
            printf("Not implemented.\n");
            ret = ~0UL;
            break;
    }
    return ret;
}

struct _a {unsigned a, b;} *a;
l4_uint32_t vector, destination;

void L4_apic::write(l4_umword_t addr, l4_umword_t val, l4_uint64_t programming_offset)
{
    counter++;
    switch(addr)
    {
        case 0x02:
            counter--;
            // send direct IPI (shortcut)
            a = (struct _a*)(GET_VM.mem().base_ptr() + val);
            if(!(a->b & 0x800))
            {
                printf("IPI not sent, only karma_logical addressing supported.\n");
                return;
            }
            vector = a->b & 0xff;
            //if(vector & 0xf0 && !(vector &0x8))
            //    printf("> %d vector=%d\n", _id, vector);
            destination = (a->a & 0xff000000) >> 24;
            // check for shortcuts
            if(a->b & 0x40000) // self
                trigger_ipi(vector);
            else if(a->b & 0x80000) // all including self
                destination = 0xf;
            else if(a->b & 0xc0000) // all excluding self
                destination = 0xe;
            karma_log(INFO, "[apic %d] sent IPI dest=%x vect=%d\n", _id, destination, vector);
            GET_VM.cpu_bus().broadcast_ipi_logical(_id, vector, destination);
            break;
        case 0x20:
            // apic id
            regs.id = val;
            break;
        case 0x30:
            // version register is read only
            break;
        case 0x80:
            // task priority register
            //printf("L4_apic%d: task priority register set to 0x%08x\n",
            //    _id, (unsigned int)val);
            regs.tpr = val;
            //regs.tpr=0x80;
            break;
        case 0xb0:
            break;
        case 0xd0:
            // karma_logical destination register
            //printf("l4_apic%d: set karma_logical destination register to %x\n",
            //    _id, (unsigned int) val);
            //printf("L4_apic: CPU%d LDR=%d\n", _id, val);
            regs.ldr = val;
            break;
        case 0xe0:
            // destination format register
            regs.dfr = val;
            break;
        case 0xf0:
            // spurious interrupt vector register
            regs.sivr = regs.sivr | (0xff & val);
            break;
        case 0x100:
            // interrupt service register read only
            break;
        case 0x110:
            // interrupt service register read only
            break;
        case 0x120:
            // interrupt service register read only
            break;
        case 0x130:
            // interrupt service register read only
            break;
        case 0x140:
            // interrupt service register read only
            break;
        case 0x150:
            // interrupt service register read only
            break;
        case 0x160:
            // interrupt service register read only
            break;
        case 0x170:
            // interrupt service register read only
            break;
        case 0x280:
            // error status register
            regs.esr = val;
            break;
        case 0x300:
            // interrupt command register
            if(val & 0x800) // karma_logical addressing mode
            {
                l4_uint8_t vector = val & 0xff;
                //if(vector & 0xf0 && !(vector &0x8))
                //    printf(">> %d vector=%d\n", _id, vector);
                l4_uint8_t destination = (regs.icrh & 0xff000000) >> 24;
                // check for shortcuts
                if(val & 0x40000) // self
                    trigger_ipi(vector);
                if(val & 0x80000) // all including self
                    destination = 0xf;
                if((val & 0xc0000) == 0xc0000) // all excluding self
                    destination = 0xe;
                GET_VM.cpu_bus().broadcast_ipi_logical(_id, vector, destination);
                regs.icrh = 0;
                regs.icrl = 0;
            }
            else
                printf("[apic %d] Ipi not sent. Only logical addressing mode supported.\n",
                    _id);
            break;
        case 0x310:
            // interrupt command register
            regs.icrh = val;
            break;
        case 0x320:
            // timer local vector table entry
            regs.tlvte = val;
            _apic_timer_intr = val & 0xff;
            //printf("L4_apic%d: apic timer interrupt vector = %d\n",
            //    _id, (int)_apic_timer_intr);
            if(val & 0x10000) _timer->stop();
            if(val & 0x20000)
                _periodic = 1;
            else
                _periodic = 0;
            _timer->setPeriodic(_periodic);
            break;
        case 0x350:
            // local interrupt 0 vector table entry
            regs.li0vte = val;
            break;
        case 0x360:
            // local interrupt 1 vector table entry
            regs.li1vte = val;
            break;
        case 0x380:
            // timer initial count register
            {
                // translate val into the time domain of the used timer
                l4_uint32_t delta = ((l4_uint64_t)val * (l4_uint64_t)_timer->getFreqKHz() * 16ULL) / (l4_uint64_t)l4re_kip()->frequency_bus;
                // prevent delta from becoming 0 if val was > 0 and thus stopping the timer unintentionally
                if(val && !delta) delta = 1;
                if(!delta) _timer->stop();
                else if(!_timer->nextEvent(delta, programming_offset))
                    // event is in the past trigger now
                    handle();
            }
            regs.ticr = val;
            regs.tccr = val;
            break;
        case 0x390:
            // timer current count register (ro)
            break;
        case 0x3e0:
            // timer divide configuration register
            regs.tdcr = val;
            break;
        default:
            printf("[apic %d] write(addr = %lx, val = %lx). Not implemented. Ignoring.\n",
                _id, addr, val);
            break;
    }
}

void L4_apic::hypercall(HypercallPayload & payload)
{
    if(payload.address() & 0x1)
    {
        l4_uint64_t prog_offset = payload.reg(2);
        prog_offset <<= 32;
        prog_offset |= payload.reg(1);
        write(payload.address() & ~0x1, payload.reg(0), prog_offset);
    }
    else
    {
        payload.reg(0) = read(payload.address());
    }
}

int L4_apic::signal_vcpu()
{
    int intr = 0;

    // find the highes pending interrupt
    intr = 255;
    while(intr >= 0 && irq_queue[intr] == 0)
        intr--;

    if(intr >= 0)
        // if an interrupt is pending pop it from it's queue
        util::cas32add(&irq_queue[intr], -1);

    // return the interrupt (-1 if none was pending)
    return intr;
}

int L4_apic::nr_pending()
{
    int counter = 0;
    for(int i=0; i<255; i++)
    {
        counter += irq_queue[i];
    }
    return counter;
}

void L4_apic::trigger_irq_vcpu(l4_uint32_t intr)
{
    util::cas32add(&irq_queue[intr], 1);
}

void L4_apic::trigger_ipi(l4_uint32_t intr)
{
    /* make target CPU vmexit */
    trigger_irq_vcpu(intr);
    _uirq->trigger();
}

unsigned L4_apic::put_ipi_logical(l4_uint8_t vector, l4_uint8_t destination)
{
    l4_uint32_t dest = destination, vect = vector;
    if((dest & ((regs.ldr & 0xff000000)>>24)))
    {
        trigger_ipi(vect);
        return 1;
    }
    if(dest == 0xf) // broadcast!
    {
        trigger_ipi(vect);
        return 1;
    }
    return 0;
}

void L4_apic::dump()
{
    printf("L4_apic: pending interrupts\n");
    for(int i(0); i != 256; ++i){
        if(irq_queue[i])
            printf("\t%u: 0x%x\n", i, irq_queue[i]);
    }
}

void L4_apic::get_uirq(L4::Cap<L4::Irq> &irq)
{
    irq = _uirq;
}

