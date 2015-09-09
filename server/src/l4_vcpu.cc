/*
 * l4_vcpu.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * l4_vcpu.cc
 *
 *  Created on: 24.09.2010
 *      Author: janis
 */

#include "l4_vcpu.h"
#include <l4/re/error_helper>

#include <l4/sys/irq>
#include <l4/sys/vcpu.h>
#include <l4/sys/factory>
#include <l4/sys/utcb.h>
#include <l4/sys/debugger.h>
#include <l4/vcpu/vcpu.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/util/util.h>
#include <l4/sys/scheduler>
#include <l4/sys/ktrace.h>
#include <pthread-l4.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "util/statistics.hpp"
#include "l4_lirq.h"
#include "util/debug.h"

using L4Re::chksys;

L4_vcpu::vcpu_specifics_key_t L4_vcpu::_specifics_first_free_key = 0;

L4_vcpu::vcpu_specifics_key_t L4_vcpu::specifics_create_key()
{
    return _specifics_first_free_key++;
}

L4_vcpu::Specific *  L4_vcpu::getSpecific(const vcpu_specifics_key_t key)
{
    return _specifics[key];
}

void  L4_vcpu::setSpecific(const vcpu_specifics_key_t key, Specific * value)
{
    _specifics[key] = value;
}

static void vcpu_destroy(void * _this)
{
    delete reinterpret_cast<L4_vcpu*>(_this);
}

static pthread_key_t init_PTHREAD_VCPU_THIS()
{
    pthread_key_t key;
    pthread_key_create(&key, vcpu_destroy);
    return key;
}

const pthread_key_t PTHREAD_VCPU_THIS = init_PTHREAD_VCPU_THIS();

void * vcpu_thread_fn(void * vcpu)
{
    karma_log(INFO, "VCPU flying!!! this=0x%x &this=0x%x\n", (unsigned int)vcpu, (unsigned int)&vcpu);
    ((L4_vcpu*)vcpu)->_setup();
    return 0;
}

void handler_fn()
{
    unsigned long ebp;
    asm volatile ("movl %%ebp, %0" : "=r"(ebp));
    fiasco_tbuf_log_3val("user_handler", (unsigned int)ebp, 0, 0);
    current_vcpu().handle();
}

static void _simulate_upcall(l4_addr_t target_stack)
{
    __asm__ __volatile__(
            "mov %%eax, %%esp\n"
            "push %%ebx\n"
            "ret"
            : : "a"(target_stack), "b"(handler_fn) );
}


L4_vcpu::L4_vcpu(unsigned prio, l4_umword_t affinity)
:   _first_free_interrupt(0)
,   _specifics(10)
,   _may_run(false)
,   _setup_finished(false)
,   _affinity(affinity)
,   _prio(prio)
{
    for(unsigned int i(0); i != max_interrupts; ++i)
        _interrupts[i] = NULL;
}

void L4_vcpu::prepare()
{
    /* new thread */
    pthread_attr_t thread_attr;

    pthread_t pthread;

    int err;
    if ((err = pthread_attr_init(&thread_attr)) != 0)
        karma_log(ERROR, "error initializing pthread attr: %d", err);

    // tell libpthread that we want a double size utcb to accommodate a vcpu state
    // thread_attr.__supersize_me = 1;

    err = pthread_create(&pthread, &thread_attr, vcpu_thread_fn, this);
    if(err)
    {
        karma_log(ERROR, "error creating thread err=%d\n", err);
        karma_log(ERROR, "Try assigning more threads to karma in your lua config using parameter \"max_threads = <#threads>\"\n");
        karma_log(ERROR, "Can not recover from the last error. Terminating.\n");
        exit(1);
    }


    _vcpu_thread = L4::Cap<L4::Thread>(pthread_getl4cap(pthread));
    if (!_vcpu_thread.is_valid())
      throw 1; //TODO throw proper exception

    l4_debugger_set_object_name(_vcpu_thread.cap(), "vcpu thread");

    _setup_finished.waitConditional(true);
    karma_log(DEBUG, "VCPU: hdl_stack = %p, this = %p\n", _hdl_stack, this);
}

