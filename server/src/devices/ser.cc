/*
 * l4_ser.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>

#include <l4/sys/types.h>
#include <l4/sys/irq>
#include <l4/sys/icu>
#include <l4/sys/vcon>
#include <l4/sys/err.h>

#include <string.h>


#include "ser.h"
#include "../l4_vm.h"
#include "devices_list.h"

Ser::Ser()
        : _shared_mem(0) {}

void
Ser::init(unsigned int irq)
{
    _base_ptr = GET_VM.mem().base_ptr();
    GET_VM.gic().attach(_ser_irq.lirq(), irq);
    start();
}

void
Ser::flush(void)
{
   strncpy(str, _write_buf.ptr(), _write_buf.count());
   str[_write_buf.count()] = 0;
   printf("%s", str);
   fflush(0);
}

void
Ser::hypercall(HypercallPayload &hp)
{
    char c;
    switch (hp.address())
    {
        case karma_ser_df_init:
            //printf("L4_ser: init\n");
            _shared_mem = _base_ptr + hp.reg(0) - 0xc0000000;
            //printf("L4_ser: _shared_mem = %x\n", (unsigned int)_shared_mem);
            break;
        case karma_ser_df_read:
            c = 0;
            if (_read_buf.count())
            {
                if (_read_buf.get(&c))
                {
                    if(c)
                    {
                        //printf("READ: \"%c\"\n", c);
                        hp.reg(0) = c;
                        break;
                    }
                }
            }
            //printf("READ: \"-1\"\n");
            hp.reg(0) = -1;
            break;
        case karma_ser_df_writeln:
            // writeln has an implicit flush!
            //printf("l4_ser: writln val=%d:\n", val);
            if(!_shared_mem)
                break;
            for(unsigned int i= 0;i<hp.reg(0); i++)
            {
                //printf("[ser] writeln (%c)\n", *(char*)(_shared_mem+i));
                _write_buf.put(*(char*)(_shared_mem+i));
            }
            strncpy(str, _write_buf.ptr(), _write_buf.count());
            str[_write_buf.count()] = 0; // zero terminate...
            L4Re::Env::env()->log()->print(str);          // print
            _write_buf.flush();          // clear buffer
            break;
        default:
            karma_log(ERROR, "[ser] unknown hypercall opcode (%x). Ignoring.\n", hp.address());
            break;
    }
}

void
Ser::run(L4::Cap<L4::Thread> & thread)
{
    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "KARMA SER IRQ");
    char *buf = (char*)malloc(1024);
    karma_log(INFO, "[ser] Hello from irq thread!\n");

    if (l4_error(L4Re::Env::env()->log()->bind(0, _irq.lirq())))
        karma_log(ERROR, "[ser] Did not get the irq capability.\n");
    else
        karma_log(DEBUG, "[ser] Successfully bound irq capability to karma_log\n");

    int len=2;
    int ret=0;
    l4_msgtag_t tag;
    if ((ret = l4_error(_irq.lirq()->attach(12, thread))))
    {
        karma_log(ERROR, "[ser] _irq->attach error %d\n", ret);
        exit(1);
    }
    while(1)
    {
        karma_log(DEBUG, "[ser] while...\n");
        if ((l4_ipc_error(tag = _irq.lirq()->receive(), l4_utcb())))
            karma_log(ERROR, "[ser] _irq->receive ipc error: %s\n", l4sys_errtostr(l4_error(tag)));
//        else if (!tag.is_irq())
//            printf("Did not receive an IRQ\n");

        ret = L4Re::Env::env()->log()->read((char *)buf, len);
        if(ret > 0 && buf[0]>0)
        {
            for(int i=0; i<ret; i++)
                _read_buf.put(buf[i]);
            _ser_irq.lirq()->trigger();
        }
    }
    _irq.lirq()->detach();
}

