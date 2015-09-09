/*
 * l4_vcpu.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * l4_vcpu.h
 *
 *  Created on: 24.09.2010
 *      Author: janis
 */

#ifndef L4_VCPU_H_
#define L4_VCPU_H_

#include <l4/sys/thread>
#include <l4/sys/vcpu.h>
#include <l4/sys/vm>

#include "thread.h"
#include "l4_lirq.h"

#include <vector>
#include <assert.h>

void * vcpu_thread_fn(void * vcpu);
void handler_fn();

class L4_lirq;
class L4_vcpu
{
public:
    class Interrupt
    {
        friend class L4_vcpu;
    private:
        l4_umword_t _label;
    public:
        virtual ~Interrupt(){}
        virtual void handle() = 0;
        virtual L4::Cap<L4::Irq> & cap() = 0;
    };

private:
    enum { max_interrupts = 256 };
    typedef Interrupt *interrupts_t[max_interrupts];
    interrupts_t _interrupts;
    l4_umword_t _first_free_interrupt;

public:
    class Specific
    {
    public:
        virtual ~Specific(){}
    };
    typedef unsigned long vcpu_specifics_key_t;

protected:
    L4::Cap<L4::Thread> _vcpu_thread;
    l4_vcpu_state_t * _vcpu_state;
    l4_addr_t _ext_state;

    typedef std::vector<Specific *> specifics_list_t;
    specifics_list_t _specifics;
    static vcpu_specifics_key_t _specifics_first_free_key;

public:
    static vcpu_specifics_key_t specifics_create_key();
    Specific * getSpecific(const vcpu_specifics_key_t key);
    void setSpecific(const vcpu_specifics_key_t key, Specific * value);

private:
    char _hdl_stack[8 << 10];

    karma::PTCondBool _may_run;
    karma::PTCondBool _setup_finished;

    l4_umword_t _affinity;
    l4_sched_param_t _schedp;
    unsigned _prio;

    l4_uint64_t _last_entry; // tsc value during the latest entry

    L4_lirq _trip;
    l4_utcb_t* _vcpu_utcb;
    inline l4_utcb_t* vcpu_utcb();

public:
    L4_vcpu(unsigned prio);
    virtual ~L4_vcpu();
private:
    L4_vcpu(const L4_vcpu& rhs);            // empty copy constructor
    L4_vcpu& operator=(const L4_vcpu& rhs); // empty assignment constructor
public:

    void prepare();
    void start();
    void set_vcpu_affinity(unsigned int cpu_affinity);
    void trip();

    void resumeUser(L4::Cap<L4::Task> user_task);

    void registerInterrupt(Interrupt &, bool register_detatched = false);

protected:
    virtual void run() = 0;
    virtual void handle_vmexit() = 0;
    virtual void setup() = 0;
    virtual void finalizeIrq() = 0;

public:
    void handle();
    void _thread_setup();
    void _setup();
    void disableIrqs();
    void enableIrqs();
    void halt();
    void super_halt();

    l4_uint64_t tsc_ticks_since_last_entry();

    l4_addr_t getExtState()
    {
        return _ext_state;
    }
    
    l4_vcpu_state_t * getVcpuState()
    {
        return _vcpu_state;
    }

private:
    // Kyma: consider decoupling vz state from vcpu; ideally it should be opaque to vcpu
    l4_umword_t getTrapNo()
    {
        return reinterpret_cast<l4_vm_vz_state_t *>(getExtState())->exit_reason;
    }

    void setTrapNo(l4_umword_t trapno)
    {
        reinterpret_cast<l4_vm_vz_state_t *>(getExtState())->exit_reason =
            static_cast<L4_vm_exit_reason>(trapno);
    }
};

inline l4_utcb_t* L4_vcpu::vcpu_utcb()
{
    assert (_vcpu_utcb);
    return _vcpu_utcb;
}

extern __thread L4_vcpu* _thread_current_vcpu;

inline static L4_vcpu& current_vcpu()
{
    assert (_thread_current_vcpu);
    return *_thread_current_vcpu;
}

#define MY_VCPU_STATE_PTR current_vcpu().getVcpuState()

#endif /* L4_VCPU_H_ */
