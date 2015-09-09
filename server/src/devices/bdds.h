/*
 * l4_bdds.h - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Matthias Lange <mlange@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#ifndef BDDS_H
#define BDDS_H

#include "device_interface.h"

#define NAME_LEN 1024

class Bdds: public device::IDevice
{
private:
    L4::Cap<L4Re::Dataspace> image_ds;
    long image_size;
    char _image_name[NAME_LEN];
    l4_addr_t _image_addr;
    l4_addr_t read_gpa;
    l4_addr_t write_gpa;

    L4::Cap<L4Re::Dataspace> read_ds, write_ds;
    l4_addr_t _read_ds_addr, _write_ds_addr;

public:
    void setImageName(const char *image_name);

    virtual void hypercall(HypercallPayload & payload);

    l4_umword_t read(l4_umword_t addr);
    void write(l4_umword_t addr, l4_umword_t val);
    int init_read_write_dataspaces();
    int load_bd_image(unsigned int mode);


};

#endif // BDDS_H
