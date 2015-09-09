/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * gpregs.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * gpregs.hpp
 *
 *  Created on: 29.10.2010
 *      Author: janis
 */

#ifndef GPREGS_HPP_
#define GPREGS_HPP_

#include "hypercall_payload.hpp"

class GP_REGS {

public:
    l4_umword_t sp;
    l4_umword_t s0;
    l4_umword_t s1;
    l4_umword_t s2;
    l4_umword_t s3;
    l4_umword_t s4;

    GP_REGS() : sp(0), s0(0), s1(0), s2(0), s3(0), s4(0) {};

    inline void toVcpuRegs(l4_vcpu_regs_t & vcpu_regs) const;
    inline void fromVcpuRegs(const l4_vcpu_regs_t & vcpu_regs);
    inline void toHypercallPayload(HypercallPayload & hc_payload) const;
    inline void fromHypercallPayload(const HypercallPayload & hc_payload);
    inline void dump(){
        printf("GPRegs: sp: 0x%lx"
                "        s0: 0x%lx"
                "        s1: 0x%lx"
                "        s2: 0x%lx"
                "        s3: 0x%lx"
                "        s4: 0x%lx",
                sp, s0, s1, s2, s3, s4);
    }
    inline l4_umword_t rawCommandReg();
    inline l4_umword_t stackFrame();
};

inline void GP_REGS::toVcpuRegs(l4_vcpu_regs_t & vcpu_regs) const
{
    vcpu_regs.sp = sp;
    vcpu_regs.r[16] = s0; /* s0 */
    vcpu_regs.r[17] = s1; /* s1 */
    vcpu_regs.r[18] = s2; /* s2 */
    vcpu_regs.r[19] = s3; /* s3 */
    vcpu_regs.r[20] = s4; /* s4 */
}

inline void GP_REGS::fromVcpuRegs(const l4_vcpu_regs_t & vcpu_regs)
{
    sp = vcpu_regs.sp;
    s0 = vcpu_regs.r[16]; /* s0 */
    s1 = vcpu_regs.r[17]; /* s1 */
    s2 = vcpu_regs.r[18]; /* s2 */
    s3 = vcpu_regs.r[19]; /* s3 */
    s4 = vcpu_regs.r[20]; /* s4 */
}

inline void GP_REGS::toHypercallPayload(HypercallPayload & hc_payload) const
{
    hc_payload.rawCommandReg() = s0;
    hc_payload.reg(0) = s1;
    hc_payload.reg(1) = s2;
    hc_payload.reg(2) = s3;
    hc_payload.reg(3) = s4;
}

inline void GP_REGS::fromHypercallPayload(const HypercallPayload & hc_payload)
{
    s0 = hc_payload.rawCommandReg();
    s1 = hc_payload.reg(0);
    s2 = hc_payload.reg(1);
    s3 = hc_payload.reg(2);
    s4 = hc_payload.reg(3);
}

inline l4_umword_t GP_REGS::rawCommandReg()
{
    return s0;
}

inline l4_umword_t GP_REGS::stackFrame()
{
    return sp;
}
#endif /* GPREGS_HPP_ */
