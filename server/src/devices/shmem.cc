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
 * shmem.cc - provides a shared memory interface to another endpoint
 */

#include <string.h>

#include "shmem.h"
#include "../l4_vm.h"
#include "../l4_vcpu.h"
#include "../l4_exceptions.h"
#include "devices_list.h"

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>

#include <l4/sys/types.h>
#include <l4/shmc/shmc.h>
#include <l4/util/util.h>
#include <l4/sys/thread.h>
#include <stdio.h>
#include <string.h>
#include <pthread-l4.h>
#include <l4/sys/debugger.h>

#include "../util/debug.h"
#include "mipsregs.h"


/*
 * NOTE: The l4shmc API does not provide any way to recover lost signals. If
 * l4shmc_wait_chunk() or l4shmc_wait_signal() are not called before the other
 * side sends a signal, that signal will be lost and the waitee will hang.
 *
 *
 * The printf statements may be printed in a different order dependent on
 * preemption and scheduling.
 */

#define CHK(func) \
{ \
    int err = (func); \
    if (err) { \
        karma_log(ERROR, "[shmem] Error %s: line %d\n", l4sys_errtostr(err),  __LINE__); \
        return; \
    } \
}

#define L4_SUCCESS 0


//#define LOGGING_ALL

#ifdef LOGGING_ALL

#define DO_LOGGING ERROR
#define DO_LOGGING_ON ERROR
#define DO_LOGGING_A ERROR
#define DO_LOGGING_shmem_prod_PRODUCER ERROR
#define DO_LOGGING_shmem_prod_CONSUMER ERROR
#define DO_LOGGING_shmem_cons_PRODUCER ERROR
#define DO_LOGGING_shmem_cons_CONSUMER ERROR

#else

#define DO_LOGGING DEBUG//ERROR
#define DO_LOGGING_ON DEBUG
#define DO_LOGGING_A DEBUG
#define DO_LOGGING_shmem_prod_PRODUCER DEBUG
#define DO_LOGGING_shmem_prod_CONSUMER DEBUG
#define DO_LOGGING_shmem_cons_PRODUCER DEBUG
#define DO_LOGGING_shmem_cons_CONSUMER DEBUG

#endif

#define DS_SIZE 10
#define DS_SIZE_CMD 1
#define READ_SIZE_CMD 2

static char some_data[] = "0-Hi consumer!";

void set_some_data(void);
void set_some_data(void) {
  static int i = 0;
  some_data[0] = '0' + i++;
  i %= 10;
}

const char *L4_shm::Chunk_one = "chunk_one";
const char *L4_shm::Chunk_two = "chunk_two";
const char *L4_shm::Signal_one = "sig_one";
const char *L4_shm::Signal_two = "sig_two";
const char *L4_shm::Signal_done = "sig_done";
const char *L4_shm::Signal_done_2 = "sig_done_2";

void
Rx_prod_thread::set_prod(L4_shm_prod *a_prod)
{
    my_prod = a_prod;
}

void
Rx_prod_thread::run(L4::Cap<L4::Thread> &)
{
    karma_log(ERROR, "HELLO FROM SECOND PRODUCER THREAD!!!!!\n");
    my_prod->thread_consumer();
}

void
Tx_cons_thread::set_cons(L4_shm_cons *a_cons)
{
    my_cons = a_cons;
}

void
Tx_cons_thread::run(L4::Cap<L4::Thread> &)
{
   karma_log(ERROR, "HELLO FROM SECOND CONSUMER THREAD!!!!!!!\n");
   my_cons->thread_producer();
}

L4_shm_prod::L4_shm_prod()
        : L4_shm(L4_shm::Producer), karma::Thread(2, "L4_shm_prod"), _let_run(false), _block_vm(false)
{
}

