/*
 * l4_cpu.h - TODO enter description
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

#include <l4/sys/thread>
#include <l4/sys/task>
#include <map>

#include "l4_vm.h"
#include "l4_vm_driver.h"
#include "l4_os.h"
#include "l4_vcpu.h"
#include "l4_cpu_bus.h"
#include "l4_mem.h"
#include "devices/apic.h"


#define VCPU_IPI_LABEL 0x40000000

class L4_vm;

class L4_cpu : public L4_vcpu
{
public:
    L4_cpu(int id,
            L4_os *l4_os);
    ~L4_cpu() {}
    virtual void run();
    void handle_irq(int irq);
    void inject_irq(unsigned irq);
    virtual void handle_vmexit();
    void backtrace();
    void dump_state();
    void dump_exit_reasons();
    void get_thread(L4::Cap<L4::Thread> &thread);
    int id() {return _id;}

    void set_ip_and_sp(l4_umword_t ip, l4_umword_t sp)
    {
        _start_ip = ip;
        _start_sp = sp;
    }

private:
    void run_intern();
    virtual void setup();
    virtual void finalizeIrq();


    unsigned exit_reason[6];
    unsigned vmmcall_reason[6];
    unsigned vmmcall_bus_read[10];
    unsigned vmmcall_bus_write[10];
    int _id;

    L4_vm_driver *_l4_vm_driver;
    L4_os *_l4_os;
    L4_apic *_l4_apic;

    l4_umword_t _start_ip, _start_sp;
};

inline static L4_cpu & current_cpu()
{
    return static_cast<L4_cpu & >(current_vcpu());
}

