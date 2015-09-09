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
#include <l4/util/util.h>

#include <l4/sys/types.h>
#include <l4/sys/irq>
#include <l4/sys/icu>
#include <l4/sys/vcon>
#include <l4/sys/err.h>

#include <string.h>


#include "ser.h"
#include "../l4_vm.h"
#include "devices_list.h"
#include "mipsregs.h"
#include "irqchip.h"

L4::Cap<L4::Vcon> con;

Ser::Ser()
        : karma::Thread(2, "KARMA SER IRQ"), _shared_mem(0) {}

void
Ser::init(enum karma_device_irqs irq)
{
    _ser_irq_number = irq;
    _base_ptr = GET_VM.mem().base_ptr();
    GET_VM.gic().attach(_ser_irq.lirq(), irq);
    con = L4::Cap<L4::Vcon>(L4Re::Env::env()->log());
    l4_vcon_attr_t attr;
    L4Re::chksys(con->get_attr(&attr), "console get_attr");
    attr.l_flags &= ~L4_VCON_ECHO;
    attr.o_flags &= ~L4_VCON_ONLRET;
    L4Re::chksys(con->set_attr(&attr), "console set_attr");

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
            //karma_log(TRACE, "[ser] init guest_buffer = %lx\n", hp.reg(0));
            // KYMA TODO add size param and register in iomem map with range check
            GET_VM.mem().phys_to_local(MIPS_KSEG0_TO_PHYS(hp.reg(0)), &_shared_mem);
            karma_log(DEBUG, "[ser] _shared_mem = %lx\n", _shared_mem);
            break;
        case karma_ser_df_read:
            //karma_log(TRACE, "[ser] karma_ser_df_read\n");
            c = 0;
            if (_read_buf.count())
            {
                if (_read_buf.get(&c))
                {
                    if(c)
                    {
                        //karma_log(TRACE, "[ser] read: \"%c\"\n", c);
                        hp.reg(0) = c;
                        break;
                    }
                }
            }
            //karma_log(TRACE, "[ser] read: \"-1\"\n");
            hp.reg(0) = -1;
            if (params.paravirt_guest)
                mipsvz_karma_irq_line(_ser_irq_number, 0);
            break;
        case karma_ser_df_writeln:
            // writeln has an implicit flush!
            //karma_log(TRACE, "[ser] writeln val=%ld:\n", hp.reg(0));
            if(!_shared_mem)
                break;
            for(unsigned int i= 0;i<hp.reg(0); i++)
            {
                //karma_log(TRACE, "[ser] writeln (%c)\n", *(char*)(_shared_mem+i));
                _write_buf.put(*(char*)(_shared_mem+i));
            }
            strncpy(str, _write_buf.ptr(), _write_buf.count());
            str[_write_buf.count()] = 0; // zero terminate...
            con->write(str, _write_buf.count());          // print
            _write_buf.flush();          // clear buffer
            break;
        case karma_ser_df_early_putchar:
            //karma_log(TRACE, "[ser] early_putchar %c\n", (char)hp.reg(0));
            // ignore early_putchar once serial device has been initialized
            if(_shared_mem)
                break;
            str[0] = (char)hp.reg(0);
            str[1] = 0;
            L4Re::Env::env()->log()->print(str);
            _write_buf.flush();
            break;
        default:
            karma_log(ERROR, "[ser] unknown hypercall opcode (%x). Ignoring.\n", hp.address());
            break;
    }
}

void
Ser::run(L4::Cap<L4::Thread> & thread)
{
    char *buf = (char*)malloc(1024);
    karma_log(INFO, "[ser] Hello from irq thread!\n");

    karma_log(INFO, "[ser] About to bind IRQ!\n");
    if (l4_error(L4Re::Env::env()->log()->bind(0, _log_irq.lirq())))
        karma_log(ERROR, "[ser] Did not get the irq capability.\n");
    else
        karma_log(DEBUG, "[ser] Successfully bound irq capability to karma_log\n");

    int len=2;
    int ret=0;
    l4_msgtag_t tag;
    l4_utcb_t * utcb = l4_utcb();

    karma_log(INFO, "[ser] About to attach IRQ!\n");
    if ((ret = l4_error(_log_irq.lirq()->attach(12, thread))))
    {
        karma_log(ERROR, "[ser] _log_irq->attach error %d\n", ret);
        exit(1);
    }

    karma_log(TRACE, "[ser] while...\n");
    while(1)
    {
        karma_log(TRACE, "About to do a RX on the IRQ...\n");
        if ((l4_ipc_error(tag = _log_irq.lirq()->receive(), utcb)))
            karma_log(ERROR, "[ser] _log_irq->receive ipc error: %s\n", l4sys_errtostr(l4_error(tag)));
//        else if (!tag.is_irq())
//            printf("Did not receive an IRQ\n");

        ret = con->read((char *)buf, len);
        if (ret < 0) {
            continue;
        }

        if(ret > 0 && buf[0]>0)
        {
            for(int i=0; i<ret; i++)
                _read_buf.put(buf[i]);
            _ser_irq.lirq()->trigger();
        }
    }
    _log_irq.lirq()->detach();
}

