/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * main.cc - TODO enter description
 * 
 * (c) 2011 Divij Gupta <>
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Matthias Lange <mlange@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include <l4/util/util.h>
#include <l4/util/cpu.h>

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/protocols>
#include <l4/sys/irq>
#include <l4/sys/debugger.h>
#include <l4/cxx/exceptions>
#include <l4/cxx/iostream>
#include <l4/cxx/l4iostream>
#include <pthread-l4.h>

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "l4_vm.h"
#include "params.h"

#include "util/debug.h"

using namespace std;
struct __params params;
int cpu_option;
L4_vm *__l4_vm;
int debug_level = INFO;

static struct option long_options[] =
{
    {"fb", 0, 0, 'f'},
    {"net", 0, 0, 'n'},
    {"pci", 0, 0, 'P'},
    {"sleep", 1, 0, 's'},
    {"mem", 1, 0, 'm'},
    {"vcpu_prio", 1, 0, 'p'},
    {"virtual_cpus", 1, 0, 'c'},
    {"affinity_mask", 1, 0, 'a'},
    {"kernel_name", 1, 0, 'N'},
    {"kernel_opts", 1, 0, 'o'},
    {"ramdisk_name", 1, 0, 'r'},
    {"bd_image", 1, 0, 'b'},
    {"debug_level", 1, 0, 'd'},
    {"shm_prod", required_argument, 0, '0'},
    {"shm_cons", required_argument, 0, '1'},
    {"paravirt_guest", 0, 0, 'g'},
    {0, 0, 0, 0}
};

static void backtrace(void)
{
    class foo:public L4::Exception_tracer{};
    foo tracer;
    for (int i = 0; i < tracer.frame_count(); ++i)
        printf("%08x\n", (unsigned int)l4_addr_t(tracer.pc_array()[i]));
    exit(1);
}

inline bool has_hw_cpu_virtualization()
{
    // KYMA TODO implement has_hw_cpu_virtualization for mips
    return true;
}

int main(int argc, char *argv[])
{
    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "Karma");
    unsigned int mem_size;
    int opt_index, c;

    // default values
    mem_size  = 128*1024*1024;
    params.virtual_cpus = 1;
    params.affinity_mask = 0;
    params.fb = false;
    params.pci = false;
    params.vcpu_prio = 0;
    params.debug_level = debug_level;
    params.kernel_name = "";
    params.ramdisk_name = "";
    params.kernel_opts = "console=ttyLv0\0";
    params.paravirt_guest = false;
    
    // use this to debug karma (enables meaningful backtraces)
    std::set_terminate(backtrace);

    karma_log(INFO, "Welcome to Karma, build at %s %s with GCC %d.%d.%d\n", __DATE__, __TIME__,
        __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);

    // get parameters
    opt_index = 0;
    while( (c = getopt_long(argc, argv, "fnPs:m:p:c:a:N:o:r:b:d:g", long_options, &opt_index)) != -1 )
    {
        switch (c)
        {
            case 'f':
                params.fb = true;
                break;
            case 'P':
                params.pci = true;
                break;
            case 'c':
                params.virtual_cpus = strtoul(optarg, 0, 0);
                karma_log(INFO, "[options] requested %d virtual CPUs\n", params.virtual_cpus);
                break;
            case 'a':
                params.affinity_mask = strtoul(optarg, 0, 0);
                karma_log(INFO, "[options] requested CPU affinity mask 0x%x\n", params.affinity_mask);
                break;
            case 's':
                sleep(strtoul(optarg, 0, 0));
                break;
            case 'm':
                mem_size = strtoul(optarg, 0, 0)*1024*1024;
                break;
            case 'N':
                params.kernel_name = optarg;
                break;
            case 'o':
                params.kernel_opts = optarg;
                break;
            case 'r':
                params.ramdisk_name = optarg;
                break;
            case 'b':
                params.bd_image_name = optarg;
                karma_log(INFO, "[options] bd_image_name=%s\n", params.bd_image_name);
                break;
            case 'p':
                params.vcpu_prio = strtoul(optarg, 0, 0);
                break;
            case 'd':
                debug_level = params.debug_level = strtoul(optarg, 0, 0);
                break;
            case '0':
                params.shm_prod = optarg;
                break;
            case '1':
                params.shm_cons = optarg;
                break;
            case 'g':
                params.paravirt_guest = true;
                break;
            default:
                break;
        }
    }

    if(params.pci)
        karma_log(INFO, "[options] PCI enabled\n");

    // Kyma TODO handle multiple instances of shm_prod/shm_cons
    if(params.shm_prod)
        karma_log(INFO, "[options] L4 shm producer name: \'%s\'\n", params.shm_prod);

    if(params.shm_cons)
        karma_log(INFO, "[options] L4 shm consumer name: \'%s\'\n", params.shm_cons);

    if(params.kernel_name)
        karma_log(INFO, "[options] Using kernel_name \'%s\'\n", params.kernel_name);

    if(params.ramdisk_name)
        karma_log(INFO, "[options] Using ramdisk_name \'%s\'\n", params.ramdisk_name);

    if(params.paravirt_guest)
        karma_log(INFO, "[options] Using paravirt_guest boot prom\n");

    if(!has_hw_cpu_virtualization())
    {
        karma_log(ERROR, "No HW CPU virtualization capabilities detected. Terminating.\n");
        exit(1);
    }

    try
    {
        __l4_vm = &GET_VM;
        __l4_vm->init(params.virtual_cpus, mem_size, params.affinity_mask);
    }
    catch(L4_exception &e)
    {
        e.print();
    }

    // let the main thread sleep forever!
    l4_sleep_forever();
    return 0;
}

