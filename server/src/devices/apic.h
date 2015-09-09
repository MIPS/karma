/*
 * l4_apic.h - TODO enter description
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

#include <l4/re/env>
#include <l4/sys/factory>
#include <l4/sys/types.h>
#include <l4/sys/irq>

#include "device_interface.h"
#include "../l4_cpu_bus.h"
#include "../l4_mem.h"
#include "../karma_timer_interface.h"
#include "../l4_vcpu.h"

#include <map>

struct registers
{
    l4_uint32_t id, // apic id
                version, // apic version
                tpr, // task priority register
                apr, // arbitration priority register
                ppr, // processor priority register
                eoi, // end of interrupt register
                rrr, // remote read register
                ldr, // karma_logical destination register
                dfr, // destination format register
                sivr, // spurious interrupt vector register
                isr[8], // in-service register
                tmr[8], // trigger mode register
                irr[8], // interrupt request register
                esr, // error status register
                icrl, // interrupt command register low
                icrh, // interrupt command register high
                tlvte, // timer local vector table entry
                thlvte, // thermal local vector table entry
                pclvte, // performance counter local vector table entry
                li0vte, // local interrupt 0 vector table entry
                li1vte, // local interrupt 1 vector table entry
                evte, // error vector table entry
                ticr, // timer initial count register
                tccr, // timer current count register
                tdcr, // timer divide configuration register
                eafr, // extended apic feature register
                eacr, // extended apic control register
                seoi, // specific end oif interrupt register
                ier[8], // interrupt enable registers
                eilvtr[3]; // extended interrupt local vector table registers
};

class L4_cpu_bus;

class L4_apic : public device::IDevice, public L4_vcpu::Interrupt
{
public:
    L4_apic();
    ~L4_apic() {}

    void setId(int);
    void init();


    virtual void hypercall(HypercallPayload & payload);

    // do we have a triggered interrupt?
    int signal_vcpu();
    int nr_pending();

    void trigger_irq_vcpu(l4_uint32_t intr);

    void
    trigger_ipi(l4_uint32_t intr);

    unsigned
    put_ipi_logical(l4_uint8_t vector, l4_uint8_t destination);

    void get_uirq(L4::Cap<L4::Irq> &irq);

    void dump();
    
    virtual L4::Cap<L4::Irq> & cap();
    virtual void handle();

private:
    l4_umword_t read(l4_umword_t addr);
    void write(l4_umword_t addr, l4_umword_t val, l4_uint64_t programming_offset);

    unsigned _base;
    l4_umword_t _ms_sleep, _periodic;
    l4_umword_t _enabled;
    l4_umword_t _pending;
    l4_umword_t _signaled;
    l4_umword_t _apic_timer_intr;
    unsigned counter;
    int _id;
    L4::Cap<L4::Irq> _uirq;
    struct registers regs;
    enum irq_state
    {
        Clear,
        Triggered
    };
    l4_uint32_t irq_queue[256];
    int max_irq;
    unsigned int intr_counter;
    bool _enable_uirq;

    KarmaTimer * _timer;
public:
    std::map<unsigned, unsigned> _interrupts;
};

