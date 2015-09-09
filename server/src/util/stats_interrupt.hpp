/*
 * stats_interrupt.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * stats_interrupt.hpp
 *
 *  Created on: 08.11.2010
 *      Author: janis
 */

#ifndef STATS_INTERRUPT_HPP_
#define STATS_INTERRUPT_HPP_

namespace util{

class Item_int_stats : public Item{
private:
    class cpu_data{
    public:
        cpu_data()
        :   _pending(0)
        ,   _last_time_zero(0)
        ,   _armed(false){}

        unsigned _pending;
        l4_uint64_t _last_time_zero;
        bool _armed;
//        unsigned _pm_pending;
//        unsigned _pm_time;
    };
    typedef std::map<l4_utcb_t *, cpu_data> cpu_map_t;
    cpu_map_t _cpu_map;
    mutex _cpu_map_mutex;
    cpu_data & getcpu(){
        _cpu_map_mutex.enter();
        cpu_data & cpu = _cpu_map[l4_utcb()];
        _cpu_map_mutex.leave();
        return cpu;
    }
public:
    Item_int_stats(const std::string & name, const std::string & description)
    :   Item(name, description){}
    void incpending() {
        cpu_data & cpu = getcpu();
        if(cpu._pending++ == 0){
            cpu._last_time_zero = tsc();
            cpu._armed = true;
        }
    }
    void decpending() {
        cpu_data & cpu = getcpu();
        --cpu._pending;
        if(cpu._armed){
            cpu._armed = false;
            addMeasurement(tsc() - cpu._last_time_zero);
        }
    }
//    void injected() {
//        cpu_data & cpu = getcpu();
//        if(cpu._armed){
//            cpu._armed = false;
//            addMeasurement(tsc() - cpu._last_time_zero);
//        }
//    }
};

} // namespace util

#endif /* STATS_INTERRUPT_HPP_ */
