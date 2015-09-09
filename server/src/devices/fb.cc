/*
 * l4_fb.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
#include "fb.h"
#include "../l4_vm.h"
#include "../l4_vcpu.h"
#include "../l4_exceptions.h"
#include "../util/local_statistics.hpp"
#include "devices_list.h"


#include <l4/sys/err.h>
#include <l4/sys/task.h>
#include <l4/sys/factory>
#include <l4/sys/icu.h>
#include <l4/sys/ktrace.h>
#include <l4/re/c/rm.h>

#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "../util/debug.h"


// as used in linux driver..., but arbitrarily changed for mag
typedef struct
{
    unsigned long base_offset;    ///< Offset to first pixel in data space
    unsigned long mem_total;      ///< Size of the frame buffer
    unsigned long map_size;       ///< Do not use, use fb_ds.size() instead
    unsigned x_res;               ///< Width of the frame buffer
    unsigned y_res;               ///< Height of the frame buffer
    unsigned bytes_per_scan_line; ///< Bytes per line
    unsigned flags;               ///< Flags of the framebuffer
    char bits_per_pixel;          ///< Bits per pixel
    char bytes_per_pixel;         ///< Bytes per pixel
    char red_size;                ///< Size of red color component in a pixel
    char red_shift;               ///< Shift of the red color component in a pixel
    char green_size;              ///< Size of green color component in a pixel
    char green_shift;             ///< Shift of the green color component in a pixel
    char blue_size;               ///< Size of blue color component in a pixel
    char blue_shift;              ///< Shift of the blue color component in a pixel
} l4re_fb_info_t;

l4re_fb_info_t fb_info;

void *refresh_thread_func(void *data)
{
    l4_debugger_set_object_name(pthread_getl4cap(pthread_self()), "L4 FB REFRESH");
#ifdef KARMA_USE_VM_AFFINITY
    l4_sched_param_t l4sp = l4_sched_param(0);
    GET_VM.get_vm_affinity(&l4sp.affinity);
#else
    l4_sched_param_t l4sp = l4_sched_param(0);
    l4sp.affinity = l4_sched_cpu_set(0, 0);
#endif
    if(l4_error(L4Re::Env::env()->scheduler()->run_thread((L4::Cap<L4::Thread>)pthread_getl4cap(pthread_self()), l4sp)))
        printf("Irq_thread error setting priority\n");
    L4_fb *fb = (L4_fb*)data;
    fb->refresh_thread();
    return 0;
}

void L4_fb::setIntr(unsigned int intr)
{
    int ret;

    karma_log(DEBUG, "L4_fb: init\n");
    
    _waiting_vcpu = NULL;
    fb_ptr = 0;
    ev_ptr = 0;

    ret = l4re_util_video_goos_fb_setup_name(&goos_fb, "fb");
    if(ret < 0)
        throw L4_EXCEPTION("setting up goos");

    if(l4re_util_video_goos_fb_view_info(&goos_fb, &fbi))
        throw L4_EXCEPTION("getting to know goos info");

    // convert fbi to framebuffer data structure
    fb_info.x_res = fbi.width;
    fb_info.y_res = fbi.height;
    fb_info.bits_per_pixel = l4re_video_bits_per_pixel(&fbi.pixel_info);
    fb_info.red_size = fbi.pixel_info.r.size;        
    fb_info.red_shift = fbi.pixel_info.r.shift;       
    fb_info.green_size = fbi.pixel_info.g.size;
    fb_info.green_shift = fbi.pixel_info.g.shift;
    fb_info.blue_size = fbi.pixel_info.b.size;
    fb_info.blue_shift = fbi.pixel_info.b.shift;
    fb_info.bytes_per_pixel = fbi.bytes_per_line/fb_info.x_res;

    _event_buf_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if(!_event_buf_ds.is_valid())
        throw L4_MALLOC_EXCEPTION;

    _irq = L4Re::Util::cap_alloc.alloc<L4::Irq>();
    if(!_irq.is_valid())
        throw L4_EXCEPTION("get irq capability");

    if(l4_error(L4Re::Env::env()->factory()->create_irq(_irq)))
        throw L4_EXCEPTION("create irq");

    if (l4_error(l4_icu_bind(l4re_util_video_goos_fb_goos(&goos_fb), 0, _irq.cap())))
        throw L4_EXCEPTION("ICU bind");

    if (l4re_event_get_buffer(l4re_util_video_goos_fb_goos(&goos_fb), _event_buf_ds.cap()))
        throw L4_EXCEPTION("no input available");

    // attach buffers to this address space
    if (l4re_rm_attach((void **)&fb_ptr, fbi.width*fbi.height*fb_info.bits_per_pixel + L4_PAGESIZE*10,
                L4RE_RM_SEARCH_ADDR,
                l4re_util_video_goos_fb_buffer(&goos_fb),
                0, L4_SUPERPAGESHIFT))
        throw L4_EXCEPTION("rm attach");
    if (l4re_rm_attach((void **)&ev_ptr, l4re_ds_size(_event_buf_ds.cap()),
                L4RE_RM_SEARCH_ADDR,
                _event_buf_ds.cap(),
                0, L4_SUPERPAGESHIFT))
        throw L4_EXCEPTION("rm attach");

    GET_VM.gic().attach(_irq, intr);

    _refresh_pending = false;
    pthread_create(&_refresh_thread, 0, ::refresh_thread_func, this);
}

void L4_fb::hypercall(HypercallPayload & payload){
    l4_addr_t addr = 0;
    switch(payload.address()){
    case karma_fb_df_get_info:
        karma_log(DEBUG, "L4_fb: get info\n");
        memcpy((void*)(payload.reg(0) + GET_VM.mem().base_ptr()), &fb_info, sizeof(l4re_video_view_info_t));
        break;
    case karma_fb_df_get_ds_size:
        karma_log(DEBUG, "L4_fb: get ds size\n");
        payload.reg(0) = (l4_umword_t)l4re_ds_size(l4re_util_video_goos_fb_buffer(&goos_fb));
        break;
    case karma_fb_df_get_ds_addr:
        karma_log(DEBUG, "L4_fb: get fb ds addr\n");
        if(GET_VM.mem().register_iomem(&addr, l4re_ds_size(l4re_util_video_goos_fb_buffer(&goos_fb))) == false)
            throw L4_EXCEPTION("register iomem");

        // TODO
        if(!GET_VM.mem().map_iomem(addr, (l4_addr_t)fb_ptr, 1))
            throw L4_EXCEPTION("map iomem");
        karma_log(DEBUG, "L4_fb: Mapped fb memory to addr %x\n", (unsigned int)addr);
        payload.reg(0) = (l4_umword_t)addr;
        break;
    case karma_fb_df_update_rect:
        karma_log(TRACE, "L4_fb: refresh\n");
        try{
            fb_fillrect * rect = (struct fb_fillrect*)(GET_VM.mem().base_ptr() + payload.reg(0));
            l4_uint64_t start = util::tsc();
            refresh(*rect);
            GET_LSTATS_ITEM(lat_l4_fb_refresh).addMeasurement(util::tsc() - start);
        }catch(...)
        {
            printf("l4_fb: unexpected exception during update_rect\n");
        }
        break;
    case karma_fb_df_get_ev_ds_size:
        karma_log(DEBUG, "L4_fb: get ev ds size\n");
        payload.reg(0) = (l4_umword_t)l4re_ds_size(_event_buf_ds.cap());
        karma_log(DEBUG, "L4_fb: event buffer size = %ld\n", payload.reg(0));
        break;
    case karma_fb_df_get_ev_ds_addr:
        karma_log(TRACE, "L4_fb: get ev ds addr\n");
        if(GET_VM.mem().register_iomem(&addr, l4re_ds_size(_event_buf_ds.cap())) == false)
            throw L4_EXCEPTION("register event buffer iomem");

        if(!GET_VM.mem().map_iomem(addr, (l4_addr_t)ev_ptr, 1))
            throw L4_EXCEPTION("map event buffer iomem");
        payload.reg(0) = (l4_umword_t)addr;
        break;
    case karma_fb_df_completion:
        checkCompletion();
        break;
    default:
        printf("l4_fb: Warning unknown device function called (%u) -> ignoring\n", payload.address());
        payload.dump();
        break;
    }
}

void L4_fb::refresh_thread(){
    _command_buffer_signal.attachReader(0);
    while(1){
        struct fb_fillrect rect;
        while(_refresh_command_buffer.get(rect)){
            do_refresh(rect);
        }
        _refresh_pending = false;
        if(_waiting_vcpu) _waiting_vcpu->trip();
        _command_buffer_signal.waitReader();
    }
}

void L4_fb::do_refresh(struct fb_fillrect & rect) {
    l4re_util_video_goos_fb_refresh(&goos_fb, rect.dx, rect.dy, rect.width, rect.height);
}
void L4_fb::refresh(struct fb_fillrect & rect) {
    _refresh_command_buffer.put(rect);
    _refresh_pending = true;
    _command_buffer_signal.signalReader();
}
bool L4_fb::checkCompletion() {
    _waiting_vcpu = &current_vcpu();
    if(!_refresh_command_buffer.empty() || _refresh_pending){
        current_vcpu().super_halt();
        // does not return
        enter_kdebug("checkCompletion");
        return false;
    } else {
        return true;
    }
}
