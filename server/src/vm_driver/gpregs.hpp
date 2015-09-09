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

#include "vmcx.hpp"
#include "hypercall_payload.hpp"

template <typename ARCH, typename EXTENSION>
class GP_REGS{

};

template <typename EXTENSION>
class GP_REGS<arch::x86, EXTENSION>{
public:
    typedef typename VMCX<arch::x86, EXTENSION>::type vmcx_t;

    l4_umword_t ax;
    l4_umword_t bx;
    l4_umword_t cx;
    l4_umword_t dx;
    l4_umword_t bp;
    l4_umword_t di;
    l4_umword_t si;

public:
    inline void toVcpuRegs(l4_vcpu_regs_t & vcpu_regs, vmcx_t &) const;
    inline void fromVcpuRegs(const l4_vcpu_regs_t & vcpu_regs, const vmcx_t &);
    inline void toHypercallPayload(HypercallPayload & hc_payload) const;
    inline void fromHypercallPayload(const HypercallPayload & hc_payload);
    inline void dump(){
        printf("GPRegs: eax: 0x%lx"
                "        ebx: 0x%lx"
                "        ecx: 0x%lx"
                "        edx: 0x%lx"
                "        ebp: 0x%lx"
                "        esi: 0x%lx"
                "        edi: 0x%lx",
                ax, bx, cx, dx, bp, si, di);
    }
};
template <>
inline void GP_REGS<arch::x86, arch::x86::svm>::toVcpuRegs(l4_vcpu_regs_t & vcpu_regs, GP_REGS<arch::x86, arch::x86::svm>::vmcx_t & vmcb) const {
    vmcb.state_save_area.rax = ax;
    vcpu_regs.bp = bp;
    vcpu_regs.bx = bx;
    vcpu_regs.cx = cx;
    vcpu_regs.dx = dx;
    vcpu_regs.di = di;
    vcpu_regs.si = si;
}
template <>
inline void GP_REGS<arch::x86, arch::x86::vmx>::toVcpuRegs(l4_vcpu_regs_t & vcpu_regs, GP_REGS<arch::x86, arch::x86::vmx>::vmcx_t &) const {
    vcpu_regs.ax = ax;
    vcpu_regs.bp = bp;
    vcpu_regs.bx = bx;
    vcpu_regs.cx = cx;
    vcpu_regs.dx = dx;
    vcpu_regs.di = di;
    vcpu_regs.si = si;
}
template <>
inline void GP_REGS<arch::x86, arch::x86::svm>::fromVcpuRegs(const l4_vcpu_regs_t & vcpu_regs, const GP_REGS<arch::x86, arch::x86::svm>::vmcx_t & vmcb){
        ax = vmcb.state_save_area.rax;
        bp = vcpu_regs.bp;
        bx = vcpu_regs.bx;
        cx = vcpu_regs.cx;
        dx = vcpu_regs.dx;
        di = vcpu_regs.di;
        si = vcpu_regs.si;
}
template <>
inline void GP_REGS<arch::x86, arch::x86::vmx>::fromVcpuRegs(const l4_vcpu_regs_t & vcpu_regs, const GP_REGS<arch::x86, arch::x86::vmx>::vmcx_t &){
        ax = vcpu_regs.ax;
        bp = vcpu_regs.bp;
        bx = vcpu_regs.bx;
        cx = vcpu_regs.cx;
        dx = vcpu_regs.dx;
        di = vcpu_regs.di;
        si = vcpu_regs.si;
}

template <typename EXTENSION>
inline void GP_REGS<arch::x86, EXTENSION>::toHypercallPayload(HypercallPayload & hc_payload) const {
    hc_payload.rawCommandReg() = cx;
    hc_payload.reg(0) = dx;
    hc_payload.reg(1) = bx;
    hc_payload.reg(2) = si;
    hc_payload.reg(3) = di;
}

template <typename EXTENSION>
inline void GP_REGS<arch::x86, EXTENSION>::fromHypercallPayload(const HypercallPayload & hc_payload){
    cx = hc_payload.rawCommandReg();
    dx = hc_payload.reg(0);
    bx = hc_payload.reg(1);
    si = hc_payload.reg(2);
    di = hc_payload.reg(3);
}

#endif /* GPREGS_HPP_ */