void
L4_shm_prod::init(const char * shm_cap)
{
    _shm = shm_cap;
    karma_log(ERROR, "[shmem_prod] Initializing Dummy interrupt\n");
    rx_thread.set_prod(this);
    irq = L4Re::chkcap(L4Re::Util::cap_alloc.alloc<L4::Irq>());
    L4Re::chksys(L4Re::Env::env()->factory()->create_irq(irq), "[shmem_prod] IRQ creation failed");
    GET_VM.gic().attach(irq, 3);
    start();
    rx_thread.start();
}

void
L4_shm_prod::hypercall(HypercallPayload &hp)
{
    int tmp_count;
    switch (hp.address())
    {
        char c;
        case karma_shmem_ds_init:
            GET_VM.mem().phys_to_local(MIPS_KSEG0_TO_PHYS(hp.reg(0)), &_shared_mem);
            GET_VM.mem().phys_to_local(MIPS_KSEG0_TO_PHYS(hp.reg(1)), &_shared_mem_read);
            karma_log(ERROR, "[shmem_prod] _shared_mem = %lx\n", _shared_mem);
            break;
        case karma_shmem_ds_write:
           if(!_shared_mem)
               break;
           karma_log(DO_LOGGING_ON, "[shmem_prod] line: %d write called  %.*s\n", __LINE__, 10, (char*)_shared_mem );
           if(!_write_buf.memcpy_to((void*)(_shared_mem), hp.reg(0)))
               karma_log(ERROR, "[shmem_prod} line: %d High speed buffer copy failed! Not enough room in buffer\n", __LINE__);
           karma_log(DO_LOGGING_ON, "[shmem_prod] line: %d letting Tx thread run\n", __LINE__);
           break;
        case karma_shmem_ds_read:
            c = 0;
            if (_read_buf.count())
            {
                karma_log(DO_LOGGING_ON, "[shmem_prod] line: %d\n", __LINE__ );
                if(_read_buf.get(&c))
                {
                    karma_log(DO_LOGGING_ON, "[shmem_prod] line: %d\n", __LINE__ );

                    if(c)
                    {
                        karma_log(DO_LOGGING_ON, "[shmem_prod] char: %c %x\n", c, c);

                        hp.reg(0) = c;
                        break;
                    }
                }
            }
            hp.reg(0) = -1;
            break;

            case karma_shmem_ds_shared_mem_read_baremetal:
                 if(!_shared_mem_read)
                     break;
                 int bytes_to_read;


                 bytes_to_read = hp.reg(0);
                 karma_log(DO_LOGGING_ON, "[shmem_prod] hypercall: shared_mem_read_baremetal line: %d\n", __LINE__);
                 _read_buf.waitConditional_memcpy_from(bytes_to_read, (void*)(_shared_mem_read));
                 karma_log(DO_LOGGING_ON, "[shmem_prod] hypercall: line: %d shared_mem_read_baremetal: unblock %d bytes\n",
                            __LINE__, bytes_to_read);

                 break;
            case karma_shmem_ds_shared_mem_read:
                 if(!_shared_mem_read)
                     break;
                 int bytes_read;
                 bytes_read = 0;
                 karma_log(DO_LOGGING_ON, "[shmem_prod] line: %d shared_mem_read: %lu bytes\n", __LINE__, hp.reg(0));
                 for(unsigned int j = 0; j < hp.reg(0); j++)
                 {
                     _read_buf.get((char*)(_shared_mem_read+j));
                     bytes_read++;
                 }
                 hp.reg(0) = bytes_read;
                 break;
            case karma_shmem_set_ds_size:
                _cmd_buf.put(DS_SIZE_CMD);
                _cmd_buf.put(hp.reg(0));
                break;
            case karma_shmem_get_bytes_available:
                tmp_count = _read_buf.count();
                karma_log(DEBUG, "[shmem_prod] line: %d bytes_available: %d\n", __LINE__, tmp_count);
                hp.reg(0) = tmp_count;
                break;
        default:
            karma_log(ERROR, "[shmem_prod] unknown hypercall (%x). Ignoring.\n", hp.address());
    }
}

