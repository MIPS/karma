/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * mem.cc - provides address translation services
 */

#include "mem.h"
#include "devices_list.h"
#include "../l4_vm.h"
#include "../l4_mem.h"
#include "../util/debug.h"

void MemDevice::hypercall(HypercallPayload & payload)
{
    l4_size_t size = 0;

    switch(payload.address())
    {
        case karma_mem_df_guest_phys_to_host_phys:
            {
                l4_addr_t &guest_phys = payload.reg(0);
                l4_addr_t &host_phys = payload.reg(1);

                GET_VM.mem().phys_to_host(guest_phys, &host_phys, &size);
                karma_log(DEBUG, "[mem] karma_mem_df_guest_phys_to_host_phys "
                        "guest phys=%p, host phys=%p size=%d\n",
                        (void*)guest_phys, (void*)host_phys, size);
            }
            break;
        case karma_mem_df_dma_base:
            {
                l4_addr_t guest_phys = (l4_addr_t)0;
                l4_addr_t &host_phys = payload.reg(0);

                GET_VM.mem().phys_to_host(guest_phys, &host_phys, &size);
                karma_log(DEBUG, "[mem] karma_mem_df_dma_base "
                        "guest phys=%p, host phys=%p size=%d\n",
                        (void*)guest_phys, (void*)host_phys, size);
            }
            break;
        default:
            break;
    }
}
