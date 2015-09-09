/*
 * ringbuff.hpp
 *
 *  Created on: 11.11.2010
 *      Author: janis
 */

#ifndef RINGBUFF_HPP_
#define RINGBUFF_HPP_

#include <malloc.h>
#include "../l4_lirq.h"

namespace util{

class ERingBufferOverflow{};

template <typename ENTRY_TYPE>
class newAllocator{
public:
    typedef ENTRY_TYPE entry_t;
    inline static entry_t * create(unsigned int size){
        return new entry_t[size];
    }
    inline static void destroy(entry_t * buffer, unsigned int){
        delete [] buffer;
    }
};

template <typename ENTRY_TYPE>
class AllignmentAllocator{
public:
    typedef ENTRY_TYPE entry_t;
    inline static entry_t * create(unsigned int size){
        unsigned int size_in_bytes = size * sizeof(entry_t);
        size_in_bytes += 0xfff;
        size_in_bytes &= ~0xfff;
        void * buffer = memalign(0x1000, size_in_bytes);
        return new (buffer) entry_t[size];
    }
    inline static void destroy(entry_t * buffer, unsigned int size){
        for(unsigned int i(0); i != size; ++i){
            buffer->~entry_t();
        }
        free(buffer);
    }

};

template <typename ENTRY_TYPE, unsigned int SIZE, template <typename _ENTRY_TYPE> class ALLOCATOR = newAllocator>
class TRingBuffer{
public:
    typedef ENTRY_TYPE entry_t;
    enum { size = SIZE };
    typedef ALLOCATOR<ENTRY_TYPE> allocator_t;
private:
    entry_t * _buffer;
    unsigned int _in_pos, _out_pos;
    unsigned int _fill;
public:
    TRingBuffer()
    :   _buffer(allocator_t::create(size))
    ,   _in_pos(0)
    ,   _out_pos(0)
    ,   _fill(0)
    {
    }
    ~TRingBuffer(){
        allocator_t::destroy(_buffer, size);
    }
    inline void put(const entry_t & entry){
        if(_fill < size){
            _buffer[_in_pos] = entry;
            _in_pos = (_in_pos + 1) % size;
            ++_fill;
        } else {
            throw ERingBufferOverflow();
        }
    }
    inline bool get(entry_t & entry){
        if(_fill){
            entry = _buffer[_out_pos];
            _out_pos = (_out_pos + 1) % size;
            --_fill;
            return true;
        }
        return false;
    }
    inline bool empty() const{
        return _fill == 0;
    }
};

class TwoWaySignal{
private:
    L4_lirq _writerIrq;
    L4_lirq _readerIrq;
public:
    TwoWaySignal(): _writerIrq(), _readerIrq(){}
    void attachWriter(int label, L4::Cap<L4::Thread> * writer = NULL){
        L4::Cap<L4::Thread> thread;
        if(writer) thread = *writer;
        else thread.invalidate();
        _writerIrq.lirq()->attach(label, thread);
    }
    void attachReader(int label, L4::Cap<L4::Thread> * reader = NULL){
        L4::Cap<L4::Thread> thread;
        if(reader) thread = *reader;
        else thread.invalidate();
        _readerIrq.lirq()->attach(label, thread);
    }
    inline void signalWriter(){
        _writerIrq.lirq()->trigger();
    }
    inline void signalReader(){
        _readerIrq.lirq()->trigger();
    }
    inline void waitWriter(){
        _writerIrq.lirq()->receive();
    }
    inline void waitReader(){
        _readerIrq.lirq()->receive();
    }
    L4::Cap<L4::Irq> & writerCap(){
        return _writerIrq.lirq();
    }
    L4::Cap<L4::Irq> & readerCap(){
        return _readerIrq.lirq();
    }
};

} // namespace util
#endif /* RINGBUFF_HPP_ */
