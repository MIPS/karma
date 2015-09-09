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
    unsigned int affinity_mask;
    unsigned int vcpu_prio;
    bool fb;
    bool pci;
    const char *kernel_name;
    const char *kernel_opts;
    const char *ramdisk_name;
    const char *bd_image_name;
    unsigned int debug_level;
    const char *shm_prod;
    const char *shm_cons;
    bool paravirt_guest;
};

extern struct __params params;

extern int cpu_option;

