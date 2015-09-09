/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * l4_vm.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Matthias Lange <mlange@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "l4_vm.h"

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/thread>

#include <malloc.h>

#include "l4_exceptions.h"
#include "l4_mem.h"
#include "util/debug.h"
#include "devices/device_init.hpp"
#include "devices/device_call.hpp"
#include "devices/devices_list.h"
#include "devices/shmem.h"
#include "irqchip.h"

L4_vm::L4_vm() : _l4_cpu(), _l4_cpu_threads(), _l4_os(0), _gic(0), _l4_mem(0),
    _online_cpu_map(0), _affinity_mask(0), _cpu_affinity(0),
    _nr_possible_cpus(0), _nr_online_cpus(0), _nr_available_cpus(0),
    old_timestamp(0), _old_ns(0), _old_s(0) {}

L4_vm::~L4_vm() {}

void L4_vm::init(unsigned int nr_cpus, unsigned int mem_size, l4_umword_t affinity_mask)
{
    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "L4 VM");

    if(discover_cpus(nr_cpus, affinity_mask))
    {
        karma_log(ERROR, "Could not query online CPUs. Terminating.\n");
        exit(1);
    }

#ifdef KARMA_USE_VM_AFFINITY
    l4_sched_param_t schedp = l4_sched_param(0);
    get_vm_affinity(&schedp.affinity);
    L4Re::Env::env()->scheduler()->run_thread(L4::Cap<L4::Thread>(L4_BASE_THREAD_CAP), schedp);
    L4Re::Env::env()->scheduler()->run_thread(L4::Cap<L4::Thread>(pthread_getl4cap(pthread_self())), schedp);
#endif

    // get vm capability
    _vm_cap = L4Re::Util::cap_alloc.alloc<L4::Vm>();
    if(!_vm_cap.is_valid())
    {
        karma_log(ERROR, "Could not allocate VM capability. Terminating.\n");
        exit(1);
    }

    l4_msgtag_t msg = L4Re::Env::env()->factory()->create_vm(_vm_cap);
    if(l4_error(msg))
    {
        karma_log(ERROR, "Could not get a VM cap from Fiasco.OC.\n"
                "Please ensure your platform supports hardware assisted virtualization (MIPS VZ)\n"
                "and that it is enabled for your platform in the kernel configuration.\n"
                "Terminating.\n");
        exit(1);
    }

    l4_debugger_set_object_name(_vm_cap.cap(), "L4 VM usertask");

    _l4_mem = &util::Singleton<L4_mem>::get();
    _l4_mem->init(mem_size, _vm_cap);

    _l4_os = L4_os::create_os(_l4_mem);

    _gic = &util::Singleton<Gic>::get();
    _gic->init();

    if (params.paravirt_guest)
        mipsvz_create_irqchip();

    GLOBAL_DEVICES_INIT;

    GET_HYPERCALL_DEVICE(mem).init();
    GET_HYPERCALL_DEVICE(gic).init(_gic);
    GET_HYPERCALL_DEVICE(ser).init(karma_irq_ser);
    GET_HYPERCALL_DEVICE(timer).init(karma_irq_timer);

	if(params.bd_image_name)
	{
		karma_log(DEBUG, "Load bdds device\n");
		GET_HYPERCALL_DEVICE(bdds).setImageName(params.bd_image_name);
	}

    if(params.fb)
    {
        try
        {
            GET_HYPERCALL_DEVICE(fb).setIntr(karma_irq_fb);
        }
        catch(L4_exception &e)
        {
            e.print();
        }
    }
    if(params.pci)
    {
        try
        {
            GET_HYPERCALL_DEVICE(pci).init();
        }
        catch(L4_exception &e)
        {
            e.print();
        }
    }
    // Kyma TODO handle multiple instances of shm_prod/shm_cons
    if(params.shm_prod)
    {
        try
        {
            GET_HYPERCALL_DEVICE(shm_prod).init(params.shm_prod);
        }
        catch(L4_exception &e)
        {
            e.print();
        }
    }
    if(params.shm_cons)
    {
        try
        {
            GET_HYPERCALL_DEVICE(shm_cons).init(params.shm_cons);
        }
        catch(L4_exception &e)
        {
            e.print();
        }
    }

#if 0 // KYMA TODO implement IPI support
    // cpu bus for IPI
    _l4_cpu_bus = new L4_cpu_bus();
#endif

    allocate_cpus();
    spawn_cpus();
}

void L4_vm::dump_online_cpus()
{
    printf("Found %u CPUs.\n", nr_possible_cpus());

    for (unsigned int i = 0; i < nr_possible_cpus(); i++)
    {
        printf("CPU %d is %s.\n", i,
                (L4Re::Env::env()->scheduler()->is_online(i)) ?
                "online" : "offline");
    }
}

/*
 * Query the system to determine how many and which cpus are online.
 *
 * NOTE: The available cpus are numbered contiguously, whereas the online
 * cpus may not be contiguous if some cpus are offline.
 */
