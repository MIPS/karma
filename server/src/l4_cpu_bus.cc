/*
 * l4_cpu_bus.cc - TODO enter description
 * 
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "l4_cpu_bus.h"

#include "util/debug.h"

L4_cpu_bus::L4_cpu_bus()
{
    _registered_apics = 0;
    for(unsigned int i=0; i<MAX_CPUS; ++i)
        _l4_apic[i] = 0;
    ipi_counter = 0;
}

void L4_cpu_bus::add(L4_apic *l4_apic)
{
    _l4_apic[_registered_apics] = l4_apic;
    _registered_apics++;
}

void L4_cpu_bus::broadcast_ipi_logical(unsigned int sender_id, l4_uint8_t vector, l4_uint8_t destination)
{
    ipi_counter++;
    l4_uint8_t tmp = destination;
    unsigned taken = 0, temp = 0;
    if(tmp == 0xe)
        tmp = 0xf;
    for(unsigned int i=0; i<_registered_apics; i++)
    {
        temp =_l4_apic[i]->put_ipi_logical(vector, tmp);
        taken += temp;
        temp = 0;
    }
    if(!taken)
    {
        karma_log(TRACE, "L4_cpu_bus: nobody received the IPI: vector: %d destination: %d sent by: %d\n",
                vector,
                destination,
                sender_id);
    }
    if(destination == 0xf && taken != _registered_apics)
        karma_log(DEBUG, "L4_cpu_bus: IPI%d from %d to all not successfull\n", vector, sender_id);
}

void L4_cpu_bus::dump()
{
    printf("L4_cpu_bus: %d IPIs\n", ipi_counter);
    ipi_counter = 0;
}

