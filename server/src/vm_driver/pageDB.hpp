/*
 * pageDB.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * pageDB.hpp
 *
 *  Created on: 11.11.2010
 *      Author: janis
 */

#ifndef PAGEDB_HPP_
#define PAGEDB_HPP_

#include <map>


class shadowPageDB{
private:
    class Entry{
    public:
        Entry * next;
        l4_addr_t va;
        l4_umword_t size; //power of two
        bool global;
    };

    Entry _pages[0x00100000];
    Entry * _free;
    Entry * _mapped;
    Entry * _gmapped;

    L4::Cap<L4::Vm> _cap;

    void _in(Entry ** list, Entry * entry){
        entry->next = *list;
        *list = entry;
    }
    Entry * _out(Entry ** prev){
        Entry * dummy = (*prev);
        *prev = dummy->next;
        return dummy;
    }

    class FPBatch{
    private:
        enum { fpages_per_batch = ((L4_UTCB_GENERIC_DATA_SIZE - 2) * sizeof(l4_umword_t)) / sizeof(l4_fpage_t) };
        l4_fpage_t _fpages[fpages_per_batch];
        L4::Cap<L4::Vm> _cap;

        l4_umword_t _map_mask;
        l4_umword_t _count;
        inline void flush(){
            if(_count > 0){
                _cap->unmap_batch(_fpages, _count, _map_mask);
                _count = 0;
            }
        }
    public:
        FPBatch(L4::Cap<L4::Vm> cap, const l4_umword_t map_mask)
        :   _cap(cap)
        ,   _map_mask(map_mask)
        ,   _count(0){}
        ~FPBatch(){
            flush();
        }
        inline void addToBatch(const l4_fpage_t & fpage){
            if(_count == fpages_per_batch){
                flush();
            }
            _fpages[_count++] = fpage;
        }
    };

public:
    shadowPageDB(){
        _cap = L4Re::Util::cap_alloc.alloc<L4::Vm>();
        if(!_cap.is_valid())
        throw L4_CAPABILITY_EXCEPTION;

        chksys(L4Re::Env::env()->factory()->create_vm(_cap));
        _mapped = _free = NULL;

        for(unsigned int i(0); i != 0x100000; ++i){
            _in(&_free, &_pages[0xfffff - i]);
        }
    }
    void flush(bool include_global = false){
        FPBatch batch(_cap, L4_FP_ALL_SPACES);
        Entry ** helper = &_mapped;
        while(*helper){
            Entry * dummy = _out(helper);
            batch.addToBatch(l4_fpage(dummy->va, dummy->size, L4_FPAGE_RWX));
//            _cap->unmap(l4_fpage(dummy->va, dummy->size, L4_FPAGE_RWX), L4_FP_ALL_SPACES);
            _in(&_free, dummy);
        }
        if(!include_global) return;
        helper = &_gmapped;
        while(*helper){
            Entry * dummy = _out(helper);
            batch.addToBatch(l4_fpage(dummy->va, dummy->size, L4_FPAGE_RWX));
//            _cap->unmap(l4_fpage(dummy->va, dummy->size, L4_FPAGE_RWX), L4_FP_ALL_SPACES);
            _in(&_free, dummy);
        }
    }

    void insert(l4_addr_t va, l4_addr_t source, l4_umword_t size, bool rw, bool global = false){
        l4_umword_t mask = ~((1UL << size) - 1);
        va &= mask;
        source &= mask;
        if(!_free){
            printf("Not enough page entries ... about to fail\n");
            exit(1);
        }
        Entry * dummy = _out(&_free);

        dummy->size = size;
        dummy->va = va;
        if(global)
            _in(&_gmapped, dummy);
        else
            _in(&_mapped, dummy);

        _cap->map(L4Re::Env::env()->task(), l4_fpage(source, size, rw ? L4_FPAGE_RW : L4_FPAGE_RO), va);
    }

    void unmap(l4_addr_t va){
        l4_addr_t va4k = va & ~0xfffUL;
        l4_addr_t va4M = va & ~0x3fffffUL;
        Entry ** helper = &_mapped;
        while(*helper){
            if((*helper)->va == va4k || (*helper)->va == va4M)
                break;
            helper = &(*helper)->next;
        }
        // if not found yet check globally mapped paged
        if(!*helper){
            helper = &_gmapped;
            while(*helper){
                if((*helper)->va == va4k || (*helper)->va == va4M)
                    break;
                helper = &(*helper)->next;
            }
        }
        if(*helper){
            Entry * entry = _out(helper);
            _cap->unmap(l4_fpage(entry->va, entry->size, L4_FPAGE_RWX), L4_FP_ALL_SPACES);
            _in(&_free, entry);
        }
    }

    L4::Cap<L4::Vm> & cap() {
        return _cap;
    }
};

#endif /* PAGEDB_HPP_ */
