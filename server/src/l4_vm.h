/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * l4_vm.h - TODO enter description
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
#pragma once


#include "util/singleton.hpp"
#include "devices/gic.h"
#include "l4_mem.h"
#include "l4_os.h"
#include "l4_cpu.h"

#define MAX_CPUS 8U

// Use VM cpu affinity for other threads
#define KARMA_USE_VM_AFFINITY

class L4_vm
{
public:
    L4_vm();
    virtual ~L4_vm();
private:
    L4_vm(const L4_vm& rhs);            // empty copy constructor
    L4_vm& operator=(const L4_vm& rhs); // empty assignment constructor
public:
    void init(unsigned int nr_cpus = 1, unsigned int mem_size = 128*1024*1024, l4_umword_t affinity_mask = 0x1);
    void spawn_cpu(l4_uint32_t cpuid, l4_umword_t eip, l4_umword_t sp);
    void dump_state();
    void dump_online_cpus();
    void print_time();
private:
    int discover_cpus(unsigned int requested_cpus, l4_umword_t affinity_mask);
    int get_nth_cpu(unsigned int n, l4_umword_t cpu_map);
    void allocate_cpus();
    void spawn_cpus();
    void setup_mp_table();

    L4_cpu *_l4_cpu[MAX_CPUS];
    L4::Cap<L4::Thread> _l4_cpu_threads[MAX_CPUS];

    L4::Cap<L4::Vm> _vm_cap;

    L4_os *_l4_os;
    Gic *_gic;
    L4_mem *_l4_mem;
    l4_umword_t _online_cpu_map;
    l4_umword_t _affinity_mask;
    unsigned int _cpu_affinity;
    unsigned int _nr_possible_cpus, _nr_online_cpus, _nr_available_cpus;
    l4_uint32_t old_timestamp, _old_ns, _old_s;

public:
    inline L4_mem & mem() { return *_l4_mem; }
    inline L4_os & os() { return * _l4_os; }
    L4::Cap<L4::Vm> & vm_cap() { return _vm_cap; }
    L4_cpu &cpu(unsigned int cpuid) { return *_l4_cpu[cpuid]; }
    Gic & gic() { return *_gic; }

    inline unsigned int nr_possible_cpus() { return _nr_possible_cpus; }
    inline unsigned int nr_online_cpus() { return _nr_online_cpus; }
    inline unsigned int nr_available_cpus() { return _nr_available_cpus; }
    inline bool valid_cpuid_index(unsigned int cpuid) { return cpuid < nr_available_cpus(); }
    void get_vm_affinity(l4_sched_cpu_set_t *affinity);
};

#define GET_VM util::Singleton<L4_vm>::get()

