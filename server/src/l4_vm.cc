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
#include <l4/util/rdtsc.h>

#include <malloc.h>

#include "mpspec_def.h"
#include "l4_exceptions.h"
#include "l4_mem.h"
#include "util/debug.h"
#include "guest_linux.hpp"
#include "devices/device_init.hpp"
#include "devices/device_call.hpp"
#include "devices/devices_list.h"

L4_vm::L4_vm(){}

void L4_vm::init(unsigned int nr_cpus, unsigned int mem_size)
{
    _requested_cpus = nr_cpus;
    _old_ns = 0;
    _old_s = 0;
    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "L4 VM");

    // get vm capability
    _vm_cap = L4Re::Util::cap_alloc.alloc<L4::Vm>();
    if(!_vm_cap.is_valid())
    {
        karma_log(ERROR, "Could not allocate VM capability. Terminating.\n");
        exit(1);
    }

#if 0
    _kvm_vm_cap = L4Re::Util::cap_alloc.alloc<L4::Vm>();
    if(!_kvm_vm_cap.is_valid())
    {
        karma_log(ERROR, "Could not allocate VM capability for staged virtualization. Terminating.\n");
        exit(1);
    }
#endif

    l4_msgtag_t msg = L4Re::Env::env()->factory()->create_vm(_vm_cap);
    if(l4_error(msg))
    {
        karma_log(ERROR, "Could not get a VM cap from Fiasco.OC.\nPlease ensure your platform supports hardware assisted virtualization (either SVM or VT).\nTerminating.\n");
        exit(1);
    }
#if 0
    msg = L4Re::Env::env()->factory()->create_vm(_kvm_vm_cap);
    if(l4_error(msg))
    {
        karma_log(ERROR, "Could not get a VM cap from Fiasco.OC.\nPlease ensure your platform supports hardware assisted virtualization (either SVM or VT).\nTerminating.\n");
        exit(1);
    }
#endif
    GLOBAL_DEVICES_INIT;

    // prepare memory (proper setup will be done by l4_os)
    _l4_mem = &GET_HYPERCALL_DEVICE(mem);

    _l4_os = new L4_linux(mem_size, _l4_mem, _vm_cap);

    // prepare (global) devices
    _gic = &GET_HYPERCALL_DEVICE(gic);
    _gic->init();
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
    if(params.net)
    {
        try
        {
            GET_HYPERCALL_DEVICE(net).init(karma_irq_net);
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
            GET_HYPERCALL_DEVICE(ahci).setIntr(karma_irq_pci);
            GET_HYPERCALL_DEVICE(pci).init();
        }
        catch(L4_exception &e)
        {
            e.print();
        }
    }

	GET_HYPERCALL_DEVICE(ioport).init();
    
    // cpu bus for IPI
    _l4_cpu_bus = new L4_cpu_bus();

    // look for cpus
    get_cpus();
    if(_requested_cpus > _nr_cpus)
        _requested_cpus = _nr_cpus;

    // setup mp bios data
    setup_mp_table();

    old_timestamp = 0;

    _present_cpus = 0;
    for(unsigned int i(0); i != _requested_cpus; ++i){
        _l4_cpu[i] = new L4_cpu(i, _l4_os);
        _l4_cpu[i]->prepare();
    }

    // spawn bootup cpu
    spawn_cpu(0, 0, 0);
}

void L4_vm::get_cpus() {
    l4_umword_t nr = 0;
    _nr_cpus = 0;
    l4_sched_cpu_set_t cs = l4_sched_cpu_set(0, 0);
    if (l4_error(L4Re::Env::env()->scheduler()->info(&nr, &cs)) < 0)
        return;
    karma_log(INFO, "Found %d CPUs.\n", (int)nr);
    for(unsigned int i=0; i<=4; i++)
    {
        karma_log(TRACE, "Testing CPU %d: ", i);
        if(L4Re::Env::env()->scheduler()->is_online(i))
        {
            _log(TRACE, "online.\n");
            _nr_cpus++;
        }
        else
        {
            _log(TRACE, "offline.\n");
        }
    }
    karma_log(INFO, "There are %d online CPUs in the system.\n", _nr_cpus);
}

inline l4_uint8_t mk_checksum(unsigned char *_ptr, int _len)
{
    // compute the checksum 
    // the sum of all bytes has to be zero
    l4_uint8_t chk = 0;
    int len = _len;
    unsigned char *ptr = _ptr;
    while(len--)
        chk += *ptr++;
    return 0xff-chk+1;
}

