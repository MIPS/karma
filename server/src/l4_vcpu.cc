/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

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

#include <string.h>

#include "util/statistics.hpp"
#include "l4_lirq.h"
#include "util/debug.h"

#undef MIPS_FPU_NOT_YET

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

void handler_fn()
{
    unsigned long sp;
    asm volatile ("move %0, $sp" : "=r"(sp));
    if (LOG_LEVEL(TRACE))
        fiasco_tbuf_log_3val("user_handler", (unsigned int)sp, 0, 0);
    current_vcpu().handle();
}

static void _simulate_upcall(l4_addr_t target_stack)
{
  // call handle_upcall without modifying stack
  asm volatile (
    "  .set push \n"
    "  .set noreorder \n"
    "  move $sp, %0 \n"
    "  jr   %1 \n"
    "       nop \n"
    "  .set pop \n"
    : : "r" (target_stack), "r" (&handler_fn)
    );
  // never reached
  karma_log(ERROR, "Error jumping to handle_upcall\n");
}

__thread L4_vcpu* _thread_current_vcpu;

L4_vcpu::L4_vcpu(unsigned prio)
:   _first_free_interrupt(0)
,   _specifics(10)
,   _may_run(false)
,   _setup_finished(false)
,   _affinity(0)
,   _prio(prio)
,   _vcpu_utcb(NULL)
{
    _schedp = l4_sched_param(_prio);

    for(unsigned int i(0); i != max_interrupts; ++i)
        _interrupts[i] = NULL;
}

L4_vcpu::~L4_vcpu() {}

void * vcpu_thread_fn(void * vcpu)
{
    karma_log(INFO, "VCPU flying!!! this=0x%x &this=0x%x\n", (unsigned int)vcpu, (unsigned int)&vcpu);
    ((L4_vcpu*)vcpu)->_thread_setup();
    ((L4_vcpu*)vcpu)->_setup();
    return 0;
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

    _setup_finished.waitConditional(true);
    karma_log(DEBUG, "VCPU: hdl_stack = %p, this = %p\n", _hdl_stack, this);
}

void L4_vcpu::_thread_setup()
{
    _vcpu_thread = L4::Cap<L4::Thread>(pthread_getl4cap(pthread_self()));
    if (!_vcpu_thread.is_valid())
    {
        karma_log(ERROR, "_vcpu_thread not valid!\n");
        exit(1);
    }

    l4_debugger_set_object_name(_vcpu_thread.cap(), "vcpu thread");

    // set thread affinity
    chksys(L4Re::Env::env()->scheduler()->run_thread(_vcpu_thread, _schedp));

    // initialize __thread local storage variable from within each vcpu thread
    _thread_current_vcpu = this;

    // save this thread's utcb for performance reasons
    _vcpu_utcb = l4_utcb();
}

void L4_vcpu::_setup()
{
    if (long res = l4vcpu_ext_alloc(&_vcpu_state, &_ext_state,
                            L4Re::Env::env()->task().cap(), l4re_env()->rm))
    {
        karma_log(ERROR, "Adding state mem failed: %ld\n", res);
        exit(1);
    }
    memset(_vcpu_state, 0, sizeof(l4_vcpu_state_t));

    _vcpu_state->entry_ip = (l4_umword_t)handler_fn;
    // putting this on the handler stack where handler_fn expects it

    l4_umword_t * hdl_stack_ptr = (l4_umword_t *) (_hdl_stack + sizeof(_hdl_stack));
    *(--hdl_stack_ptr) = 0;
    _vcpu_state->entry_sp = (l4_umword_t)hdl_stack_ptr;

    karma_log(DEBUG, "VCPU: utcb = %p, vcpu_state = %p\n", vcpu_utcb(), _vcpu_state);
    karma_log(DEBUG, "VCPU: hdl_stack = %p, this = %p\n", _hdl_stack, this);

    karma_log(DEBUG, "switching to extended vcpu operation.\n");
    chksys(_vcpu_thread->vcpu_control_ext((l4_addr_t)_vcpu_state));

    pthread_setspecific(PTHREAD_VCPU_THIS, this);

#if defined(MIPS_FPU_NOT_YET)
    _vcpu_state->state       = 0;
    _vcpu_state->saved_state = L4_VCPU_F_USER_MODE | L4_VCPU_F_IRQ;
#else
    _vcpu_state->state       = L4_VCPU_F_FPU_ENABLED;
    _vcpu_state->saved_state = L4_VCPU_F_USER_MODE | L4_VCPU_F_FPU_ENABLED | L4_VCPU_F_IRQ;
#endif

    _trip.lirq()->attach(max_interrupts, _vcpu_thread);

    this->setup();

    _setup_finished = true;
    _may_run.waitConditional(true);

    karma_log(INFO, "*** Starting VM Guest ***\n");
    run();
}

void L4_vcpu::start()
{
    karma_log(INFO, "Start VCPU\n");
    _may_run = true;
}

void L4_vcpu::set_vcpu_affinity(unsigned int cpu_affinity)
{
    _affinity = cpu_affinity;
    _schedp = l4_sched_param(_prio);
    _schedp.affinity = l4_sched_cpu_set(_affinity, 0, 0x1);
}

/*
 * Entered via _simulate_upcall or directly via kernel upcall after guest VM
 * exit.
 */
