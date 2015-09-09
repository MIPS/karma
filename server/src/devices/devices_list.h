/*
 * devices_list.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * devices_list.h
 *
 *  Created on: 14.02.2011
 *      Author: janis
 */

#ifndef DEVICES_LIST_H_
#define DEVICES_LIST_H_

#define KARMA_DEVICE_ID(name) KARMA_HYC_##name
#define KARMA_HYPERCALL_DEVICES_LIST typedef enum{
#define KARMA_DECLARE_HYPERCALL_DEVICE(name) KARMA_DEVICE_ID(name),
#define KARMA_HYPERCALL_DEVICES_LIST_END MAX_HYC} KARMA_HYC_items_t;

KARMA_HYPERCALL_DEVICES_LIST
KARMA_DECLARE_HYPERCALL_DEVICE(karma)
KARMA_DECLARE_HYPERCALL_DEVICE(mem)
KARMA_DECLARE_HYPERCALL_DEVICE(vm)
KARMA_DECLARE_HYPERCALL_DEVICE(apic)
KARMA_DECLARE_HYPERCALL_DEVICE(ahci)
KARMA_DECLARE_HYPERCALL_DEVICE(bdds)
KARMA_DECLARE_HYPERCALL_DEVICE(pci)
KARMA_DECLARE_HYPERCALL_DEVICE(fb)
KARMA_DECLARE_HYPERCALL_DEVICE(gic)
KARMA_DECLARE_HYPERCALL_DEVICE(ser)
KARMA_DECLARE_HYPERCALL_DEVICE(timer)
KARMA_DECLARE_HYPERCALL_DEVICE(net)
KARMA_DECLARE_HYPERCALL_DEVICE(ioport)
KARMA_DECLARE_HYPERCALL_DEVICE(arch)
KARMA_HYPERCALL_DEVICES_LIST_END

enum karma_gic_device_functions {
    karma_gic_df_enable = 0x2,
    karma_gic_df_set_cpu= 0x4,
    karma_gic_df_get_base_reg= 0x8,
};

#define L4_MAX_GIC_NR   1
#define L4_MAX_IRQ      16

enum karma_ser_df_functions {
    karma_ser_df_init,
    karma_ser_df_read,
    karma_ser_df_writeln,
};

#define L4_TIMER_BASE 0x0
#define L4_TIMER_INIT               L4_TIMER_BASE + 0x0
#define L4_TIMER_ENABLE             L4_TIMER_BASE + 0x4
#define L4_TIMER_CYCLES             L4_TIMER_BASE + 0x8

#define L4_TIMER_ENABLE_PERIODIC 1
#define L4_TIMER_ENABLE_ONESHOT 2

enum karma_karma_device_functions {
    karma_df_printk,
    karma_df_panic,
    karma_df_debug,
    karma_df_get_time,
    karma_df_get_khz_cpu,
    karma_df_get_khz_bus,
    karma_df_oops,
    karma_df_report_jiffies_update,
    karma_df_log3val,
    karma_df_request_irq,
};

enum karma_mem_device_functions {
    karma_mem_df_guest_phys_to_host_phys,
    karma_mem_df_dma_base,
};

enum karma_arch_device_functions {
    karma_arch_df_wrmsr,
};

enum karma_fb_device_functions {
    karma_fb_df_get_info,
    karma_fb_df_get_ds_size,
    karma_fb_df_get_ds_addr,
    karma_fb_df_update_rect,
    karma_fb_df_get_ev_ds_size,
    karma_fb_df_get_ev_ds_addr,
    karma_fb_df_completion,
};

enum karma_ioport_device_functions {
    karma_ioport_inb,
    karma_ioport_inw,
    karma_ioport_inl,
    karma_ioport_outb,
    karma_ioport_outw,
    karma_ioport_outl,
    karma_ioport_insb,
    karma_ioport_insw,
    karma_ioport_insl,
    karma_ioport_outsb,
    karma_ioport_outsw,
    karma_ioport_outsl,
};

#define L4_NET_BASE_ADDR 0x0
#define L4_NET_GET_INFO             L4_NET_BASE_ADDR + 0x2
#define L4_NET_SEND_BUF_ADDR        L4_NET_BASE_ADDR + 0x8
#define L4_NET_RECV_BUF_ADDR        L4_NET_BASE_ADDR + 0xA
#define L4_NET_GET_BUF_SIZE         L4_NET_BASE_ADDR + 0xE
#define L4_NET_GET_IRQ              L4_NET_BASE_ADDR + 0x10
#define L4_NET_SEND_PACKET          L4_NET_BASE_ADDR + 0x20
#define L4_NET_RECV_SIZE            L4_NET_BASE_ADDR + 0x30

#define L4_PCI_BASE_ADDR 0x0
#define L4_PCI_PROBE                L4_PCI_BASE_ADDR + 0x2
#define L4_PCI_CONF1_READ           L4_PCI_BASE_ADDR + 0x4
#define L4_PCI_CONF1_WRITE          L4_PCI_BASE_ADDR + 0x8
#define L4_PCI_ENABLE_IRQ           L4_PCI_BASE_ADDR + 0xA
#define L4_PCI_IOMAP                L4_PCI_BASE_ADDR + 0xC

#define L4_AHCI_CONFIGURE_IRQ           0x4
#define L4_AHCI_CONFIGURE_IOMEM_BASE    0xA

enum karma_bdds_device_functions {
    karma_bdds_df_mode              = 0x0,
    karma_bdds_df_disk_size         = 0x4,
    karma_bdds_df_transfer_read_ds  = 0x8,
    karma_bdds_df_transfer_write_ds = 0xc,
    karma_bdds_df_transfer_ds_size  = 0x10,
    karma_bdds_df_transfer_write    = 0x14,
    karma_bdds_df_transfer_read     = 0x18,
};

enum karma_device_irqs {
    karma_irq_timer = 0,
    karma_irq_ser = 1,
    karma_irq_fb = 13,
    karma_irq_net = 14,
    karma_irq_pci = 15,
};

#define KARMA_MAKE_COMMAND(device, addr)\
        (unsigned long)(((addr) & 0xffff) | (((device) & 0xff) << 16) | 0x80000000)

#endif /* DEVICES_LIST_H_ */
