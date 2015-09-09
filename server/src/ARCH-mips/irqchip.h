/*
 * Copyright (C) 2015 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#ifndef _IRQ_H
#define _IRQ_H

#include "vmm.h"
#include <l4/karma/karma_devices.h>

int mipsvz_create_irqchip();
bool mipsvz_write_irqchip(vz_vmm_t *vmm, unsigned int write, l4_addr_t address);
int mipsvz_karma_irq_line(enum karma_device_irqs karma_irq, unsigned int level);

#endif /* _IRQ_H */
