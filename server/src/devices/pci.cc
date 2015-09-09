/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

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

#if defined(ARCH_mips)
l4_addr_t pci_ioport;

static l4_addr_t request_pci_ioports(void)
{
  l4io_device_handle_t fbdev_hdl;
  l4io_resource_handle_t fbres_hdl;
  l4_addr_t vaddr = 0;

  // KYMA TODO remove dependency on IOPORT_RESOURCE_END having to match
  // KYMA "System Controller Ports" window size (in linux, pci-machvirt.c)
  if (l4io_lookup_device("System Controller Ports", &fbdev_hdl, 0, &fbres_hdl))
    {
      karma_log(ERROR, "Could not find System Controller Ports\n");
      return 0;
    }

  vaddr = l4io_request_resource_iomem(fbdev_hdl, &fbres_hdl);
  if (vaddr == 0)
    {
      karma_log(ERROR, "Could not map System Controller Ports\n");
      return 0;
    }

  return vaddr;
}
#endif

void L4_pci::init()
{
    karma_log(DEBUG, "[pci] L4 PCI init\n");
    int err = 0;

    _vbus = L4Re::Env::env()->get_cap<void>("vbus", 4);

    if(!_vbus.is_valid())
        throw L4_EXCEPTION("query vbus");

    _root_bridge = 0;
    err = l4vbus_get_device_by_hid(_vbus.cap(), 0, &_root_bridge, "PNP0A03", 0, 0);
    if(err<0)
        throw L4_EXCEPTION("find PCI root bridge");
    karma_log(INFO, "[pci] found root bridge\n");

#if defined(ARCH_mips)
    pci_ioport = request_pci_ioports();
    karma_log(INFO, "[pci] pci_ioport base %x\n", (unsigned)pci_ioport);
    if (LOG_LEVEL(INFO))
        l4io_dump_vbus();
#endif
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
            karma_log(INFO, "[pci] PCI IRQ ENABLE pin=%d trigger=%x polarity=%x ret=%d\n", irq->pin,
                irq->trigger, irq->polarity, irq->ret);
            if(irq->ret)
            {
                GET_VM.gic().attach(irq->ret, irq->ret, true);
                GET_VM.gic().unmask(irq->ret);
            }
            break;
        case L4_PCI_IOMAP:
            map = (struct l4_iomap*)(GET_VM.mem().base_ptr() + val);
            karma_log(INFO, "[pci] L4_PCI_IOMAP iomap addr=%x, size=%d\n", map->addr, map->size);
#if defined(ARCH_mips)
            // KYMA TODO Instead of looking up "System Controller Ports" to init
            // pci_ioport, it would be better to have linux guest use the
            // L4_PCI_IOMAP hypercall to request this iomem region. However,
            // something isn't working with the mapping so leave it this way for
            // now.
            if(1) // KYMA TODO sanity check addr range and size
            {
                // 1st arg is guest virt, 2nd arg is source (mem resource)
                GET_VM.mem().map_iomem_hard(map->addr, pci_ioport, map->size, 1);

                karma_log(INFO, "[pci] mapped io mem to VM\n");
            }
#else
            if(map->addr > 0xf0000000 && map->addr != (unsigned)-1) // is this PCI or other peripheral device?
            {
                l4_addr_t virt = 0, reg_start, reg_len;
                //map->size *= 2;
                if(l4io_search_iomem_region(map->addr, map->size, &reg_start, &reg_len))
                {
                    karma_log(ERROR, "[pci] ERROR: no io region found!\n");
                }
                karma_log(INFO, "[pci] L4_pci: found io region %x, %x\n", (unsigned)reg_start, (unsigned)reg_len);
                if(l4io_request_iomem(map->addr, map->size, 0, &virt))
                {
                    karma_log(ERROR, "[pci] ERROR: l4io_request_iomem(%x,%d,0,&virt) failed\n",
                        (unsigned)reg_start, (unsigned)reg_len);
                }

                karma_log(INFO, "[pci] l4io_request_iomem(%x,%d,0,virt=%x)\n",
                    (unsigned)map->addr, (unsigned)map->size, (unsigned)virt);

                karma_log(INFO, "[pci] the virtual address of the mapped region is %lx\n", virt);

                karma_log(INFO, "[pci] DEBUG l4io_request_iomem(%x,%d,0,virt=%x)\n",
                        (unsigned)reg_start, (unsigned)reg_len, (unsigned)virt);

                GET_VM.mem().map_iomem_hard(map->addr, virt, map->size, 1);

                karma_log(INFO, "[pci] mapped io mem to VM\n");
            }
#endif
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