void L4_vcpu::handle()
{
    _last_entry = util::tsc();

    L4_vm_exit_reason e = static_cast<L4_vm_exit_reason>(getTrapNo());

    karma_log(TRACE, "handle upcall exit_reason=%s:%x\n", L4_vm_exit_reason_to_str(e), e);

#if 1
    /*
     * Kyma: In case of Fiasco Kernel with config::Irq_shortcut, Irqs events may
     * be delivered directly instead of via ipc messages. This breaks the
     * vcpu_resume_commit API which no longer returns 1 to indicate a pending
     * ipc message containing the triggered Irq event.
     *
     * As a workaround Fiasco has been changed to mark the tag label with
     * L4_PROTO_IRQ so that this situation can be detected. The tag label must
     * be cleared before the next vcpu_resume_commit call to prevent spurious
     * Irq events from being reported.
     *
     * Note: Although there are no known issues, it is possible this workaround
     * interferes with some other L4 applications that expect a zeroed out tag
     * label or which do not clear the label.
     *
     */
    if ((_vcpu_state->i.tag.label() == L4_PROTO_IRQ)
            && (!l4_msgtag_has_error(_vcpu_state->i.tag)))
    {
        karma_log(TRACE, "handle upcall i.label %d exit_reason=%s:%x\n",
                (int)_vcpu_state->i.label, L4_vm_exit_reason_to_str(e), e);

        // clear L4_PROTO_IRQ from tag now that we've received the irq
        _vcpu_state->i.tag = l4_msgtag(0,0,0,0);

        // process this as an IRQ received via ipc
        e = L4_vm_exit_reason_ipc; // trapno = 0xfe
    }
#endif
    if (e == L4_vm_exit_reason_ipc) { // trapno == 0xfe
        int irq = _vcpu_state->i.label;
        if ((irq >= 0) && (irq < max_interrupts))
            _interrupts[irq]->handle();
        finalizeIrq();
    }
    else if (e == L4_vm_exit_reason_irq) // trapno == 0xfeef
        handle_vmexit();
    else
        karma_log(ERROR, "Upcall not handled exit_reason=%s:%x\n", L4_vm_exit_reason_to_str(e), e);

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
    l4_msgtag_t msg;
    long result;

    _vcpu_state->user_task = user_task.cap();
#if defined(MIPS_FPU_NOT_YET)
    _vcpu_state->saved_state |= L4_VCPU_F_USER_MODE;
#else
    _vcpu_state->saved_state |= L4_VCPU_F_FPU_ENABLED | L4_VCPU_F_USER_MODE;
#endif

    msg = _vcpu_thread->vcpu_resume_start(vcpu_utcb());
    msg = _vcpu_thread->vcpu_resume_commit(msg, vcpu_utcb());
    result = l4_error_u(msg, vcpu_utcb());
    if(result == 0)
    {
        setTrapNo(static_cast<l4_umword_t>(L4_vm_exit_reason_irq)); // trapno = 0xfeef
        _simulate_upcall(_vcpu_state->entry_sp);
    } 
    else if(result > 0)
    {
        // we got ipc, receive ipc and simulate upcall
        _vcpu_state->i.tag = l4_ipc_wait(vcpu_utcb(), &_vcpu_state->i.label, L4_IPC_NEVER);
        if (!l4_msgtag_has_error(_vcpu_state->i.tag))
        {
            setTrapNo(static_cast<l4_umword_t>(L4_vm_exit_reason_ipc)); // trapno = 0xfe
            _simulate_upcall(_vcpu_state->entry_sp);
        }
    }

    L4_vcpu::disableIrqs();
    karma_log(ERROR, "an error occurred during resume operation. Terminating.\n");
    // KYMAXXX TODO this exit path causes kernel panic... l4_task_unmap?
    exit(1);
}

void L4_vcpu::disableIrqs()
{
    _vcpu_state->state &= ~L4_VCPU_F_IRQ;
    l4_barrier();
}

void L4_vcpu::enableIrqs()
{
    while (1)
    {
        _vcpu_state->state |= L4_VCPU_F_IRQ;
        l4_barrier();

        if (!(_vcpu_state->sticky_flags & L4_VCPU_SF_IRQ_PENDING))
            break;

        disableIrqs();
        _vcpu_state->i.tag = l4_ipc_wait(vcpu_utcb(), &_vcpu_state->i.label, L4_IPC_NEVER);
        if (!l4_msgtag_has_error(_vcpu_state->i.tag))
            _interrupts[_vcpu_state->i.label]->handle();
    }
}

void L4_vcpu::halt()
{
    disableIrqs();
    _vcpu_state->i.tag = l4_ipc_wait(vcpu_utcb(), &_vcpu_state->i.label, L4_IPC_NEVER);
    if (!l4_msgtag_has_error(_vcpu_state->i.tag))
        _interrupts[_vcpu_state->i.label]->handle();
    enableIrqs();
}

// super_halt is run in disabled mode
void L4_vcpu::super_halt()
{
    _vcpu_state->i.tag = l4_ipc_wait(vcpu_utcb(), &_vcpu_state->i.label, L4_IPC_NEVER);
    if (!l4_msgtag_has_error(_vcpu_state->i.tag))
    {
        setTrapNo(static_cast<l4_umword_t>(L4_vm_exit_reason_ipc)); // trapno = 0xfe
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

