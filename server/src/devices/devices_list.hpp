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
#include "apic.h"
#include "ahci.h"
#include "bdds.h"
#include "pci.h"
#include "fb.h"
#include "gic.h"
#include "../l4_mem.h"
#include "ser.h"
#include "timer.h"
#include "net.h"
#include "ioport.h"
#include <cstring>
#include <stdio.h>

DEFINE_GLOBAL_DEVICE(karma, KarmaDevice)
DEFINE_GLOBAL_DEVICE(mem, L4_mem)
DEFINE_GLOBAL_DEVICE(vm, CPUSpawner)
DEFINE_LOCAL_DEVICE(apic, L4_apic)
DEFINE_GLOBAL_DEVICE(ahci, L4_ahci)
DEFINE_GLOBAL_DEVICE(bdds, Bdds)
DEFINE_GLOBAL_DEVICE(pci, L4_pci)
DEFINE_GLOBAL_DEVICE(fb, L4_fb)
DEFINE_GLOBAL_DEVICE(gic, Gic)
DEFINE_GLOBAL_DEVICE(ser, Ser)
DEFINE_GLOBAL_DEVICE(timer, L4_timer)
DEFINE_GLOBAL_DEVICE(net, L4_net)
DEFINE_GLOBAL_DEVICE(ioport, Ioport)

// Dummy definition of "arch"-device
// architecture specifics will be intercepted directly by the vm_driver
DEFINE_DUMMY_DEVICE(arch)

#endif /* DEVICES_LIST_HPP_ */
