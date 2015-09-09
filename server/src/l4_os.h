/*
 * l4_os.h - TODO enter description
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

#include "l4_mem.h"

class L4_os
{
public:
    static L4_os * create_os(L4_mem *l4_mem);
    virtual void setup_os() = 0;
    virtual void guest_os_init(void *context) = 0;
    virtual void backtrace(l4_umword_t addr, l4_umword_t val) = 0;
    virtual l4_addr_t get_guest_entrypoint() = 0;
};

