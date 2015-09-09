/*
 * l4_pci.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "pci.h"
#include "devices_list.h"
#include "../l4_exceptions.h"
#include "../l4_vm.h"
#include "../util/debug.h"

#include <l4/sys/err.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/io/io.h>
#include <l4/util/util.h>

#include <stdlib.h>

void *irq_thread(void *);

struct l4_pci_conf
{
    unsigned int bus, df, value;
    int reg, len;
    unsigned int ret;
};

struct l4_pci_irq
{
    unsigned int bus, devfn;
    int pin;
    unsigned char trigger, polarity;
    int ret;
};

struct l4_iomap
{
    unsigned int addr, size;
};

void L4_pci::init()
{
    karma_log(DEBUG, "L4_pci\n");
    int err = 0;

    _vbus = L4Re::Env::env()->get_cap<void>("vbus", 4);

    if(!_vbus.is_valid())
        throw L4_EXCEPTION("query vbus");

    _root_bridge = 0;
    err = l4vbus_get_device_by_hid(_vbus.cap(), 0, &_root_bridge, "PNP0A03", 0, 0);
    if(err<0)
        throw L4_EXCEPTION("find PCI root bridge");
    karma_log(INFO, "found root bridge\n");
}

void L4_pci::hypercall(HypercallPayload & payload){
    if(payload.address() & 1){
        write(payload.address() & ~1L, payload.reg(0));
    } else {
        payload.reg(0) = read(payload.address());
    }
}

void
L4_pci::write(l4_umword_t addr, l4_umword_t val)
{
    struct l4_pci_conf *conf = 0;
    struct l4_pci_irq *irq = 0;
    struct l4_iomap *map = 0;
    l4_uint32_t value;
    switch(addr)
    {
        case L4_PCI_CONF1_READ:
            karma_log(TRACE, "L4_PCI_CONF1_READ\n");
            conf = (struct l4_pci_conf*)(GET_VM.mem().base_ptr() + val);
            conf->ret = l4vbus_pci_cfg_read(_vbus.cap(), _root_bridge, conf->bus, conf->df, conf->reg, &value, conf->len);
            conf->value = value;
            break;
        case L4_PCI_CONF1_WRITE:
            karma_log(TRACE, "L4_PCI_CONF1_WRITE\n");
            conf = (struct l4_pci_conf*)(GET_VM.mem().base_ptr() + val);
            conf->ret = l4vbus_pci_cfg_write(_vbus.cap(), _root_bridge, conf->bus, conf->df, conf->reg, conf->value, conf->len);
            break;
        case L4_PCI_ENABLE_IRQ:
            irq = (struct l4_pci_irq*)(GET_VM.mem().base_ptr() + val);
            irq->ret = l4vbus_pci_irq_enable(_vbus.cap(), _root_bridge, irq->bus, irq->devfn, irq->pin, &irq->trigger, &irq->polarity);
            karma_log(INFO, "PCI IRQ ENABLE pin=%d trigger=%x polarity=%x ret=%d\n", irq->pin,
                irq->trigger, irq->polarity, irq->ret);
            if(irq->ret)
            {
                GET_VM.gic().attach(irq->ret, irq->ret, true);
                GET_VM.gic().unmask(irq->ret);
            }

        break; // disable irq threads for now
        case L4_PCI_IOMAP:
            map = (struct l4_iomap*)(GET_VM.mem().base_ptr() + val);
            karma_log(INFO, "L4_PCI_IOMAP iomap addr=%x, size=%d\n", map->addr, map->size);
            if(map->addr > 0xf0000000 && map->addr != (unsigned)-1) // is this PCI or other peripheral device?
            {
                l4_addr_t virt = 0, reg_start, reg_len;
                //map->size *= 2;
                if(l4io_search_iomem_region(map->addr, map->size, &reg_start, &reg_len))
                {
                    printf("no io region found!\n");
                }
                karma_log(INFO, "L4_pci: found io region %x, %x\n", (unsigned)reg_start, (unsigned)reg_len);
                if(l4io_request_iomem(map->addr, map->size, 0, &virt))
                {
                    printf("l4io_request_iomem(%x,%d,0,&virt) failed\n",
                        (unsigned)reg_start, (unsigned)reg_len);
                }

                karma_log(INFO, "l4io_request_iomem(%x,%d,0,virt=%x)\n",
                    (unsigned)map->addr, (unsigned)map->size, (unsigned)virt);

                karma_log(INFO, "the virtual address of the mapped region is %lx\n", virt);

                karma_log(INFO, "L4_pci: DEBUG l4io_request_iomem(%x,%d,0,virt=%x)\n",
                        (unsigned)reg_start, (unsigned)reg_len, (unsigned)virt);

                GET_VM.mem().map_iomem_hard(map->addr, virt, map->size, 1);

                karma_log(INFO, "L4_pci: mapped io mem to VM\n");
            }
            break;
        default:
            break;
    }
}

l4_umword_t
L4_pci::read(l4_umword_t addr)
{
    l4_umword_t ret = -1;
    switch(addr)
    {
        case L4_PCI_PROBE:
            ret = 0;
            break;
        default:
            break;
    }
    return ret;
}