L4_shm_cons::L4_shm_cons()
        : L4_shm(L4_shm::Consumer), karma::Thread(2, "L4_shm_cons"), _let_run(false) {}

void
L4_shm_cons::init(const char * shm_cap)
{
    _shm = shm_cap;
    karma_log(ERROR, "[shmem_con] Initializing Dummy interrupt\n");
     irq = L4Re::chkcap(L4Re::Util::cap_alloc.alloc<L4::Irq>());
    tx_thread.set_cons(this);
    L4Re::chksys(L4Re::Env::env()->factory()->create_irq(irq), "[shmem_con] IRQ creation failed");
    GET_VM.gic().attach(irq, 3);
    start();

    tx_thread.start();
}

void
L4_shm_cons::hypercall(HypercallPayload &hp)
{

    int tmp_count;
    switch (hp.address())
    {
        case karma_shmem_ds_init:
            GET_VM.mem().phys_to_local(MIPS_KSEG0_TO_PHYS(hp.reg(0)), &_shared_mem);
            GET_VM.mem().phys_to_local(MIPS_KSEG0_TO_PHYS(hp.reg(1)), &_shared_mem_read);
            karma_log(ERROR, "[shmem_cons] _shared_mem = %lx\n", _shared_mem);
        break;
        case karma_shmem_ds_write:
           if(!_shared_mem)
               break;
           karma_log(DO_LOGGING_ON, "[shmem_cons] line: %d write called  %.*s\n", __LINE__, 10,(char *) _shared_mem );

           if(!_write_buf.memcpy_to((void*)(_shared_mem), hp.reg(0)))
               karma_log(ERROR, "[shmem_cons] line: %d High speed buffer copy failed! Not enough space in buffer\n", __LINE__);

           karma_log(DO_LOGGING_ON, "[shmem_cons] hypercall: line: %d _write_buf.count(): %d\n", __LINE__, _write_buf.count());
        break;
        case karma_shmem_ds_read:
            if(!_shared_mem_read)
                break;

            for(unsigned int j = 0; j < hp.reg(0); j++)
            {
               _read_buf.get((char*)(_shared_mem_read+j));
            }

            break;
         case karma_shmem_set_ds_size:
               _cmd_buf.put(DS_SIZE_CMD);
               _cmd_buf.put(hp.reg(0));
            break;
         case karma_shmem_ds_set_read_size:
               karma_log(DO_LOGGING_ON, "[shmem_cons] hypercall: line: %d shmem_ds_set_read_size: %lu _read_buf.count(): %d\n",
                         __LINE__, hp.reg(0), _read_buf.count());

/*********** Critical Secion Start ***********************/
/**********  in between the logic filter on the buffer *****/
/**********  and setting the read_size the Rx thread  ******/
/*********   could run and add to the buffer         *******/
               _read_buf.lockBuffer();
               if(_read_buf.count() < hp.reg(0))
               {
                   karma_log(DO_LOGGING_ON, "[shmem_cons] hypercall: line: %d shmem_ds_set_read_size requested number of bytes not available. Requesting interrupt....\n",
                             __LINE__);
                 l4util_xchg32(&read_size, hp.reg(0));
               }
               hp.reg(0) = _read_buf.count();
               _read_buf.unlockBuffer();
/*********** Critical section end ***********************/
            break;

         case karma_shmem_ds_shared_mem_read:
            if(!_shared_mem_read)
               break;
            karma_log(DO_LOGGING_ON, "[shmem_cons] line: %d hypercall: shared_mem_read: %lu bytes, _read_buf.count(): %d\n"
                      , __LINE__, hp.reg(0), _read_buf.count());

/************ Critical Section Start ***********************/
/************ in between copying the data  *****************/
/************ to the read buffer and   ********************/
/************ cancelling the interrupt *******************/
/************ request the Rx thread could ****************/
/***********  run again and fire another interrupt *******/

            _read_buf.lockBuffer();
            _read_buf.memcpy_from((void*)(_shared_mem_read), hp.reg(0), 1);

             l4util_xchg32(&read_size, 0);

            _read_buf.unlockBuffer();

/*********** Critical Section End **************************/

             karma_log(DO_LOGGING_ON, "[shmem_cons] line: %d hypercall: shared_mem_read complete\n", __LINE__);

             break;

            case karma_shmem_get_bytes_available:
                tmp_count = _read_buf.count();
                karma_log(DO_LOGGING_ON, "[shmem_cons] line: %d hypercall: bytes_available: %d\n", __LINE__, tmp_count);
                hp.reg(0) = tmp_count;
                break;
        default:
            karma_log(ERROR, "[shmem_cons] unknown hypercall (%x). Ignoring.\n", hp.address());
    }
}

