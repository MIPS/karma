/*
 * l4_net.cc - TODO enter description
 * 
 * (c) 2011 Jan Nordholz <jnordholz@sec.t-labs.tu-berlin.de>
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
//#define DEBUG_LEVEL DEBUG
#include "net.h"
#include "../l4_vm.h"
#include "../params.h"
#include "../l4_exceptions.h"
#include "../util/debug.h"

#include <l4/sys/factory>
#include <l4/re/c/dataspace.h>

#include <malloc.h> // memalign

#define L4_NET_GET_INFO             0x2
#define L4_NET_SEND_BUF_ADDR        0x8
#define L4_NET_RECV_BUF_ADDR        0xA
#define L4_NET_GET_BUF_SIZE         0xE
#define L4_NET_GET_IRQ              0x10
#define L4_NET_SEND_PACKET          0x20
#define L4_NET_RECV_SIZE            0x30


#define BUF_SIZE                    16*1024

L4_net::L4_net()
: karma::Thread(0, "L4 NET IRQ")
{}
void L4_net::init(unsigned int irq){
    _intr = irq;
    int res;

    l4ankh_init();

    karma_log(DEBUG, "l4ankh_open(shm_name=%s, bufsize=%d)\n", params.ankh_shm, BUF_SIZE);
    res = l4ankh_open(params.ankh_shm, BUF_SIZE);
    if(res)
    {
        printf("%s:%d (%s): Error l4ankh_open(%s, %d) = %d\n", __FILE__, __LINE__, __func__, params.ankh_shm, BUF_SIZE, res);
        throw L4_EXCEPTION("l4ankh_open");
    }

    struct AnkhSessionDescriptor *desc = l4ankh_get_info();
    karma_log(INFO, "mac = %0X:%0X:%0X:%0X:%0X:%0X\n", desc->mac[0], desc->mac[1], desc->mac[2], desc->mac[3], desc->mac[4], desc->mac[5]);
    karma_log(INFO, "mtu = %d\n", desc->mtu);
    memcpy(&_ankh_info, desc, sizeof(AnkhSessionDescriptor));

    karma_log(DEBUG, "l4ankh_prepare_send(cap=%ld)\n", pthread_getl4cap(pthread_self()));
    res = l4ankh_prepare_send(pthread_getl4cap(pthread_self()));
    if(res)
    {
        printf("%s:%d: Error l4ankh_prepare_send(cap=%d) = %d\n", __FILE__, __LINE__, (unsigned)pthread_getl4cap(pthread_self()), res);
        throw L4_EXCEPTION("l4ankh_prepare_send");
    }
    
    karma_log(DEBUG, "allocating send buffer: %d bytes\n", BUF_SIZE);
    karma_log(DEBUG, "allocation recv buffer: %d bytes\n", BUF_SIZE);
    _send_buf = (char*)memalign(BUF_SIZE, 4096);
    if(!_send_buf)
    {
        printf("allocation of %d bytes send buffer failed\n", BUF_SIZE);
        throw L4_EXCEPTION("allocate send buffer failed");
    }
    _recv_buf = (char*)memalign(BUF_SIZE, 4096);
    if(!_recv_buf)
    {
        printf("allocation of %d bytes recv buffer failed\n", BUF_SIZE);
        throw L4_EXCEPTION("allocate recv buffer failed");
    }

    GET_VM.gic().attach(_net_irq.lirq(), _intr);
    start();
//    start_irq_thread();
//    irq_init();
}

void L4_net::hypercall(HypercallPayload & hp){
    if(hp.address() & 1){
        write(hp.address() & ~1UL, hp.reg(0));
    } else {
        hp.reg(0) = read(hp.address());
    }
}

void L4_net::write(l4_umword_t addr, l4_umword_t val) {
    l4_addr_t target = 0;
    switch(addr)
    {
        case L4_NET_GET_INFO:
            target = (l4_addr_t)(GET_VM.mem().base_ptr() + val);
            memcpy((void*)target, &_ankh_info, sizeof(struct AnkhSessionDescriptor));
            break;
        case L4_NET_SEND_PACKET:
            karma_log(DEBUG, "sending packet of length %ld via ankh\n", val);
            l4ankh_send(_send_buf, val, 1);
            break;
        default:
            break;
    }
}

l4_umword_t L4_net::read(l4_umword_t addr) {
    l4_umword_t ret = -1;
    l4_addr_t _addr = 0;
    switch(addr)
    {
        case L4_NET_SEND_BUF_ADDR:
            if(GET_VM.mem().register_iomem(&_addr, BUF_SIZE) == false)
            {
                printf("%s:%d Error could not register send iomem of size %d\n", __FILE__, __LINE__,  BUF_SIZE);
                throw L4_EXCEPTION("regsiter iomem");
            }
            if(!GET_VM.mem().map_iomem(_addr, (l4_addr_t)_send_buf, 1))
            {
                printf("%s:%d Error could not map send io memory from karma virt %lx to guest phys %lx\n",
                    __FILE__, __LINE__, (unsigned long)_send_buf, _addr);
                throw L4_EXCEPTION("map iomem");
            }
            printf("mapped send buffer %p to 0x%lx\n", _send_buf, _addr);
            ret = _addr;
            break;
        case L4_NET_RECV_BUF_ADDR:
            if(GET_VM.mem().register_iomem(&_addr, BUF_SIZE) == false)
            {
                printf("%s:%d Error could not register recv iomem of size %d\n", __FILE__, __LINE__,  BUF_SIZE);
                throw L4_EXCEPTION("regsiter iomem");
            }
            if(!GET_VM.mem().map_iomem(_addr, (l4_addr_t)_recv_buf, 1))
            {
                printf("%s:%d Error could not map recv io memory from karma virt %lx to guest phys %lx\n",
                    __FILE__, __LINE__, (unsigned long)_recv_buf, _addr);
                throw L4_EXCEPTION("map iomem");
            }
            printf("mapped recv buffer %p to 0x%lx\n", _recv_buf, _addr);

            ret = _addr;
            break;
        case L4_NET_GET_BUF_SIZE:
            return BUF_SIZE;
        case L4_NET_GET_IRQ:
            return _intr;
        case L4_NET_RECV_SIZE:
            return _recv_size;
        default:
            printf("L4_net: Call not implemented. Addr = %x\n", (unsigned)addr);
            break;
    }
    return ret;
}

void L4_net::run(L4::Cap<L4::Thread> &) {
    unsigned int res, size = BUF_SIZE;

    res = l4ankh_prepare_recv(pthread_getl4cap(pthread_self()));
    if(res)
    {
        printf("L4_net: l4ankh_prepare_recv(owner=%d) failed with code %d\n",
            (unsigned)pthread_getl4cap(pthread_self()), res);
        return;
    }
    while(1)
    {
        size = BUF_SIZE;
        l4ankh_recv_blocking(_recv_buf, &size);
        _recv_size = size;
        printf("L4_net: received %d bytes\n", size);
        _net_irq.lirq()->trigger();
    }
}

