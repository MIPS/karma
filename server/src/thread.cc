/*
 * thread.cc - TODO enter description
 *
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * thread.cc
 *
 *  Created on: 24.09.2010
 *      Author: janis
 */

#include "thread.h"
#include "l4_vm.h"

namespace karma{

    void Thread::_start_thread(unsigned int cpu)
    {
        pthread_attr_t thread_attr;

        int err;
        if ((err = pthread_attr_init(&thread_attr)) != 0)
            printf("error initializing pthread attr: %d", err);

#ifdef KARMA_USE_VM_AFFINITY
        // Set VM affinity to be inherited by other threads. Note that
        // pthread_manager thread remains on cpu 0 (see
        // uclibc/lib/libpthread/src/manager.cc:__pthread_start_manager)
        GET_VM.get_vm_affinity(&thread_attr.affinity);
        (void)cpu;
#endif

        err = pthread_create(&_pthread, &thread_attr, __thread_func, this);
        if(err)
        {
            printf("error creating thread err=%d\n", err);
            printf("Can not recover from the last error. About to fail!\n");
            printf("Try assigning more threads to karma in your lua config using parameter \"max_threads = <#threads>\"\n");
            exit(1);
        }
        l4_debugger_set_object_name(pthread_getl4cap(_pthread), _name.c_str());
        l4_sched_param_t l4sp = l4_sched_param(_prio);
#ifndef KARMA_USE_VM_AFFINITY
        if(!L4Re::Env::env()->scheduler()->is_online(cpu))
            cpu = 0;
        l4sp.affinity = l4_sched_cpu_set(cpu, 0);
#endif
        if (l4_error(L4Re::Env::env()->scheduler()->run_thread((L4::Cap<L4::Thread>)pthread_getl4cap(_pthread), l4sp)))
            printf("Irq_thread error setting priority\n");

    }

} // namespace karma
