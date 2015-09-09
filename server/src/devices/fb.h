/*
 * l4_fb.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#pragma once

#include "device_interface.h"
#include "../util/ringbuff.hpp"
#include "../l4_vcpu.h"

#include <l4/sys/irq>
#include <l4/re/env>
#include <l4/re/c/util/video/goos_fb.h>
#include <l4/re/c/event_buffer.h>
#include <l4/re/rm>
#include <l4/re/dataspace>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/event>
#include <l4/sys/types.h>

// from include/linux/fb.h
struct fb_fillrect
{
        l4_uint32_t dx;       /* screen-relative */
        l4_uint32_t dy;
        l4_uint32_t width;
        l4_uint32_t height;
        l4_uint32_t color;
        l4_uint32_t rop;
};

void *refresh_thread_func(void *data);

class L4_fb : public device::IDevice, public L4_vcpu::Interrupt
{
private:
    util::TRingBuffer<struct fb_fillrect, 20> _refresh_command_buffer;
    bool _refresh_pending;
    util::TwoWaySignal _command_buffer_signal;

    l4re_util_video_goos_fb_t goos_fb;
    l4re_event_buffer_consumer_t ev_buf;
    L4::Cap<L4Re::Dataspace> _fb_ds, _event_buf_ds;
    L4::Cap<L4::Irq> _irq;
    l4re_video_view_info_t fbi;
    void *fb_ptr, *ev_ptr, *info_ptr;
    pthread_t _refresh_thread;
    L4_vcpu * _waiting_vcpu;

public:
    void setIntr(unsigned int intr);
    virtual void hypercall(HypercallPayload &);
    void refresh_thread();
    void do_refresh(struct fb_fillrect & rect);
    void refresh(struct fb_fillrect & rect);
    bool checkCompletion();
    void dump() {}

    virtual void handle(){/* empty, we just want to trip the vcpu */};
    virtual L4::Cap<L4::Irq> & cap(){
        return _command_buffer_signal.writerCap();
    }
};

