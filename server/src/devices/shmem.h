/**
 *
 * Copyright  (c) 2015 Elliptic Technologies
 *
 * \author  Jason Butler jbutler@elliptictech.com
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * shmem.h - provides a shared memory interface to another endpoint
 */

#pragma once

#include "device_interface.h"
#include "../thread.h"
#include "../l4_vcpu.h"
#include "../util/ringbuff.hpp"
#include "../util/buffer.hpp"
#include "../l4_lirq.h"

class L4_shm_prod;
class L4_shm_cons;

class Rx_prod_thread : public karma::Thread
{
    public:
        void set_prod(L4_shm_prod *a_prod);
        virtual void run(L4::Cap<L4::Thread> & thread);
        L4_shm_prod *my_prod;
};

class Tx_cons_thread : public karma::Thread
{
    public:
        void set_cons(L4_shm_cons *a_cons);
        virtual void run(L4::Cap<L4::Thread> & thread);
        L4_shm_cons *my_cons;
};

class L4_shm
{
    friend class L4_shm_prod;
    friend class L4_shm_cons;
private:

public:
    enum Role
    {
        Producer,
        Consumer,
    };
    L4_shm(Role role) : _role(role) {};

    static const char *Chunk_one;
    static const char *Signal_one;
    static const char *Signal_done;
    static const char *Chunk_two;
    static const char *Signal_two;
    static const char *Signal_done_2;

//    friend void L4_shm_prod::thread_producer();
//    friend void L4_shm_prod::thread_consumer();
protected:
    L4_vcpu * _waiting_vcpu;
    const char * _shm;
    Role _role;

/******** Hyper call members **************/
    unsigned counter;
    Buffer<16834> _read_buf;
    Buffer<16384> _write_buf;
    Buffer32<256> _cmd_buf;
    Buffer32<256> _cmd_buf_prod;
    l4_uint32_t read_size;

    char str[1024+1]; // +1 for string zero terminate
    l4_addr_t _shared_mem;   //Hyper-calls use this pointer to pass data to L4_shm
    l4_addr_t _shared_mem_read;  //For reads
    l4_addr_t _base_ptr;
    L4::Cap<L4::Irq> irq;
    //L4_lirq _shmem_irq;
};

class L4_shm_prod : public L4_shm, public device::IDevice, protected karma::Thread
{
public:
    L4_shm_prod();
    void init(const char * shm_cap);
    virtual void hypercall(HypercallPayload &);
    Rx_prod_thread rx_thread;
    void thread_producer();
    void thread_consumer();
    karma::PTCondBool _let_run;
    karma::PTCondBool _block_vm;

protected:
    virtual void run(L4::Cap<L4::Thread> & thread);

};

class L4_shm_cons : public L4_shm, public device::IDevice, protected karma::Thread
{
public:
    L4_shm_cons();
    void init(const char * shm_cap);
    virtual void hypercall(HypercallPayload &);
    Tx_cons_thread tx_thread;
    void thread_consumer();
    void thread_producer();
    karma::PTCondBool _let_run;

protected:
    virtual void run(L4::Cap<L4::Thread> & thread);

};
