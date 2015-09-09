/*
 * devices_list.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * devices_list.hpp
 *
 *  Created on: 14.02.2011
 *      Author: janis
 */

#ifndef DEVICES_LIST_HPP_
#define DEVICES_LIST_HPP_

#include "devices.hpp"
#include "devices_list.h"
#include "device_interface.h"
#include "cpu_spawner.hpp"
#include "karma_device.hpp"
#include "../l4_vm.h"
#include "bdds.h"
#include "pci.h"
#include "fb.h"
#include "gic.h"
#include "mem.h"
#include "ser.h"
#include "timer.h"
#include "shmem.h"

DEFINE_GLOBAL_DEVICE(karma, KarmaDevice)
DEFINE_GLOBAL_DEVICE(mem, MemDevice)
DEFINE_GLOBAL_DEVICE(vm, CPUSpawner)
DEFINE_DUMMY_DEVICE(dummy_local)
DEFINE_DUMMY_DEVICE(dummy0)
DEFINE_GLOBAL_DEVICE(bdds, Bdds)
DEFINE_GLOBAL_DEVICE(pci, L4_pci)
DEFINE_GLOBAL_DEVICE(fb, L4_fb)
DEFINE_GLOBAL_DEVICE(gic, GicDevice)
DEFINE_GLOBAL_DEVICE(ser, Ser)
DEFINE_GLOBAL_DEVICE(timer, L4_timer)
DEFINE_DUMMY_DEVICE(dummy1)
DEFINE_DUMMY_DEVICE(dummy2)
DEFINE_GLOBAL_DEVICE(shm_prod, L4_shm_prod)
DEFINE_GLOBAL_DEVICE(shm_cons, L4_shm_cons)

// Dummy definition of "arch"-device
// architecture specifics will be intercepted directly by the vm_driver
DEFINE_DUMMY_DEVICE(arch)

#endif /* DEVICES_LIST_HPP_ */