void
L4_shm_prod::thread_consumer()
{
    l4shmc_area_t shmarea;
    l4shmc_chunk_t p_two;
    l4shmc_signal_t s_two, s_done_2;

    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "prod_rx_thread");

    // attach this thread to the shm object
    CHK(l4shmc_attach(_shm, &shmarea));

    /******* Consumer Init *****************************/
    karma_log(DO_LOGGING_A, "line: %d\n", __LINE__);

    CHK(l4shmc_get_chunk(&shmarea, L4_shm::Chunk_two, &p_two));

    karma_log(DO_LOGGING_A, "line: %d\n", __LINE__);

    CHK(l4shmc_connect_chunk_signal(&p_two, &s_two));

    CHK(l4shmc_get_signal(&shmarea, L4_shm::Signal_done_2, &s_done_2));

    karma_log(DO_LOGGING_A, "line: %d\n", __LINE__);



    CHK(l4shmc_attach_signal_to(&shmarea, L4_shm::Signal_two,
                                pthread_getl4cap(pthread_self()), 10000, &s_two));

    karma_log(DO_LOGGING_A, "line: %d\n", __LINE__);

    while(1)
    {

       karma_log(DO_LOGGING, "[shmem_prod] Consumer: line: %d\n", __LINE__);

       if (L4_SUCCESS == l4shmc_wait_chunk(&p_two))
       {

             karma_log(DO_LOGGING_ON, "[shmem_prod] Consumer line %d : Received from chunk one: %s %x\n",
                  __LINE__, (char *)l4shmc_chunk_ptr(&p_two), (unsigned)l4shmc_chunk_ptr(&p_two));

             karma_log(DO_LOGGING, "[shmem_prod] Consumer line: %d chunk_size: %ld\n", __LINE__, l4shmc_chunk_size(&p_two));

             if(!_read_buf.memcpy_to(l4shmc_chunk_ptr(&p_two), l4shmc_chunk_size(&p_two)))
                 karma_log(ERROR, "[shmem_prod] Consumer line: %d high speed buffer copy failed! not enough space in buffer\n", __LINE__);

             karma_log(DO_LOGGING, "[shmem_prod] Consumer line: %d\n", __LINE__);
             CHK(l4shmc_chunk_consumed(&p_two));
             karma_log(DO_LOGGING_shmem_prod_CONSUMER, "[shmem_prod] Consumer: line: %d triggering done signal\n", __LINE__);

             CHK(l4shmc_trigger(&s_done_2));
             karma_log(DO_LOGGING_ON, "[shmem_prod] Consumer: line: %d \n", __LINE__);

         }

   }
}

