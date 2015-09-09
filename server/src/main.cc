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
// keep track of io ports
l4_uint32_t ioport_map[(1<<16)/sizeof(l4_uint32_t)];
L4_vm *__l4_vm;

static struct option long_options[] =
{
    {"fb", 0, 0, 'f'},
    {"net", 0, 0, 'n'},
    {"pci", 0, 0, 'P'},
    {"sleep", 1, 0, 's'},
    {"mem", 1, 0, 'm'},
    {"ahci",0,0,'h'},
	{"hpet", 0, 0, 't'},
	{"vcpu_prio", 1, 0, 'p'},
    {"virtual_cpus", 1, 0, 'c'},
    {"ankh_shm", 1, 0, 'A'},
    {"kernel_name", 1, 0, 'N'},
    {"kernel_opts", 1, 0, 'o'},
    {"ramdisk_name", 1, 0, 'r'},
	{"bd_image", 1, 0, 'b'},
    {"debug_level", 1, 0, 'd'},
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

inline int determine_cpu_type(void)
{
  l4_umword_t ax;
    char vendor[13]; vendor[12] = 0;

    if (l4util_cpu_has_cpuid())
      l4util_cpu_cpuid(0, &ax, (l4_umword_t*)&vendor[0], (l4_umword_t*)&vendor[8], (l4_umword_t*)&vendor[4]);
    else
      return Unknown_cpu;

    if(!strcmp(vendor, "GenuineIntel"))
        return Intel_cpu;
    else if(!strcmp(vendor, "AuthenticAMD"))
        return AMD_cpu;
    else
        return Unknown_cpu;
}

inline bool has_hw_cpu_virtualization(int cpu_type)
{
    l4_umword_t eax, ebx, ecx, edx;

    if (!l4util_cpu_has_cpuid())
      return false;

    if(cpu_type == AMD_cpu) //amd cpu
    {
        l4util_cpu_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
        if(ecx & 0x4)
            return true;
    }
    else if(cpu_type == Intel_cpu) //intel cpu
    {
        l4util_cpu_cpuid(0x1, &eax, &ebx, &ecx, &edx);
        if(ecx & 0x20)
            return true;
    }
    return false;
}

inline bool has_hw_memory_virtualization(int cpu_type)
{
   l4_umword_t eax, ebx, ecx, edx;

    if (!l4util_cpu_has_cpuid())
      return false;

    if(cpu_type == AMD_cpu) // check for nested paging capability on AMD only. Intel ePT is not yet supported
    {
        l4util_cpu_cpuid(0x8000000a, &eax, &ebx, &ecx, &edx);
        if(edx & 1) // nested paging supported
            return true;
        else
            return false;
    }
    // intel cpu, vtlb required
    return false;
}

int main(int argc, char *argv[])
{
    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "Karma");
    unsigned int mem_size;
    int opt_index, c;

    // default values
    mem_size  = 128*1024*1024;
    params.virtual_cpus = 1;
    params.fb = false;
    params.net = false;
    params.pci = false;
    params.vtlb = false;
    params.hpet = false;
    params.vcpu_prio = 0;
    params.debug_level = 3;
    params.kernel_name = "bzImage";
    params.ramdisk_name = "ramdisk";
    params.kernel_opts = (char *)\
                         "root=/dev/ram rw "\
                         "lapic "\
                         "video=l4fb:800x600@16,refreshsleep:100 "\
                         "noacpi "\
                         "noapm "\
                         "time "\
                         "clocksource=tsc "\
                         "console=ttyLv0 "\
                         "tsc=reliable "\
                         "force_tsc_stable "\
                         "no_console_suspend "\
                         "nosep "\
                         "init=/sbin/init\0";
    // apic=debug no-hlt clocksource=jiffies no_ipi_broadcast=1 mem=256M tsc=reliable console=ttyLv1 pci=off pause_on_oops=5 selinux=0 quiet nosmp mem=nopentium
    
    // use this to debug karma (enables meaningful backtraces)
    std::set_terminate(backtrace);

    karma_log(INFO, "Welcome to Karma, build at %s %s with GCC %d.%d.%d\n", __DATE__, __TIME__,
        __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);

    // get parameters
	opt_index = 0;
    while( (c = getopt_long(argc, argv, "fnPs:m:htp:c:A:T:N:o:r:b:", long_options, &opt_index)) != -1 )
    {
        switch (c)
        {
            case 'f':
                params.fb = true;
                break;
            case 'h':
                params.ahci = true;
                break;
            case 'n':
                params.net = true;
                break;
            case 'P':
                params.pci = true;
                break;
            case 'c':
                params.virtual_cpus = atoi(optarg);
                karma_log(INFO, "Requested %d virtual CPUs\n", params.virtual_cpus);
                break;
            case 's':
                sleep(atoi(optarg));
                break;
            case 'm':
                mem_size = atoi(optarg)*1024*1024;
                break;
            case 'A':
                // ankh shm
                params.ankh_shm = optarg;
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
            case 't':
                params.hpet = true;
                break;
            case 'b':
                params.bd_image_name = optarg;
	            karma_log(INFO, "bd_image_name=%s\n", params.bd_image_name);
                break;
            case 'p':
                params.vcpu_prio = atoi(optarg);
		break;
            case 'd':
                params.debug_level = atoi(optarg);
		break;
            default:
                break;
        }
    }

    if(params.net)
        karma_log(INFO, "Networking enabled\n");

    if(params.pci)
        karma_log(INFO, "PCI enabled\n");

    if(params.kernel_name)
        karma_log(INFO, "Using linux kernel \'%s\'\n", params.kernel_name);

    if(params.ramdisk_name)
        karma_log(INFO, "Using linux ramdisk \'%s\'\n", params.ramdisk_name);

    cpu_option = determine_cpu_type();
    switch(cpu_option)
    {
        case AMD_cpu:
            karma_log(INFO, "AMD CPU detected\n");
            break;
        case Intel_cpu:
            karma_log(INFO, "Intel CPU detected\n");
            break;
        default:
            karma_log(ERROR, "Unknown CPU detected. Terminating.\n");
            exit(1);
    }
    params.vtlb = ! has_hw_memory_virtualization(cpu_option);
    if(params.vtlb)
        karma_log(INFO, "Using vtlb.\n");
    else
        karma_log(INFO, "Using hardware memory virtualization.\n");

    if(!has_hw_cpu_virtualization(cpu_option))
    {
        karma_log(ERROR, "No HW CPU virtualization capabilities detected. Terminating.\n");
        exit(1);
    }

	try
	{
		__l4_vm = &GET_VM;
		__l4_vm->init(params.virtual_cpus, mem_size);
	}
	catch(L4_exception &e)
	{
		e.print();
	}

    // let the main thread sleep forever!
    l4_sleep_forever();
    return 0;
}

