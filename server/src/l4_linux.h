/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * l4_linux.h - TODO enter description
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

#include <l4/re/dataspace>
#include <l4/sys/vm>

#include "l4_os.h"
#include "l4_mem.h"
#include "params.h"

class L4_linux : public L4_os {
public:
    L4_linux(L4_mem *l4_mem);
    ~L4_linux();
private:
    L4_linux(const L4_linux& rhs);            // empty copy constructor
    L4_linux& operator=(const L4_linux& rhs); // empty assignment constructor
public:
    void setup_os();
    void guest_os_init(void *context);
    void backtrace(l4_umword_t ip, l4_umword_t stackframe);
    l4_addr_t get_guest_entrypoint();
private:
    void setup_memory();
    void setup_prom();
    void setup_prom_paravirt();
    void setup_prom_machvirt();
    l4_addr_t _lx_ramdisk_addr; //the ramdisk datapsace allocated by us, is attached to this address
    l4_addr_t _lx_ramdisk_size;
    l4_addr_t _guest_ramdisk_addr; // the start of the ramdisk as what the guest sees
    l4_addr_t _guest_entrypoint;
    L4_mem *_memory;
    l4_umword_t _guest_argc;
    l4_addr_t _guest_argv;
    L4::Cap<L4Re::Dataspace> lx_image_ds, lx_ramdisk_ds;
};

