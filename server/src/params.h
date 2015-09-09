/*
 * params.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Matthias Lange <mlange@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

struct __params
{
    unsigned int asid;
    unsigned int virtual_cpus;
    unsigned int vcpu_prio;
    char *ankh_shm;
    bool fb;
    bool net;
    bool pci;
    bool ahci;
    bool vtlb;
    bool hpet;
    const char *kernel_name;
    const char *kernel_opts;
    const char *ramdisk_name;
    const char *bd_image_name;
    unsigned int debug_level;
};

enum cpu_type
{
    AMD_cpu     = 0,
    Intel_cpu   = 1,
    Unknown_cpu = -1,
};

extern struct __params params;

extern int cpu_option;

