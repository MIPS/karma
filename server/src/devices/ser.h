/*
 * l4_ser.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#ifndef L4_SER_H
#define L4_SER_H

#include "device_interface.h"
#include "../util/buffer.hpp"
#include "../l4_lirq.h"
#include "../thread.hpp"
#include "../util/debug.h"

class 
Ser : public device::IDevice, protected karma::Thread
{
private:
    L4_lirq _irq;
    L4_lirq _ser_irq;
    unsigned counter;
    Buffer<1024> _read_buf;
    Buffer<1024> _write_buf;

    char str[1024];
    l4_addr_t _shared_mem;
    l4_addr_t _base_ptr;

public:
    Ser();

    void init(unsigned int);
    void flush(void);

    virtual void hypercall(HypercallPayload &);

protected:
    virtual void run(L4::Cap<L4::Thread> & thread);
};

#endif // L4_SER_H