// setup in the virtual address space of Karma
void L4_vm::setup_mp_table()
{
    // we store the intel_mp_floating struct into the bios memory area
    struct intel_mp_floating *imf = (struct intel_mp_floating*)(_l4_mem->base_ptr() + 0xf0000);
    imf->mpf_signature[0] = '_';
    imf->mpf_signature[1] = 'M';
    imf->mpf_signature[2] = 'P';
    imf->mpf_signature[3] = '_';
    imf->mpf_physptr = 0xf1000;
    imf->mpf_specification = 0x4; // 1.4
    imf->mpf_length = 1;
    imf->mpf_feature1 = 0;
    //imf->mpf_feature2 = 1<<7;


    imf->mpf_checksum = mk_checksum((unsigned char*)imf, 16);

    // construct mpc_table
    struct mpc_table *tbl = (struct mpc_table*)(_l4_mem->base_ptr()+ 0xf1000);
    tbl->signature[0] = 'P';
    tbl->signature[1] = 'C';
    tbl->signature[2] = 'M';
    tbl->signature[3] = 'P';
    tbl->length  = sizeof(struct mpc_table) + _requested_cpus*sizeof(struct mpc_cpu);
    tbl->spec = 0x4;
    tbl->oem[0] = 'S';
    tbl->oem[1] = 't';
    tbl->oem[2] = 'e';
    tbl->oem[3] = 'f';
    tbl->oem[4] = 'f';
    tbl->oem[5] = 'e';
    tbl->oem[6] = 'n';
    tbl->oem[7] = 0;
    tbl->productid[0] = 'T';
    tbl->productid[1] = 'U';
    tbl->productid[2] = 'D';
    tbl->productid[3] = 'R';
    tbl->productid[4] = 'E';
    tbl->productid[5] = 'S';
    tbl->productid[6] = 'D';
    tbl->productid[7] = '.';
    tbl->productid[8] = 0;
    tbl->oemptr = 0;
    tbl->oemcount = 0;
    tbl->lapic = 0xfffe0000; // standard apic
    tbl->checksum = 0;

    // add cpus
    l4_addr_t tmp = (l4_addr_t)tbl;
    tmp += sizeof(struct mpc_table);
    struct mpc_cpu *cpu = (struct mpc_cpu*)tmp;
    cpu->type = MP_PROCESSOR;
    cpu->apicid = 0;
    cpu->apicver = 0x1f;
    cpu->cpuflag = CPU_ENABLED | CPU_BOOTPROCESSOR;

    for(unsigned int i = 1; i<_requested_cpus; i++)
    {
        cpu = (struct mpc_cpu*)(tmp + i*sizeof(struct mpc_cpu));
        cpu->type = MP_PROCESSOR;
        cpu->apicid = i;
        cpu->apicver = 0x1f;
        cpu->cpuflag = CPU_ENABLED;
    }
    
    tbl->checksum = mk_checksum((unsigned char*)tbl, tbl->length);
}

void L4_vm::spawn_cpu(l4_uint32_t apicid, l4_umword_t eip, l4_umword_t sp)
{
    karma_log(DEBUG, "spawn_cpu: apic_id=%d, eip = %lx, sp = %lx\n", apicid, eip, sp);
    karma_log(DEBUG, "spawn_cpu: apicid %x\n", apicid);
    int i = apicid;
    if(eip)
        _l4_cpu[apicid]->set_ip_and_sp(eip - 0xc0000000, sp);

    // requested a CPU that is not online. Try to place it elsewhere...
    if(!L4Re::Env::env()->scheduler()->is_online(i))
        i = i % _nr_cpus;

    _l4_cpu[apicid]->get_thread(_l4_cpu_threads[apicid]);
    if(apicid == 0)
        _gic->set_cpu(*_l4_cpu[apicid]);
    _l4_cpu[apicid]->setCPU(i);
    _l4_cpu[apicid]->start();
    _present_cpus++;
}

void L4_vm::dump_state()
{
    printf("Dumping the state of all CPUs:\n");
    for(unsigned int i=0; i<_present_cpus;++i)
        _l4_cpu[i]->dump_state();
}

void L4_vm::dump_exit_reasons()
{
    for(unsigned int i=0; i<_present_cpus; i++)
        _l4_cpu[i]->dump_exit_reasons();
    _l4_cpu_bus->dump();
}

void L4_vm::print_time()
{
    l4_uint32_t s, ns, seconds = 0, nseconds = 0;
    l4_tsc_to_s_and_ns(l4_rdtsc(), &s, &ns);
    if(_old_s!=0)
    {
        seconds = _old_s;
        nseconds = _old_ns;
        seconds = s - seconds;
        nseconds = ns - nseconds;
        printf("L4_vm: time diff %2d: %dm %2ds %3dms\n",
            old_timestamp, seconds/60, seconds%60, nseconds/1000000);
    }
    old_timestamp++;
    _old_s = s;
    _old_ns = ns;
}