int L4_vm::discover_cpus(unsigned int requested_cpus, l4_umword_t affinity_mask)
{
    l4_umword_t nr = 0;
    l4_sched_cpu_set_t cs = l4_sched_cpu_set(0, 0);

    if (l4_error(L4Re::Env::env()->scheduler()->info(&nr, &cs)) < 0)
        return -1;

    _online_cpu_map = cs.map;
    _nr_possible_cpus = nr;
    _nr_online_cpus = __builtin_popcount(cs.map);

    if (requested_cpus < 1)
        requested_cpus = 1;

    _nr_available_cpus = std::min(_nr_online_cpus, MAX_CPUS);
    _nr_available_cpus = std::min(_nr_available_cpus, requested_cpus);

    // limit the affinity mask to only online cpus (mask can be zero)
    _affinity_mask = affinity_mask & _online_cpu_map;

    // set base cpu affinity in case no affinity mask is specified
    _cpu_affinity = get_nth_cpu(0, _online_cpu_map);

    if (nr_possible_cpus() != nr_available_cpus()) {
        karma_log(INFO, "CPUs possible:  %d\n", nr_possible_cpus());
        karma_log(INFO, "CPUs online:    %d\n", nr_online_cpus());
    }
    karma_log(INFO, "Virtual CPUs available: %d\n", nr_available_cpus());

    if (_affinity_mask)
        karma_log(INFO, "Using CPU affinity mask 0x%x\n", (unsigned int)_affinity_mask);
    else
        karma_log(INFO, "Using default CPU affinity\n");

    return 0;
}

/*
 * This function returns the index of the nth cpu in the cpu bitmap, skipping
 * any gaps in the cpu map, and wrapping around if n > number cpus in the map.
 *
 * n is 0-based.  n will be capped to modulo nr cpus
 * map represents the cpu bitmap
 *
 * returns the 0-based index of the nth cpu, -1 if there is any error.
 */
int L4_vm::get_nth_cpu(unsigned int n, l4_umword_t cpu_map)
{
    if (!cpu_map)
        return -1;

    n = n % __builtin_popcount(cpu_map);

    // remove the n-1 lowest bits which are set
    for (unsigned int i = 0; i < n; i++) {
        // the (v & -v) trick uses 2's complement math to preserve the lowest
        // set bit; we then clear that bit
        cpu_map ^= (cpu_map & -cpu_map);
    }
    return __builtin_ffs(cpu_map) - 1;
}

/*
 * When the affinity_mask specifies at least one online cpu the vcpu affinities
 * are selected from this mask, otherwise a default affinity is chosen.
 */
void L4_vm::allocate_cpus()
{
    if (LOG_LEVEL(DEBUG))
        dump_online_cpus();

    for (unsigned int i = 0; i < nr_available_cpus(); ++i) {
        l4_umword_t cpu_map = _affinity_mask ? _affinity_mask : _online_cpu_map;
        unsigned int cpu_affinity = get_nth_cpu(i, cpu_map);

        _l4_cpu[i] = new L4_cpu(i, _l4_os);
        _l4_cpu[i]->set_vcpu_affinity(cpu_affinity);
        _l4_cpu[i]->prepare();
    }
}

void L4_vm::spawn_cpus()
{
    for (unsigned int i = 0; i < nr_available_cpus(); ++i)
    {
        spawn_cpu(i, os().get_guest_entrypoint(), 0);
    }
}

void L4_vm::get_vm_affinity(l4_sched_cpu_set_t *affinity)
{
    if (_affinity_mask)
        *affinity = l4_sched_cpu_set(0, 0, _affinity_mask);
    else
        *affinity = l4_sched_cpu_set(_cpu_affinity, 0, 0x1);
}

/*
 * cpuid is zero based cpu index
 */
void L4_vm::spawn_cpu(l4_uint32_t cpuid, l4_umword_t eip, l4_umword_t sp)
{
    karma_log(DEBUG, "spawn_cpu: cpuid=%d, eip = %lx, sp = %lx\n", cpuid, eip, sp);

    if (!valid_cpuid_index(cpuid)) {
        karma_log(WARN, "spawn_cpu: cpuid=%d out of range\n", cpuid);
        return;
    }

    if (_l4_cpu[cpuid]->spawned()) {
        karma_log(INFO, "spawn_cpu: cpuid=%d already spawned\n", cpuid);
        return;
    }

    _l4_cpu[cpuid]->set_spawned();

    if(eip)
        _l4_cpu[cpuid]->set_ip_and_sp(eip, sp);

    _l4_cpu[cpuid]->get_thread(_l4_cpu_threads[cpuid]);
    if(cpuid == 0)
        _gic->set_cpu(*_l4_cpu[cpuid]);

    _l4_cpu[cpuid]->start();
}

void L4_vm::dump_state()
{
    printf("Dumping the state of all CPUs:\n");
    for(unsigned int i = 0; i < nr_available_cpus(); ++i)
        _l4_cpu[i]->dump_state();
}

void L4_vm::print_time()
{
#if 0 // KYMA TODO implement print_time
    l4_uint32_t s, ns, seconds = 0, nseconds = 0;
    l4_tsc_to_s_and_ns(l4_rdtsc(), &s, &ns);
    if(_old_s!=0)
    {
        seconds = _old_s;
        nseconds = _old_ns;
        seconds = s - seconds;
        nseconds = ns - nseconds;
        printf("L4_vm: time diff %2d: %dm %2ds %3dms\n",
            _old_timestamp, seconds/60, seconds%60, nseconds/1000000);
    }
    _old_timestamp++;
    _old_s = s;
    _old_ns = ns;
#endif
}

