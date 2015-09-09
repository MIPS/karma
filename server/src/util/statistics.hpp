/*
 * statistics.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * statistics.hpp
 *
 *  Created on: 06.11.2010
 *      Author: janis
 */

#ifndef STATISTICS_HPP_
#define STATISTICS_HPP_

#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include "singleton.hpp"
#include "cas.hpp"
#include <l4/util/util.h>

namespace util{
#define GET_STATS util::Singleton<util::Stats>::get()


class IItem{
protected:
    std::string _name;
    std::string _description;

public:
    IItem(const std::string & name, const std::string & description)
    :   _name(name)
    ,   _description(description)
    {
    }
    virtual void print() const = 0;
    virtual void flush() = 0;
};

class Stats{
private:
    typedef std::map<l4_uint32_t, IItem *> map_type;
    typedef std::map<l4_utcb_t *, unsigned> lock_map_t;
    map_type _item_map;
    mutable lock_map_t _measuring;
    mutable lock_map_t _locked;
    mutable mutex _lock;
public:
    Stats()
    {
    }
    void addItem(const l4_uint32_t id, IItem * item){
        enterExclusive();
        _item_map[id] = item;
        leaveExclusive();
    }
    void print() const{
        enterExclusive();
        map_type::const_iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter){
            iter->second->print();
        }
        leaveExclusive();
    }
    void flush(){
        enterExclusive();
        map_type::iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter){
            iter->second->flush();
        }
        leaveExclusive();
    }
    void enterMeasurement() const{
        l4_utcb_t * utcb = l4_utcb();
        _lock.enter();
        while(!_locked.empty() && (_measuring.find(utcb) == _measuring.end())){
           _lock.leave();
            l4_sleep(0);
            _lock.enter();
        }
        ++_measuring[utcb];
       _lock.leave();
    }
    void leaveMeasurement() const{
        l4_utcb_t * utcb = l4_utcb();
        _lock.enter();
        if(--_measuring[utcb] == 0) _measuring.erase(utcb);
       _lock.leave();
    }
    void enterExclusive() const {
        l4_utcb_t * utcb = l4_utcb();
        _lock.enter();
        while(!_locked.empty() && _locked.find(utcb) == _locked.end()){
           _lock.leave();
            l4_sleep(0);
            _lock.enter();
        }
        ++_locked[utcb];
        while(!_measuring.empty()){
           _lock.leave();
            l4_sleep(0);
            _lock.enter();
        }
       _lock.leave();
    }
    void leaveExclusive() const{
        l4_utcb_t * utcb = l4_utcb();
        _lock.enter();
        if(--_locked[utcb] == 0) _locked.erase(utcb);
       _lock.leave();
    }
};


class Item: public IItem{
private:
    l4_uint64_t _sum;
    l4_uint64_t _sumsq;
    l4_uint64_t _min;
    l4_uint64_t _max;
    l4_uint64_t _count;
public:
    Item(const std::string & name = "", const std::string & description = "")
    : IItem(name, description)
    {
        flush();
    }
    void addMeasurement(const l4_uint64_t value){
        GET_STATS.enterMeasurement();
        cas8Badd(&_sum, value);
        cas8Badd(&_sumsq, value * value);
        l4_uint64_t old_dummy = _min;
        do{
            if(old_dummy <= value) break;
        } while(!cas8Bcond(&_min, old_dummy, value));
        old_dummy = _max;
        do{
            if(old_dummy >= value) break;
        } while(!cas8Bcond(&_max, old_dummy, value));
        cas8Badd(&_count, 1);
        GET_STATS.leaveMeasurement();
    }
    virtual void print() const{
        double freq = l4re_kip()->frequency_cpu; // kHz
        freq /= 1000.0; // MHz (ticks per µs)
        double sum = (double)_sum / freq;
        double sumsq = (double)_sumsq / (freq * freq);
        double mean = sum/(double)_count;
        double var = (sumsq/(double)_count) - (mean * mean);
        double dev = 0;//sqrt(var);
        double min = (double)_min / freq;
        double max = (double)_max / freq;
        printf("%s: N:%llu abs:%f min:%f  avg:%f max:%f var:%f dev:%f\n"
                , _name.c_str(), _count, sum, min, mean, max, var, dev);
    }
    virtual void flush(){
        _sum = 0;
        _count = 0;
        _sumsq = 0;
        _min = 0xffffffffffffffffULL;
        _max = 0;
    }
    void setName(const std::string & name){
        _name = name;
    }
};

class ItemList : public IItem{
private:
    typedef std::map<l4_uint32_t, Item> item_map_t;
    item_map_t _item_map;
    mutex _item_map_mutex;
public:
    ItemList(const std::string & name, const std::string & description)
    : IItem(name, description)
    {
        flush();
    }
    void addMeasurement(const l4_uint32_t id, l4_uint64_t value){
        GET_STATS.enterMeasurement();
        _item_map_mutex.enter();
        item_map_t::iterator iter = _item_map.find(id);
        _item_map_mutex.leave();
        if(iter != _item_map.end()){
            iter->second.addMeasurement(value);
        } else {
            _item_map_mutex.enter();
            Item & item = _item_map[id];
            _item_map_mutex.leave();
            item.addMeasurement(value);
            std::stringstream s;
            s << "0x" << std::hex << id;
            item.setName(s.str());
        }
        GET_STATS.leaveMeasurement();
    }
    virtual void flush(){
        item_map_t::iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter) iter->second.flush();
    }
    virtual void print() const{
        printf("%s: \"%s\"\n", _name.c_str(), _description.c_str());
        item_map_t::const_iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++ iter){
            printf("\t");
            iter->second.print();
        }
    }
};

//#undef GET_STATS
//#define GET_STATS util::Singleton<util::Stats>::get()
#define STATS_ITEM_LIST typedef enum{
#define DECLARE_STATS_ITEM(name) SI_##name,
#define STATS_ITEM_LIST_END } stats_items_t;
#define DEFINE_STATS_ITEM(name, description, item_type)\
        template<>\
        class Singleton_traits<Tnumber<SI_ ## name> >{\
        public:\
            typedef item_type type;\
            static item_type & create(){\
                static item_type object(#name, description);\
                Singleton<Stats>::get().addItem((l4_uint32_t)SI_ ## name, &object);\
                return object;\
            }\
        };

#define GET_STATS_ITEM(name) if(false) util::Singleton<util::Tnumber<util::SI_ ## name> >::get()

} // namespace util

#include "stats_interrupt.hpp"

namespace util{

STATS_ITEM_LIST
DECLARE_STATS_ITEM(vmexit_reason)
DECLARE_STATS_ITEM(handle_interrupt)
DECLARE_STATS_ITEM(interrupt_latency)
DECLARE_STATS_ITEM(pf_while_x)
STATS_ITEM_LIST_END

DEFINE_STATS_ITEM(vmexit_reason, "Timing in µs of every vmexit path by exit reason", ItemList)
DEFINE_STATS_ITEM(handle_interrupt, "Timing in µs of every interrupt handling path by irq", ItemList)
DEFINE_STATS_ITEM(interrupt_latency, "Timing in µs of the time when the number of interrupts rise above zero and the the first interrupt injected since", Item_int_stats)
DEFINE_STATS_ITEM(pf_while_x, "Counting #PFs while interrupts are disabled (x = 0) or enabled (x = 1)", ItemList)

}
#endif /* STATISTICS_HPP_ */