void
L4_shm_prod::thread_producer()
{
    l4shmc_chunk_t p_one;
    l4shmc_signal_t s_one, s_done;
    l4shmc_area_t shmarea;
    l4_umword_t chunk_size = 0;

    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "prod");

    karma_log(INFO, "[shmem] Hello from producer thread!\n");

    // attach this thread to the shm object
    CHK(l4shmc_attach(_shm, &shmarea));
    karma_log(DO_LOGGING, "line: %d\n", __LINE__);

    // get chunk
    CHK(l4shmc_get_chunk(&shmarea, L4_shm::Chunk_one, &p_one));
    karma_log(DO_LOGGING, "line: %d\n", __LINE__);


    // get producer signal
    CHK(l4shmc_get_signal(&shmarea, L4_shm::Signal_one, &s_one));
    karma_log(DO_LOGGING, "line: %d\n", __LINE__);

    // connect chunk and signal
    CHK(l4shmc_connect_chunk_signal(&p_one, &s_one));
    karma_log(DO_LOGGING, "line: %d\n", __LINE__);


    CHK(l4shmc_attach_signal_to(&shmarea, L4_shm::Signal_done,
                                pthread_getl4cap(pthread_self()), 10000, &s_done));

    while (1)
    {
        karma_log(DO_LOGGING, "[shmem_prod] Producer: line: %d\n", __LINE__);
        _write_buf.waitConditional(1);
        karma_log(DO_LOGGING, "[shmem_prod] Producer: line: %d\n", __LINE__);

        while (l4shmc_chunk_try_to_take(&p_one))
            karma_log(WARN, "[shmem_prod] Producer: l4shmc_chunk_try_to_take re-try; should not happen!\n");

        karma_log(DO_LOGGING, "[shmem_prod] Producer: line: %d\n", __LINE__);

        _write_buf.lockBuffer();
        if((chunk_size = _write_buf.count()))  //This is not a bug I want to do if(_write_buf.count()) and chunck_size = _write_buf.count() at the same time
        {
            _write_buf.memcpy_from(l4shmc_chunk_ptr(&p_one), chunk_size, 1);
        }
        _write_buf.unlockBuffer();

        karma_log(DO_LOGGING_shmem_prod_PRODUCER, "[shmem_prod] Producer: line: %d data ready signalled. %c %x chunk_size: %lu\n",
                   __LINE__, *(char*)l4shmc_chunk_ptr(&p_one), (unsigned)l4shmc_chunk_ptr(&p_one), chunk_size);
        CHK(l4shmc_chunk_ready_sig(&p_one, chunk_size));
        karma_log(DO_LOGGING, "[shmem_prod] line: %d\n", __LINE__);
        chunk_size = 0;
        CHK(l4shmc_wait_signal(&s_done));
        karma_log(DO_LOGGING_shmem_prod_PRODUCER, "[shmem_prod] Producer: line: %d\n", __LINE__);
    }

    karma_log(DO_LOGGING, "line: %d\n", __LINE__);
    l4_sleep_forever();
}

void
L4_shm_prod::run(L4::Cap<L4::Thread> &)
{
    thread_producer();
}

void
L4_shm_cons::thread_producer()
{
    l4shmc_area_t shmarea;
    l4shmc_chunk_t p_two;
    l4shmc_signal_t s_two, s_done_2;
    l4_umword_t chunk_size = 0;

    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "cons_tx");
    // attach to shared memory area
    CHK(l4shmc_attach(_shm, &shmarea));

     /*********** Producer Init ***********************/

    CHK(l4shmc_get_chunk(&shmarea, L4_shm::Chunk_two, &p_two));

    CHK(l4shmc_get_signal(&shmarea, L4_shm::Signal_two, &s_two));

    CHK(l4shmc_connect_chunk_signal(&p_two, &s_two));

    CHK(l4shmc_attach_signal_to(&shmarea, L4_shm::Signal_done_2,
                                pthread_getl4cap(pthread_self()), 10000, &s_done_2));

    while(1)
    {
        _write_buf.waitConditional(1);

        karma_log(DO_LOGGING, "[shmem_con] Producer: line: %d\n", __LINE__);
        while (l4shmc_chunk_try_to_take(&p_two))
            karma_log(WARN, "[shmem_con] Producer: l4shmc_chunk_try_to_take re-try; should not happen!\n");

        _write_buf.lockBuffer();
        if((chunk_size = _write_buf.count()))
        {
            _write_buf.memcpy_from(l4shmc_chunk_ptr(&p_two), chunk_size, 1);
        }
        _write_buf.unlockBuffer();

        karma_log(DO_LOGGING_shmem_cons_PRODUCER, "[shmem_cons] Producer: line: %d  data ready signalled. %.*s %x\n",
                                 __LINE__, 10, (char *)l4shmc_chunk_ptr(&p_two), *(char*)l4shmc_chunk_ptr(&p_two));
        CHK(l4shmc_chunk_ready_sig(&p_two, chunk_size));
        chunk_size = 0;
        karma_log(DO_LOGGING, "[shmem_cons] Producer: line: %d\n", __LINE__);

        CHK(l4shmc_wait_signal(&s_done_2));
        karma_log(DO_LOGGING_shmem_cons_PRODUCER, "[shmem_cons] Producer: line: %d\n", __LINE__);
    }
}

