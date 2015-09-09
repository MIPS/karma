/*
 * l4_bd.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/c/dataspace.h>

#include <l4/sys/types.h>
#include <l4/sys/irq>
#include <l4/sys/icu>
#include <l4/sys/vcon>

#include <string.h>

#include "bdds.h"
#include "../util/debug.h"
#include "../l4_vm.h"
#include "../util/buffer.hpp"
#include "devices_list.h"

void
Bdds::setImageName(const char *image_name)
{
    unsigned int len = strlen(image_name);
    if (len >= NAME_LEN)
    {
        // len must be less than NAME_LEN to leave room for the
        // terminating \0 char.
        karma_log(ERROR, "Block device image file name too long. Terminating.\n");
        exit(1);
    }
    strncpy(_image_name, image_name, NAME_LEN);

}

void 
Bdds::hypercall(HypercallPayload & payload)
{
    if(payload.address() & 1)
        write(payload.address() & ~1L, payload.reg(0));
    else
        payload.reg(0) = read(payload.address());
}

l4_umword_t
Bdds::read(l4_umword_t addr)
{
    l4_umword_t ret = ~0UL;
    switch (addr)
    {
        case karma_bdds_df_transfer_ds_size:
            ret = 4096;
            break;
        case karma_bdds_df_disk_size:
            ret = image_size;
            break;
        case karma_bdds_df_transfer_read_ds:
            ret = read_gpa;
            break;
        case karma_bdds_df_transfer_write_ds:
            ret = write_gpa;
            break;
        default:
            karma_log(ERROR, "attempted bdds read with unknown opcode: %lx. Ignoring.\n", addr);
    }

    return ret;
}

void
Bdds::write(l4_umword_t addr, l4_umword_t val)
{
    int ret = 0;

    switch (addr)
    {
        case karma_bdds_df_mode:
            karma_log(DEBUG, "[bdds] init\n");
            ret = load_bd_image(val);
            if (ret < 0)
            {
                karma_log(ERROR, "Error loading block device image into karma\n");
                return;
            }
            ret = init_read_write_dataspaces();
            if (ret)
            {
                karma_log(ERROR, "Error creating read/write dataspaces\n");
                return;
            }
        case karma_bdds_df_transfer_write:
            break;
        case karma_bdds_df_transfer_read:
            memcpy((void *) _read_ds_addr, (void *) (_image_addr + val), 4096);
            break;
        default:
            karma_log(ERROR, "attempted bdds write with unknow opcode %lx. Ignoring.\n", addr);
    }
}

int
Bdds::init_read_write_dataspaces() 
{
    int r;
    // initialize dataspaces for read and write
    read_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace> ();
    write_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace> ();

    if (!read_ds.is_valid())
    {
        karma_log(ERROR, "[bdds] read_ds not valid\n");
        return -1;
    }

    if (!write_ds.is_valid())
    {
        karma_log(ERROR, "[bdds] write_ds not valid\n");
        return -1;
    }

    // allocate space for both buffers
    if ((r = L4Re::Env::env()->mem_alloc()->alloc(4096, read_ds,
            L4Re::Mem_alloc::Continuous)))
    {
        karma_log(ERROR, "[bdds] mem alloc for read_ds failed\n");
        return -1;
    }

    if ((r = L4Re::Env::env()->mem_alloc()->alloc(4096, write_ds,
            L4Re::Mem_alloc::Continuous)))
    {
        karma_log(ERROR, "[bdds] mem alloc for write_ds failed\n");
        return -1;
    }

    _read_ds_addr = _write_ds_addr = 0;
    r = L4Re::Env::env()->rm()->attach((void**) &_read_ds_addr, 4096,
            L4Re::Rm::Search_addr, read_ds, 0, L4_SUPERPAGESHIFT);

    if (r)
    {
        karma_log(ERROR, "[bdds] attaching read dataspace failed\n");
        return -1;
    }

    r = L4Re::Env::env()->rm()->attach((void**) &_write_ds_addr, 4096,
            L4Re::Rm::Search_addr, write_ds, 0, L4_SUPERPAGESHIFT);

    if (r)
    {
        karma_log(ERROR, "[bdds] attaching write dataspace failed\n");
        return -1;
    }

    if (GET_VM.mem().register_iomem(&read_gpa, 4096) == false)
    {
        karma_log(ERROR, "[bdds] register read_ds iomem failed, size %ld\n",
                l4re_ds_size(read_ds.cap()));
        return -1;
    }

    GET_VM.mem().map_iomem(read_gpa, _read_ds_addr, 1);
    
    if (GET_VM.mem().register_iomem(&write_gpa, 4096) == false)
    {
        karma_log(ERROR, "[bdds] register write_ds iomem failed\n");
        return -1;
    }
    
    GET_VM.mem().map_iomem(write_gpa, _write_ds_addr, 1);

    return 0;
}

int
Bdds::load_bd_image(unsigned int mode)
{
    int ret;
    image_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace> ();
    if (!image_ds.is_valid())
    {
        karma_log(ERROR, "[bdds] Fatal error, could not allocate bd image capability. Terminating\n");
        exit(1);
    }

    L4::Cap<L4Re::Namespace> rom = L4Re::Env::env()->get_cap<
            L4Re::Namespace> ("rom");

    if (!rom.is_valid())
    {
        karma_log(ERROR, "[bdds] Fatal error, could not find rom namespace. Terminating.\n");
        exit(1);
    }

    // get dataspace capability of image
    ret = rom->query(_image_name, image_ds);
    if(ret < 0)
    {
        karma_log(ERROR, "[bdds] Fatal error, could not find rom/%s. Terminating.\n", _image_name);
        exit(1);
    }

    image_size = image_ds->size();
    if(image_size < 0) // error
    {
        karma_log(ERROR, "[bdds] Error determining image size. Terminating.\n");
        exit(1);
    }

    int r;

    if ((mode & 0x1)) // mount rw
        r = L4Re::Env::env()->rm()->attach((void**) &_image_addr,
                image_size, L4Re::Rm::Search_addr, image_ds, 0,
                L4_SUPERPAGESHIFT);
    else
        r = L4Re::Env::env()->rm()->attach((void**) &_image_addr,
                image_size, L4Re::Rm::Read_only | L4Re::Rm::Search_addr,
                image_ds, 0, L4_SUPERPAGESHIFT);

    if (r)
    {
        karma_log(ERROR, "[bdds] Error attaching bd image dataspace\n");
        return -1;
    }

    karma_log(INFO, "[bdds] image is @ 0x%lx, size is 0x%lx\n", _image_addr,
            image_ds->size());
    return 0;
}

