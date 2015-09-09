/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#pragma once

#include "device_interface.h"

class MemDevice : public device::IDevice {
public:
    virtual void hypercall(HypercallPayload &);
    void init() {};
};
