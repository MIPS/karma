/*
 * l4_gic.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#include "gic.h"

//#include "../l4_apic.h"
#include "../l4_vm.h"
#include "../l4_cpu.h"
#include "devices_list.h"

#define DEBUG_LEVEL 4
#include "../util/debug.h"

class GicInterrupt : public L4_vcpu::Interrupt
{
private:
    l4_umword_t _guest_irq;
    L4_cpu & _cpu;
    L4::Cap<L4::Irq> _cap;
public:
    GicInterrupt(const L4::Cap<L4::Irq> & cap, L4_cpu & cpu, l4_umword_t guest_irq)
    :   _guest_irq(guest_irq)
    ,   _cpu(cpu)
    ,   _cap(cap)
    {
    }
    virtual ~GicInterrupt(){}
    virtual void handle()
    {
        _cpu.handle_irq(_guest_irq);
    }
    virtual L4::Cap<L4::Irq> & cap()
    {
        return _cap;
    }

};

Gic::Gic()
    : _cpu(NULL), _hasVCPU(false)
{}

void
Gic::init()
{
    _shared_state = reinterpret_cast<l4_gic_shared_state *>(memalign(0x1000, 0x1000));
    for(unsigned int i(0); i != NR_INTR; ++i)
        _irqs[i].invalidate();
    _shared_state->_enabled= 0xffffffff;
    _shared_state->_level_trigered = 0;
    GET_VM.mem().register_iomem(&_shared_state_gpa, 0x1000);
    GET_VM.mem().map_iomem(_shared_state_gpa, (l4_addr_t)_shared_state, 1);
}

Gic::~Gic()
{
    L4Re::Env::env()->task()->unmap(l4_fpage((l4_addr_t)_shared_state, 0x1000, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
    free(_shared_state);
}


void
Gic::set_cpu(L4_cpu & cpu)
{
    _cpu = &cpu;
    _hasVCPU = true;
    karma_log(INFO, "[gic] set_vcpu!\n");
    while(!_postponed_attach_list.empty()){
        postponed_attach & p = _postponed_attach_list.front();
        if(p.cap_type)
            attach(p.cap, p.as, p.level_triggered);
        else
            attach(p.irq, p.as, p.level_triggered);
        _postponed_attach_list.pop_front();
    }
}

void
Gic::hypercall(HypercallPayload & payload)
{
    if(payload.address() & 1)
        write(payload.address() & ~1L, payload.reg(0));
    else
        payload.reg(0) = read(payload.address());
}

l4_umword_t
Gic::read(l4_umword_t addr)
{
    switch (addr)
    {
        case karma_gic_df_enable:
            return _shared_state->_enabled;
        case karma_gic_df_get_base_reg:
            return _shared_state_gpa;
	    default:
            karma_log(ERROR, "[gic] read with unknown opcode: %lx. Ignoring.\n", addr);
            break;
    };
    return ~0UL;
}

void
Gic::write(l4_umword_t addr, l4_umword_t val)
{
    switch (addr)
    {
        case karma_gic_df_enable:
            unmask(val);
            break;
        case karma_gic_df_set_cpu:
            karma_log(INFO, "[gic] karma_gic_df_set_cpu not implemented.\n");
            break;
        default:
            karma_log(ERROR, "[gic] write with unknown opcode: %lx. Ignoring.\n", addr);
            break;
    };
}

template <typename T>
inline static void setFlag(T & field, const bool value, const unsigned int index)
{
    if(value)
        field |= ((T)1) << index;
    else
        field &= ~(((T)1) << index);
}

void
Gic::attach(L4::Cap<L4::Irq> & lirq, l4_umword_t as, bool level)
{
    L4::Cap<L4::Irq> & irq = _irqs[as];
    if(irq.is_valid())
        return;
    if(!lirq.is_valid())
        return;

    if(_hasVCPU)
    {
        irq = lirq;
        setFlag(_shared_state->_level_trigered, level, as);
        // XXX missing sensible memory management for irqhandler
        _cpu->registerInterrupt(*new GicInterrupt(irq, *_cpu, as));
    }
    else 
    {
        // postpone the attach untill we have a vcpu
        postponed_attach p;
        p.as = as;
        p.cap = lirq;
        p.cap_type = true;
        p.level_triggered = level;
        _postponed_attach_list.push_back(p);
    }
}

void 
Gic::attach(l4_umword_t irq_no, l4_umword_t as, bool level)
{
    if(as == (l4_umword_t)-1)
        as = irq_no;
    L4::Cap<L4::Irq> & irq = _irqs[as];

    if(_hasVCPU)
    {
        // if irq is valid irq_no is already attached so there is nothing to do
        if(!irq.is_valid())
        {
            setFlag(_shared_state->_level_trigered, level, as);
            irq = L4Re::Util::cap_alloc.alloc<L4::Irq>();
            if(!irq.is_valid())
            {
                karma_log(ERROR, "[gic] could not allocate irq. Terminating.\n");
                exit(1);
            }

            // request the irq from io
            if(l4io_request_irq(irq_no, irq.cap()))
            {
                karma_log(ERROR, "[gic] l4io_request_irq(%d) failed. Terminating.\n", (int)irq_no);
                exit(1);
            }

            // attach the requested irq to the label "as"
            // XXX missing sensible memory management
            karma_log(DEBUG, "[gic] calling _cpu->registerInterrupt()\n");
            _cpu->registerInterrupt(*new GicInterrupt(irq, *_cpu, as));
        } 
        else 
        {
            karma_log(ERROR, "[gic] Error! Tried to attach to an already attached irq-slot! %lu as %lu. Ignoring.\n", irq_no, as);
        }
    } 
    else 
    {
        // postpone the attach until we have a vcpu
        postponed_attach p;
        p.as = as;
        p.irq = irq_no;
        p.cap_type = false;
        p.level_triggered = level;
        _postponed_attach_list.push_back(p);
    }
}

void
Gic::detach(l4_umword_t label)
{
    L4::Cap<L4::Irq> & irq = _irqs[label];
    irq->detach();
    irq.invalidate();
}

void
Gic::mask(l4_umword_t label)
{
    _shared_state->_enabled &= ~(1 << label);
}

void
Gic::unmask(l4_umword_t label)
{
    L4::Cap<L4::Irq> & irq = _irqs[label];
    if(irq.is_valid())
    {
        _shared_state->_enabled |= 1 << label;
        irq->unmask();
    }
}

bool
Gic::isMasked(l4_umword_t label) const
{
    return !(_shared_state->_enabled & (1 << label));
}

void
Gic::checkIRQ(l4_umword_t label)
{
	if((label < NR_INTR) && GET_VM.gic().isMasked(label))
    {
		karma_log(INFO, "[gic] ATTENTION: Trying to deliver a masked interrupt (label=%ld, mask=%x)\n",
	        label, (unsigned int)GET_VM.gic()._shared_state->_enabled);
	}
}