void
L4_shm_cons::thread_consumer()
{
    l4shmc_area_t shmarea;
    l4shmc_chunk_t p_one;
    l4shmc_signal_t s_one, s_done;

    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "cons");

    karma_log(INFO, "[shmem_cons] Hello from consumer thread!\n");

    // attach to shared memory area
    CHK(l4shmc_attach(_shm, &shmarea));

    // get chunk 'one'
    CHK(l4shmc_get_chunk(&shmarea, L4_shm::Chunk_one, &p_one));

    // add a signal
    CHK(l4shmc_get_signal(&shmarea, L4_shm::Signal_done, &s_done));

    // attach signal to this thread
    CHK(l4shmc_attach_signal_to(&shmarea, L4_shm::Signal_one,
                                pthread_getl4cap(pthread_self()), 10000, &s_one));

    // connect chunk and signal
    CHK(l4shmc_connect_chunk_signal(&p_one, &s_one));

    karma_log(INFO, "[shmem_cons] Consumer: READY\n");

    while (1)
    {

            karma_log(DO_LOGGING, "[shmem_con] Consumer: line: %d trying to take chunk....\n", __LINE__);

            if (L4_SUCCESS == l4shmc_wait_chunk(&p_one))
            {

                karma_log(DO_LOGGING_ON, "[shmem_con] Consumer: line: %d chunk_size: %ld, Received from chunk one: %s %x\n",
                   __LINE__, l4shmc_chunk_size(&p_one), (char *)l4shmc_chunk_ptr(&p_one), (unsigned)l4shmc_chunk_ptr(&p_one));
                if(!_read_buf.memcpy_to(l4shmc_chunk_ptr(&p_one), l4shmc_chunk_size(&p_one)))
                   karma_log(ERROR, "[shmem_con] Consumer: line: %d high speed buffer copy failed! Not enough room in buffer\n", __LINE__);

                karma_log(DO_LOGGING_shmem_cons_CONSUMER, "[shmem_cons] Consumer: line: %d\n", __LINE__);
                memset(l4shmc_chunk_ptr(&p_one), 0, l4shmc_chunk_size(&p_one));
                karma_log(DO_LOGGING_shmem_cons_CONSUMER, "[shmem_cons] Consumer: line: %d\n", __LINE__);

                CHK(l4shmc_chunk_consumed(&p_one));

                karma_log(DO_LOGGING_shmem_cons_CONSUMER, "[shmem_cons] Consumer: line: %d\n", __LINE__);

                CHK(l4shmc_trigger(&s_done));

                karma_log(DO_LOGGING_shmem_cons_CONSUMER, "[shmem_cons] Consumer: line: %d read_size: %u _read_buf.count(): %u\n"
                          , __LINE__, read_size, _read_buf.count());

                _read_buf.lockBuffer();
                if(read_size && (_read_buf.count() >= read_size))
                {
                   l4util_xchg32(&read_size, 0);
                   karma_log(DO_LOGGING_ON, "[shmem_cons] line: %d triggering dummy irq\n", __LINE__);
                   irq->trigger();
                }
                _read_buf.unlockBuffer();

            }

    }
}

void
L4_shm_cons::run(L4::Cap<L4::Thread> &)
{
    thread_consumer();
}
