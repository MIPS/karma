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
#include "l4_mem.h"


#define VCPU_IPI_LABEL 0x40000000

class L4_vm;

class L4_cpu : public L4_vcpu
{
public:
    L4_cpu(int id,
            L4_os *l4_os);
    virtual ~L4_cpu();
private:
    L4_cpu(const L4_cpu& rhs);            // empty copy constructor
    L4_cpu& operator=(const L4_cpu& rhs); // empty assignment constructor
public:
    virtual void run();
    void handle_irq(int irq);
    virtual void handle_vmexit();
    void backtrace();
    void dump_state();
    void dump_exit_reasons();
    void get_thread(L4::Cap<L4::Thread> &thread);
    int id() {return _id;}
    inline L4_vcpu& vcpu() { return static_cast<L4_vcpu & >(*this); }

    void set_ip_and_sp(l4_umword_t ip, l4_umword_t sp)
    {
        _start_ip = ip;
        _start_sp = sp;
    }

    void set_spawned() { _spawned = true; }
    bool spawned() { return _spawned; }

private:
    void run_intern();
    virtual void setup();
    virtual void finalizeIrq();

    int _id;

    L4_vm_driver *_l4_vm_driver;
    L4_os *_l4_os;

    l4_umword_t _start_ip, _start_sp;
    bool _spawned;
};

inline static L4_cpu & current_cpu()
{
    return static_cast<L4_cpu & >(current_vcpu());
}

