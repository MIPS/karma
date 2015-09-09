/*
 * l4_cpu_bus.h - TODO enter description
 * 
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include <l4/sys/types.h>

#include "devices/apic.h"

#define MAX_CPUS 8

class L4_apic;

class
L4_cpu_bus
{
public:
    L4_cpu_bus();
    ~L4_cpu_bus() {}
    void add(L4_apic *l4_apic);
    // broadcast an IPI with karma_logical addressing mode
    void broadcast_ipi_logical(unsigned int sender_id, l4_uint8_t vector, l4_uint8_t destination);
    void dump();
private:
    unsigned ipi_counter;
    L4_apic *_l4_apic[MAX_CPUS];
    unsigned int _registered_apics;
};

