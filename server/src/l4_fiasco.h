/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * l4_fiasco.h - setup Fiasco as the Karma guest OS
 */

#pragma once

#include <l4/re/dataspace>
#include <l4/sys/vm>

#include "l4_os.h"
#include "l4_mem.h"
#include "params.h"

class L4_fiasco : public L4_os {
public:
    L4_fiasco(L4_mem *l4_mem);
    ~L4_fiasco();
private:
    L4_fiasco(const L4_fiasco& rhs);            // empty copy constructor
    L4_fiasco& operator=(const L4_fiasco& rhs); // empty assignment constructor
public:
    void setup_os();
    void guest_os_init(void *context);
    void backtrace(l4_umword_t ip, l4_umword_t stackframe);
    l4_addr_t get_guest_entrypoint();
private:
    void setup_memory();
    l4_addr_t _guest_entrypoint;
    L4_mem *_memory;
    L4::Cap<L4Re::Dataspace> os_image_ds;
};

