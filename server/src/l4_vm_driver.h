/*
 * l4_vm_driver.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/dataspace>
#include <l4/re/util/cap_alloc>
#include <l4/sys/vm>
#include <l4/sys/scheduler>
#include <l4/sys/factory>
#include <l4/sys/kdebug.h>
#include <l4/sys/types.h>
#include <l4/sys/thread>
#include <l4/util/rdtsc.h>

#include "l4_os.h"
#include "l4_mem.h"
#include "l4_linux.h"
#include "l4_vcpu.h"

#include "vmcs.h"

#include <contrib/libudis86/udis86.h>
#include <map>

#define UNKNOWN_INSTR_AMD 0
#define EAX_REG_AMD 1
#define EDX_REG_AMD 2 
#define ECX_REG_AMD 3
#define EBX_REG_AMD 4
#define EBP_REG_AMD 5
#define ESI_REG_AMD 6
#define EDI_REG_AMD 7
#define CLTS_INSTR_AMD 8
#define FNINIT_INSTR_AMD 9

#define EAX_REG_INTEL 0
#define ECX_REG_INTEL 1
#define EDX_REG_INTEL 2 
#define EBX_REG_INTEL 3
#define EBP_REG_INTEL 5
#define ESI_REG_INTEL 6
#define EDI_REG_INTEL 7


class L4_apic;
class L4_cpu;

class L4_vm_driver
{
public:
    enum Exitcode
    {
//        Intercepted_Instruction,
        Vmmcall,
//        Physical_Interrupt,
//        Virtual_Interrupt,
        Hlt,
        Error,
        Handled,
        Unhandled
    };
    static L4_vm_driver * create(
         unsigned id);

    virtual ~L4_vm_driver() {}
    virtual void vmresume() = 0;
    virtual void handle_interrupt(L4_apic & apic, int irq) = 0;
    virtual void handle_vmexit(L4_apic & apic) = 0;
    virtual void pre_vmresume() = 0;
    virtual void set_ip_and_sp(l4_umword_t eip, l4_umword_t sp) = 0;

    // debug
    virtual void dump() = 0;
    virtual void dump_state() = 0;
    virtual void dump_call_chain() = 0;
    virtual l4_umword_t current_ip() = 0;
};

