/*
 * local_statistics.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * local_statistics.cc
 *
 *  Created on: 14.02.2011
 *      Author: janis
 */

#include "local_statistics.hpp"
#include "../l4_vcpu.h"

namespace util{

L4_vcpu::vcpu_specifics_key_t LSTATS_VCPU_KEY = L4_vcpu::specifics_create_key();

class LStats & getLStats(){
    return *static_cast<LStats*>(current_vcpu().getSpecific(LSTATS_VCPU_KEY));
}

void initLStats(){
    LStats * stats = new LStats();
    stats->init();
    current_vcpu().setSpecific(LSTATS_VCPU_KEY, stats);
}

}

util::mutex util::LStats::_lstats_map_lock;
std::vector<L4_lirq *> util::LStats::_lstats_notifier_map;