void L4_vcpu::_setup()
{
    long r;

    if ((r = l4vcpu_ext_alloc(&_vcpu_state, &_ext_state,
                            L4Re::Env::env()->task().cap(), l4re_env()->rm)))
    {
        karma_log(ERROR, "Adding state mem failed: %ld\n", r);
        exit(1);
    }
    memset(_vcpu_state, 0, sizeof(l4_vcpu_state_t));

    _vcpu_state->entry_ip = (l4_umword_t)handler_fn;
    // putting this on the handler stack where handler_fn expects it

    l4_umword_t * hdl_stack_ptr = (l4_umword_t *) (_hdl_stack + sizeof(_hdl_stack));
    *(--hdl_stack_ptr) = 0;
    _vcpu_state->entry_sp = (l4_umword_t)hdl_stack_ptr;

    karma_log(DEBUG, "VCPU: utcb = %p, vcpu_state = %p\n", l4_utcb(), _vcpu_state);
    karma_log(DEBUG, "VCPU: hdl_stack = %p, this = %p\n", _hdl_stack, this);

    karma_log(DEBUG, "switching to extended vcpu operation.\n");
    chksys(_vcpu_thread->vcpu_control_ext((l4_addr_t)_vcpu_state));

    l4_sched_param_t sp = l4_sched_param(_prio);
    if(_affinity != (l4_umword_t)-1)
        sp.affinity = l4_sched_cpu_set(_affinity, 0);

    karma_log(DEBUG, "calling run_thread on VCPU\n");
    L4Re::Env::env()->scheduler()->run_thread(L4::Cap<L4::Thread>(pthread_getl4cap(pthread_self())), sp);

    pthread_setspecific(PTHREAD_VCPU_THIS, this);

    _vcpu_state->state       = L4_VCPU_F_FPU_ENABLED;
    _vcpu_state->saved_state = L4_VCPU_F_USER_MODE | L4_VCPU_F_FPU_ENABLED | L4_VCPU_F_IRQ;

    _trip.lirq()->attach(max_interrupts, _vcpu_thread);

    this->setup();

    _setup_finished = true;
    _may_run.waitConditional(true);

    run();
}

void L4_vcpu::start()
{
    karma_log(INFO, "Start VCPU\n");
    _may_run = true;
}

void L4_vcpu::setCPU(int cpu)
{
    l4_sched_param_t schedp = l4_sched_param(_prio);
    schedp.affinity = l4_sched_cpu_set(cpu, 0);
    if (l4_error(L4Re::Env::env()->scheduler()->run_thread(_vcpu_thread, schedp)))
        karma_log(ERROR, "L4_vm: Error migrating thread to CPU%02d\n", cpu);
}

L4_vcpu::~L4_vcpu() 
{
}

void L4_vcpu::handle()
{
    _last_entry = util::tsc();
    if(_vcpu_state->r.trapno == 0xfe)
    {
        if(_vcpu_state->i.label < max_interrupts)
            _interrupts[_vcpu_state->i.label]->handle();
        finalizeIrq();
    }
    else if(_vcpu_state->r.trapno == 0xfeef)
        handle_vmexit();
    else
        karma_log(ERROR, "trapno: 0x%lx\n", _vcpu_state->r.trapno);

    karma_log(ERROR, "should not be reached. Terminating.\n");
    exit(1);
}

void L4_vcpu::registerInterrupt(Interrupt & interrupt, bool register_detached)
{
    interrupt._label = _first_free_interrupt++;
    _interrupts[interrupt._label] = &interrupt;
    if(!register_detached)
        chksys(interrupt.cap()->attach(interrupt._label, _vcpu_thread));
}

void L4_vcpu::resumeUser(L4::Cap<L4::Task> user_task)
{
    _vcpu_state->user_task = user_task.cap();
    _vcpu_state->saved_state |= L4_VCPU_F_FPU_ENABLED | L4_VCPU_F_USER_MODE;

    long result = l4_error(_vcpu_thread->vcpu_resume_commit(_vcpu_thread->vcpu_resume_start()));
    if(result == 0)
    {
        _vcpu_state->r.trapno = 0xfeef;
        _simulate_upcall(_vcpu_state->entry_sp);
    } 
    else if(result > 0)
        // we got ipc super_halt does exactly what we need
        // receive ipc and simulate upcall
        super_halt();
    else
        karma_log(ERROR, "an error occurred during resume operation. Terminating.\n");
    exit(1);
}

void L4_vcpu::disableIrqs()
{
    _vcpu_state->state &= ~L4_VCPU_F_IRQ;
    asm volatile("" : : : "memory");
}

void L4_vcpu::enableIrqs()
{
    while (1)
    {
        _vcpu_state->state |= L4_VCPU_F_IRQ;
        asm volatile("" : : : "memory");

        if (!(_vcpu_state->sticky_flags & L4_VCPU_SF_IRQ_PENDING))
            break;

        disableIrqs();
        _vcpu_state->i.tag = l4_ipc_wait(l4_utcb(), &_vcpu_state->i.label, L4_IPC_NEVER);
        if (!l4_msgtag_has_error(_vcpu_state->i.tag))
            _interrupts[_vcpu_state->i.label]->handle();
    }
}

void L4_vcpu::halt()
{
    disableIrqs();
    _vcpu_state->i.tag = l4_ipc_wait(l4_utcb(), &_vcpu_state->i.label, L4_IPC_NEVER);
    if (!l4_msgtag_has_error(_vcpu_state->i.tag))
        _interrupts[_vcpu_state->i.label]->handle();
    enableIrqs();
}

// super_halt is run in disabled mode
void L4_vcpu::super_halt()
{
    _vcpu_state->i.tag = l4_ipc_wait(l4_utcb(), &_vcpu_state->i.label, L4_IPC_NEVER);
    if (!l4_msgtag_has_error(_vcpu_state->i.tag))
    {
        _vcpu_state->r.trapno = 0xfe;
        _simulate_upcall(_vcpu_state->entry_sp);
    }
}

l4_uint64_t L4_vcpu::tsc_ticks_since_last_entry()
{
    return util::tsc() - _last_entry;
}

void L4_vcpu::trip()
{
    _trip.lirq()->trigger();
}

