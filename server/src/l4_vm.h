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
#include "l4_cpu_bus.h"
#include "devices/gic.h"

class L4_os;
class Gic;
class L4_timer;
class L4_ser;
class L4_fb;
class L4_net;
class L4_pci;
class L4_ahci;
class L4_mem;
class L4_hpet;
class L4_cpu;

class L4_vm
{
public:
    L4_vm();
    ~L4_vm() {}
    void init(unsigned int nr_cpus = 1, unsigned int mem_size = 128*1024*1024);
    void spawn_cpu(l4_uint32_t apicid, l4_umword_t eip, l4_umword_t sp);
    void dump_state();
    void dump_exit_reasons();
    void print_time();
private:
    void get_cpus();
    void setup_mp_table();

    L4_cpu *_l4_cpu[MAX_CPUS];
    L4::Cap<L4::Thread> _l4_cpu_threads[MAX_CPUS];

    L4::Cap<L4::Vm> _vm_cap; // _kvm_vm_cap;

    L4_os *_l4_os;
    Gic *_gic;
    L4_mem *_l4_mem;
    L4_cpu_bus *_l4_cpu_bus;
    unsigned int _nr_cpus, _requested_cpus;
    unsigned int _present_cpus;
    l4_uint32_t old_timestamp, _old_ns, _old_s;

public:
    inline L4_mem & mem() { return *_l4_mem; }
    inline L4_os & os() { return * _l4_os; }
    L4::Cap<L4::Vm> & vm_cap() { return _vm_cap; }
    L4_cpu_bus & cpu_bus() { return *_l4_cpu_bus; }
    Gic & gic() { return *_gic; }
};

#define GET_VM util::Singleton<L4_vm>::get()

