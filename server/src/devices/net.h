/*
 * l4_net.h - TODO enter description
 * 
 * (c) 2011 Jan Nordholz <jnordholz@sec.t-labs.tu-berlin.de>
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include "device_interface.h"
#include "../l4_lirq.h"
#include "../thread.hpp"

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/cxx/ipc_stream>
#include <l4/cxx/iostream>
#include <l4/cxx/l4iostream>
#include <l4/ankh/client-c.h>
#include <l4/util/util.h>

class L4_net : public device::IDevice, private karma::Thread
{
public:
    L4_net();
    void init(unsigned int irq);
    virtual void hypercall(HypercallPayload &);
    virtual void run(L4::Cap<L4::Thread> & thread);
private:
    void write(l4_umword_t addr, l4_umword_t val);
    l4_umword_t read(l4_umword_t addr);
    L4::Cap<void> _ankh_session;
    char *_send_buf, *_recv_buf;
#if 0 // old interface
    Ankh::Shm_receiver *_ankh_receiver;
    Ankh::Shm_sender *_ankh_sender;
    Ankh::Shm_chunk *_ankh_info;
    l4shmc_area_t _shm_area;
#endif
    struct AnkhSessionDescriptor _ankh_info;
    L4_lirq _net_irq;
    pthread_mutex_t _mtx;
    unsigned int _intr, _recv_size;
};

