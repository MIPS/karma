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

class L4_vm_driver
{
public:
    enum Exitcode
    {
        Vmmcall,
        Hlt,
        Error,
        Handled,
        Unhandled
    };
    static L4_vm_driver * create(unsigned id);

    virtual ~L4_vm_driver();
    virtual void vmresume() = 0;
    virtual void handle_interrupt(int irq) = 0;
    virtual void handle_vmexit() = 0;
    virtual void pre_vmresume() = 0;
    virtual void set_ip_and_sp(l4_umword_t eip, l4_umword_t sp) = 0;

    // debug
    virtual void dump() = 0;
    virtual void dump_state() = 0;
    virtual l4_umword_t current_ip() = 0;
};

