/*
 * l4_cpu.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "l4_cpu.h"

#include "util/debug.h"
#include "util/statistics.hpp"
#include "util/local_statistics.hpp"
#include "devices/device_call.hpp"
#include "devices/device_init.hpp"

class IPIInterrupt: public L4_vcpu::Interrupt
{
private:
    L4::Cap<L4::Irq> _irq;
    L4_cpu & _cpu;
public:
    IPIInterrupt(const L4::Cap<L4::Irq> & irq, L4_cpu & cpu)
    :   _irq(irq)
    ,   _cpu(cpu){}
    virtual ~IPIInterrupt(){}
    virtual void handle()
    {
        _cpu.handle_irq(VCPU_IPI_LABEL);
    }
    virtual L4::Cap<L4::Irq> & cap()
    {
        return _irq;
    }
};

L4_cpu::L4_cpu(
        int id,
        L4_os *l4_os):
        L4_vcpu(params.vcpu_prio, id),
        _id(id),
        _l4_os(l4_os),
        _start_ip(0),
        _start_sp(0)
{ }

void L4_cpu::dump_exit_reasons()
{
    printf("CPU %d exit reasons\n", _id);
    printf("VMMCALL: %7d ", exit_reason[0]);
    printf("HLT:     %7d ", exit_reason[1]);
    printf("vINT:    %7d ", exit_reason[2]);
    printf("pINT:    %7d ", exit_reason[3]);
    printf("Intercept%7d ", exit_reason[4]);
    printf("Injected Interrupt: %7d ", exit_reason[5]);
    for(int i=0; i<6; i++)
        exit_reason[i]=0;
    _l4_apic->dump();
    _l4_vm_driver->dump();
}

void L4_cpu::handle_irq(int irq)
{
	GET_VM.gic().checkIRQ(irq);

    _l4_vm_driver->handle_interrupt(*_l4_apic, irq);
    _l4_vm_driver->vmresume();
}

void L4_cpu::inject_irq(unsigned irq)
{
    if(irq == VCPU_IPI_LABEL){
        if(_l4_apic->nr_pending() == 0) return;
    }
    _l4_vm_driver->handle_interrupt(*_l4_apic, irq);
}


void L4_cpu::handle_vmexit()
{
    _l4_vm_driver->handle_vmexit(*_l4_apic);
    _l4_vm_driver->vmresume();
}

void L4_cpu::finalizeIrq()
{
    _l4_vm_driver->vmresume();
}

void L4_cpu::run()
{
    try
    {
        LSTATS_INIT;
        util::getLStats().id(_id);
        run_intern();
    }
    catch(L4_exception &e)
    {
        e.print();
        backtrace();
        enter_kdebug("L4_cpu: Caught an exception");
    }
}

void L4_cpu::run_intern() {
    _l4_vm_driver = L4_vm_driver::create(_id);
    if(_start_ip) _l4_vm_driver->set_ip_and_sp(_start_ip, _start_sp);
    {
        L4::Cap<L4::Irq> uirq;
        _l4_apic->get_uirq(uirq);
        registerInterrupt(*new IPIInterrupt(uirq, *this));
    }

    GET_VM.cpu_bus().add(_l4_apic);
    _l4_apic->init();

    _l4_vm_driver->pre_vmresume();

    _vcpu_state->saved_state = L4_VCPU_F_IRQ;

    _l4_vm_driver->vmresume();

    while(1)
      ;
    // we turned control over to the vm. Upon Interrupts, vmmcalls or intercepts
    // one of the handlers is entered

}

void L4_cpu::setup()
{
    LOCAL_DEVICES_INIT;
    _l4_apic = &GET_HYPERCALL_DEVICE(apic);
    karma_log(DEBUG, "setting apic_id to: %x\n", _id);
    _l4_apic->setId(_id);
}

void L4_cpu::backtrace()
{
    printf("This is CPU %d\n", _id);
    _l4_os->backtrace(_vcpu_state->r.ip, _vcpu_state->r.bp);
    GET_VM.dump_state();
}

void L4_cpu::dump_state()
{
    printf("CPU %d\n", _id);
    _l4_vm_driver->dump_state();
    _l4_os->backtrace(_vcpu_state->r.ip, _vcpu_state->r.bp);
}

void L4_cpu::get_thread(L4::Cap<L4::Thread> &thread)
{
    thread = _vcpu_thread;
}

