/*
 * tlb_handling_npt.hpp - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * tlb_handling_npt.hpp
 *
 *  Created on: 03.11.2010
 *      Author: janis
 */

#ifndef TLB_HANDLING_NPT_HPP_
#define TLB_HANDLING_NPT_HPP_


template <typename ARCH, typename EXTENSION>
class NPT{
public:
    typedef typename VMCX<arch::x86, EXTENSION>::type vmcx_t;
    typedef GP_REGS<arch::x86, EXTENSION> gpregs_t;

    inline void init(vmcx_t &){

    }

    inline void resume(){

    }

    inline void intercept(vmcx_t & , gpregs_t &, L4_vm_driver::Exitcode &){

    }

    inline void sanitize_vmcx(vmcx_t & ){

    }

};

#endif /* TLB_HANDLING_NPT_HPP_ */
