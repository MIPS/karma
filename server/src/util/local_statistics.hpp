/*
 * local_statistics.hpp - TODO enter description
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

#ifndef LOCAL_STATISTICS_HPP_
#define LOCAL_STATISTICS_HPP_

#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
//#include <math.h>
#include "cas.hpp"
#include <stdexcept>
#ifndef KARMA_OUTSIDE_L4RE
#include "../l4_lirq.h"
#include "../l4_vcpu.h"
#endif

namespace util{

//inline l4_uint64_t tsc() {
//    unsigned long long ret;
//    asm volatile("rdtsc":"=A"(ret));
//    return ret;
//}

#define _TO_STRING(x) #x
#define TO_STRING(x) _TO_STRING(x)
#define AT "@" __FILE__ ":" TO_STRING(__LINE__) ":"

typedef enum{
    TID_LIITEM = 0,
    TID_LITEM = 1,
    TID_LITEM_LIST = 2,
    TID_LITEM_HISTO = 3,
} TID_t;

class ESerialization: public std::runtime_error{
public:
    ESerialization(const std::string & what) : std::runtime_error(what) {}
};


template<typename STREAM, typename type>
struct _processField{
    inline void operator()(const std::string & field, STREAM & s, type & value){
        throw ESerialization(AT "unknown stream type");
    }
};

template<typename type>
struct _processField<std::istream, type>{
    inline void operator()(const std::string & field, std::istream & is, type & value){
        char buffer[512];
        if(field.size() >= 512){
            throw ESerialization(AT "field name to long");
        }
        is.read(buffer, field.size());
        buffer[field.size()] = 0;
        std::string dummy = buffer;
        if(dummy != field){
            std::stringstream s;
            s << AT << "Expected field \"" << field << "\" but read \"" << dummy << "\"";
            throw ESerialization(s.str());
        }
        is >> value;
        is.ignore(1);
    }
};
template<typename type>
struct _processField<std::ostream, type>{
    inline void operator()(const std::string & field, std::ostream & os, const type & value){
        os << field << value << " ";
    }
};
template<>
struct _processField<std::istream, std::string>{
    inline void operator()(const std::string & field, std::istream & is, std::string & value){
        char buffer[1024];
        if(field.size() >= 1024){
            throw ESerialization(AT "field name to long");
        }
        is.read(buffer, field.length());
        buffer[field.size()] = 0;
        std::string dummy = buffer;
        if(dummy != field){
            std::stringstream s;
            s << AT << "Expected field \"" << field << "\" but read \"" << dummy << "\"";
            throw ESerialization(s.str());
        }
        std::string::size_type length;
        _processField<std::istream, std::string::size_type> processField;
        processField("length:", is, length);
        is.ignore(1);
        is.read(buffer, length);
        buffer[length] = 0;
        value = buffer;
        is.ignore(2);
    }
};
template<>
struct _processField<std::ostream, std::string>{
    inline void operator()(const std::string & field, std::ostream & os, const std::string & value){
        if(value.length() >= 1024){
            throw ESerialization(AT "value to long");
        }

        os << field;
        _processField<std::ostream, std::string::size_type> processField;
        processField("length:", os, value.length());
        os << "\"" << value << "\" ";
    }
};

template<typename type>
inline static void processField(const std::string & field, std::istream & s, type & value){
    _processField<std::istream, type> do_process;
    do_process(field, s, value);
}
template<typename type>
inline static void processField(const std::string & field, std::ostream & s, const type & value){
    _processField<std::ostream, type> do_process;
    do_process(field, s, value);
}

class LIItem;

static LIItem * Type_IDcreate(unsigned int tid);

class LIItem{
protected:
    std::string _name;
    std::string _description;
public:
    LIItem(const std::string & name, const std::string & description)
    :   _name(name)
    ,   _description(description)
    {
    }
    virtual ~LIItem(){}
    virtual void print() const = 0;
    virtual void flush() = 0;

    virtual TID_t type_id() = 0;
    virtual void save(std::ostream & os) const {
        processField("name:", os, _name);
        processField("desc:", os, _description);
    }
    virtual void load(std::istream & is){
        processField("name:", is, _name);
        processField("desc:", is, _description);
    }
    const std::string & name() const {
        return _name;
    }
    const std::string & description() const {
        return _description;
    }
    static void saveItem(LIItem * item, std::ostream & os){
        processField("TID:", os, item->type_id());
        item->save(os);
    }
    static LIItem * loadItem(std::istream & is){
        unsigned int tid;
        processField("TID:", is, tid);
        LIItem * newItem = Type_IDcreate(tid);
        newItem->load(is);
        return newItem;
    }
};

class LStats : public L4_vcpu::Specific{
public:
    typedef std::vector<LIItem *> map_type;
private:
    map_type _item_map;
    int _id;
#ifndef KARMA_OUTSIDE_L4RE
    static util::mutex _lstats_map_lock;
    typedef std::vector<L4_lirq *> lstats_notifier_map_t;
    static lstats_notifier_map_t _lstats_notifier_map;
    L4_lirq _lirq;
#endif
public:
    LStats()
    {
    }
    void init(){
#ifndef KARMA_OUTSIDE_L4RE
        _lirq.lirq()->attach(0x20000000, L4::Cap<L4::Thread>());
        _lstats_map_lock.enter();
        _lstats_notifier_map.push_back(&_lirq);
        _lstats_map_lock.leave();
#endif
    }
    ~LStats()
    {
        map_type::const_iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter){
            delete (*iter);
        }
    }
    int id() const { return _id; }
    void id(const int value) { _id = value; }
    void addItem(const l4_uint32_t id, LIItem * item){
        if(_item_map.size() <= id){
            _item_map.resize(id + 1);
        }
        _item_map[id] = item;
    }
    LIItem & getItem(l4_uint32_t id){
        return *_item_map[id];
    }
    void print() const{
        map_type::const_iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter){
            std::stringstream s;
            (*iter)->print();
        }
    }
    void save(std::ostream & os) const {
        map_type::const_iterator iter = _item_map.begin();
        processField("cpu_id:", os, _id);
        processField("entries:", os, _item_map.size());
        for(; iter != _item_map.end(); ++iter){
            LIItem::saveItem(*iter, os);
        }
    }
    void load(std::istream & is){
        map_type::iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter){
            delete *iter;
        }
        _item_map.clear();
        map_type::size_type size;
        processField("cpu_id:", is, _id);
        processField("entries:", is, size);
        _item_map.resize(size);
        for(unsigned int i(0); i != size; ++i){
            _item_map[i] = LIItem::loadItem(is);
        }
    }
    void flush(){
        map_type::iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter){
            (*iter)->flush();
        }
    }
    map_type::const_iterator begin() const {
        return _item_map.begin();
    }
    map_type::const_iterator end() const {
        return _item_map.end();
    }
#ifndef KARMA_OUTSIDE_L4RE
    inline static void dumpAll(){
        for(unsigned int i(0); i < _lstats_notifier_map.size(); ++i){
            _lstats_notifier_map[i]->lirq()->trigger();
        }
    }
#endif
};



class LItem: public LIItem{
protected:
    l4_uint64_t _sum;
    l4_uint64_t _sumsq;
    l4_uint64_t _min;
    l4_uint64_t _max;
    l4_uint64_t _count;
    l4_uint64_t _kHz;
public:
    LItem(const std::string & name = "", const std::string & description = "")
    : LIItem(name, description)
    {
        flush();
#ifndef KARMA_OUTSIDE_L4RE
        _kHz = l4re_kip()->frequency_cpu; // kHz
#endif
    }
    void setKHz(const l4_uint64_t newKHz){
        _kHz = newKHz;
    }
    void addMeasurement(const l4_uint64_t value){
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
    }
    virtual TID_t type_id(){
        return TID_LITEM;
    }
    virtual void save(std::ostream & os) const{
        LIItem::save(os);
        const_cast<LItem*>(this)->serialize(os);
    }
    virtual void load(std::istream & is){
        LIItem::load(is);
        serialize(is);
    }
    template <typename STREAM>
    void serialize(STREAM & s){
        processField("sum:", s, _sum);
        processField("sumsq:", s, _sumsq);
        processField("min:", s, _min);
        processField("max:", s, _max);
        processField("count:", s, _count);
        processField("kHz:", s, _kHz);
    }
    virtual void print() const{
//#ifndef KARMA_OUTSIDE_L4RE
        double freq = _kHz; // kHz
        freq /= 1000.0; // MHz (ticks per µs)
        double sum = (double)_sum / freq;
        double sumsq = (double)_sumsq / (freq * freq);
        double mean = sum/(double)_count;
        double var = (sumsq/(double)_count) - (mean * mean);
        double dev = 0; //sqrt(var);
        double min = (double)_min / freq;
        double max = (double)_max / freq;
        printf("%s: N:%llu abs:%f min:%f  avg:%f max:%f var:%f dev:%f\n"
                , _name.c_str(), _count, sum, min, mean, max, var, dev);
//#endif
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

class LItemHist : public LItem{
public:
    enum { histo_max = 1000 };
private:
    l4_uint32_t _histo[histo_max];
    l4_uint32_t _overflows;
public:
    LItemHist(const std::string & name = "", const std::string & description = "")
    : LItem(name, description)
    {
        flush();
    }
    virtual void addMeasurement(const l4_uint64_t value){
        l4_uint64_t usecs = (value * 1000) / _kHz;
        if(usecs < histo_max){
            _histo[usecs]++;
        } else {
            _overflows++;
        }
        LItem::addMeasurement(value);
    }
    virtual TID_t type_id(){
        return TID_LITEM_HISTO;
    }
    virtual void flush(){
        LItem::flush();
        for(unsigned i(0); i < histo_max; ++i){
            _histo[i] = 0;
        }
        _overflows = 0;
    }
    virtual void save(std::ostream & os) const{
        LItem::save(os);
        const_cast<LItemHist*>(this)->serialize(os);
    }
    virtual void load(std::istream & is){
        LItem::load(is);
        serialize(is);
    }
    template <typename STREAM>
    void serialize(STREAM & s){
        processField("overflows:", s, _overflows);
        for(unsigned i(0); i < histo_max; ++i){
            processField("h:", s, _histo[i]);
        }
    }
    l4_uint32_t histoEntry(l4_uint32_t index){
        return _histo[index];
    }
};


class LItemList : public LIItem{
private:
    typedef std::map<l4_uint32_t, LItem> item_map_t;
    item_map_t _item_map;
public:
    LItemList(const std::string & name = "", const std::string & description = "")
    : LIItem(name, description)
    {
        flush();
    }
    void addMeasurement(const l4_uint32_t id, l4_uint64_t value){
        item_map_t::iterator iter = _item_map.find(id);
        if(iter != _item_map.end()){
            iter->second.addMeasurement(value);
        } else {
            LItem & item = _item_map[id];
            item.addMeasurement(value);
            std::stringstream s;
            s << "0x" << std::hex << id;
            item.setName(s.str());
        }
    }
    virtual void flush(){
        item_map_t::iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter) iter->second.flush();
    }
    virtual TID_t type_id(){
        return TID_LITEM_LIST;
    }
    virtual void save(std::ostream & os) const {
        LIItem::save(os);
        processField("entries:", os, _item_map.size());
        item_map_t::const_iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++iter){
            processField("key:", os, iter->first);
            iter->second.save(os);
        }
    }
    virtual void load(std::istream & is){
        LIItem::load(is);
        unsigned int size;
        processField("entries:", is, size);
        for(unsigned int i(0); i < size; ++i){
            l4_uint32_t key;
            processField("key:", is, key);
            _item_map[key].load(is);
        }
    }
    virtual void print() const{
//#ifndef KARMA_OUTSIDE_L4RE
        printf("%s: \"%s\"\n", _name.c_str(), _description.c_str());
        item_map_t::const_iterator iter = _item_map.begin();
        for(; iter != _item_map.end(); ++ iter){
            printf("\t");
            iter->second.print();
        }
//#endif
    }
};

static LIItem * Type_IDcreate(unsigned int tid){
    switch(tid){
    case TID_LIITEM:
        return NULL;
    case TID_LITEM:
        return new LItem();
    case TID_LITEM_LIST:
        return new LItemList();
    case TID_LITEM_HISTO:
        return new LItemHist();
    }
    return NULL;
}

template <typename ITEM_TYPE>
struct litem_cast{
    inline ITEM_TYPE * operator()(util::LIItem * item) const{
        return static_cast<ITEM_TYPE *>(item);
    }
    inline const ITEM_TYPE * operator()(const util::LIItem * item) const{
        return static_cast<ITEM_TYPE *>(item);
    }
    inline ITEM_TYPE & operator()(util::LIItem & item) const{
        return static_cast<ITEM_TYPE *>(item);
    }
    inline const ITEM_TYPE & operator()(const util::LIItem & item) const{
        return static_cast<ITEM_TYPE *>(item);
    }
};

//#undef GET_STATS
//#define GET_STATS util::Singleton<util::Stats>::get()
#define LSTATS_ITEM_LIST typedef enum{
#define DECLARE_LSTATS_ITEM(name) LSI_##name,
#define LSTATS_ITEM_LIST_END MAX_LSI} lstats_items_t;

template <unsigned int ITEM_ID>
class ItemTraits{

};

extern LStats & getLStats();

template <unsigned int ITEM_ID>
class LStatsInitializer{
private:
    typedef ItemTraits<ITEM_ID - 1> item_traits_t;
    typedef LStatsInitializer<ITEM_ID - 1> next_t;
public:
    static void init(){
        getLStats().addItem(ITEM_ID - 1, item_traits_t::create());
        next_t::init();
    }
};

template <>
class LStatsInitializer<0>{
public:
    static void init(){
    }
};

extern void initLStats();

#define LSTATS_INIT \
    util::initLStats(); \
    util::LStatsInitializer<(unsigned int)util::MAX_LSI>::init()

#define DEFINE_LSTATS_ITEM(name, description, item_type)\
        template<>\
        class ItemTraits<LSI_##name>{\
        public:\
            enum { id = LSI_##name };\
            typedef item_type type;\
            inline static type * create(){\
                return new type(#name, description);\
            }\
        };


#define GET_LSTATS_ITEM(name) static_cast<util::ItemTraits<util::LSI_##name>::type &>(util::getLStats().getItem(util::LSI_##name))

} // namespace util


namespace util{

LSTATS_ITEM_LIST
DECLARE_LSTATS_ITEM(vmexit_reason)
DECLARE_LSTATS_ITEM(handle_interrupt)
DECLARE_LSTATS_ITEM(pf_while_x)
DECLARE_LSTATS_ITEM(pageDB_insert)
DECLARE_LSTATS_ITEM(pageDB_unmap)
DECLARE_LSTATS_ITEM(pageDB_flush)
DECLARE_LSTATS_ITEM(lat_hpet2karma)
DECLARE_LSTATS_ITEM(lat_karma2guest)
DECLARE_LSTATS_ITEM(lat_apic_programming)
DECLARE_LSTATS_ITEM(lat_l4_fb_refresh)
LSTATS_ITEM_LIST_END


DEFINE_LSTATS_ITEM(vmexit_reason, "Timing in µs of every vmexit path by exit reason", LItemList)
DEFINE_LSTATS_ITEM(handle_interrupt, "Timing in µs of every interrupt handling path by irq", LItemList)
DEFINE_LSTATS_ITEM(pf_while_x, "Counting #PFs while interrupts are disabled (x = 0) or enabled (x = 1)", LItemList)
DEFINE_LSTATS_ITEM(pageDB_insert, "Timing in µs of pageDB insert operations", LItem)
DEFINE_LSTATS_ITEM(pageDB_unmap, "Timing in µs of pageDB unmap operations", LItem)
DEFINE_LSTATS_ITEM(pageDB_flush, "Timing in µs of pageDB flush operations", LItem)
DEFINE_LSTATS_ITEM(lat_hpet2karma, "Latency in µs of an HPET interrupt reaching karma", LItemHist)
DEFINE_LSTATS_ITEM(lat_karma2guest, "Latency in µs of an HPET event being injected into the guest", LItemHist)
DEFINE_LSTATS_ITEM(lat_apic_programming, "Latency in µs of an APIC programming hypercall reaching karma", LItemHist)
DEFINE_LSTATS_ITEM(lat_l4_fb_refresh, "Latency in µs of the vcpu setting of a fb refresh request",  LItemHist)
}
#endif /* LOCAL_STATISTICS_HPP_ */
