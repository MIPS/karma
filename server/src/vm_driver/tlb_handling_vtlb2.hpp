/*
 * tlb_handling_vtlb2.hpp - TODO enter description
 * 
 * (c) 2011 Jan Nordholz <jnordholz@sec.t-labs.tu-berlin.de>
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * tlb_handling_vtlb.hpp
 *
 *  Created on: 03.11.2010
 */

#ifndef TLB_HANDLING_VTLB_HPP_
#define TLB_HANDLING_VTLB_HPP_
template <typename ARCH, typename EXTENSION>
class VTLB2{

};

template <typename EXTENSION>
class VTLB2<arch::x86, EXTENSION>{
    l4_umword_t _shadow_cr3;
public:
    typedef typename VMCX<arch::x86, EXTENSION>::type vmcx_t;
    typedef GP_REGS<arch::x86, EXTENSION> gpregs_t;

    inline void init(vmcx_t & vmcx){
        _shadow_cr3 = (l4_addr_t)-1;
        _init(vmcx);
    }

    inline void resume(){

    }

    void intercept(vmcx_t & vmcx, gpregs_t & gpregs, L4_vm_driver::Exitcode & code);

    inline void sanitize_vmcx(vmcx_t & ){

    }
private:
    void _init(vmcx_t &);
    l4_umword_t & choose_creg(vmcx_t & vmcx);
    void handle_register_access(vmcx_t & vmcx, gpregs_t & gpregs, l4_uint8_t * vinsn);
    void reset_cr3(const l4_addr_t addr);
    int shadow_fault_in(const unsigned long, l4_addr_t);
    int invalidate_address(l4_addr_t);
    int pglookup(const unsigned int, const l4_addr_t, l4_addr_t &, l4_size_t *);
};

template <>
void VTLB2<arch::x86, arch::x86::svm>::_init(l4_vm_svm_vmcb_t & vmcb){
    vmcb.control_area.np_enable = 0;

    vmcb.state_save_area.cr0 |= (1UL << 31);
    vmcb.control_area.intercept_rd_crX = (1UL << 3) | (1UL << 0);
    vmcb.control_area.intercept_wr_crX = (1UL << 3) | (1UL << 0);
    vmcb.control_area.intercept_exceptions = 0x000FFF7FUL; // UD, TS, SS, GP, PF
    vmcb.control_area.intercept_instruction0 = 0xFF44001F;
}

template <typename EXTENSION>
int VTLB2<arch::x86, EXTENSION>::pglookup(const unsigned int access_type, const l4_addr_t vaddr, l4_addr_t & real_addr, l4_size_t * const pgentry_size) {
    unsigned long entry;
    unsigned long *ptbl;

    if (access_type & 128) printf("PgLookup: request %lx, ptable %lx\n", vaddr, _shadow_cr3);

    // @@@ include bits 32-39 (extended effective address size)... (so far they're ignored when parsing entries)
    if (_shadow_cr3 == (l4_addr_t)-1) {
        entry = (unsigned long)vaddr;
    } else {
        ptbl = (unsigned long *)(_shadow_cr3 + GET_VM.mem().base_ptr());
        entry = ptbl[(int)(vaddr >> 22)];
        if (access_type & 128) printf("PgTable1 E (@%lx): %lx\n", (unsigned long)&ptbl[(int)(vaddr >> 22)], entry);
        if (!(entry & 1) && (access_type & 1)) { return 1; }
        if (!(entry & 2) && (access_type & 2)) { return 2; }
        if (!(entry & 4) && (access_type & 4)) { return 4; }
        if (entry & (1<<7)) {
            entry = (entry & 0xffc00000UL) | (vaddr & 0x003fffffUL);
            if (pgentry_size) *pgentry_size = (1<<22);
        } else {
            ptbl = (unsigned long *)((entry & 0xfffff000UL) + GET_VM.mem().base_ptr());
            entry = ptbl[(int)((vaddr >> 12) & 0x3ff)];
            if (access_type & 128) printf("PgTable2 E (@%lx): %lx\n", (unsigned long)&ptbl[(int)((vaddr >> 12) & 0x3ff)], entry);
            if (!(entry & 1) && (access_type & 1)) { return 1; }
            if (!(entry & 2) && (access_type & 2)) { return 2; }
            if (!(entry & 4) && (access_type & 4)) { return 4; }
            entry = (entry & 0xfffff000UL) | (vaddr & 0xfffUL);
            if (pgentry_size) *pgentry_size = (1<<12);
       }
    }

    if (GET_VM.mem().phys_to_karma(entry, real_addr)) {
        return 8;
    }

    if (access_type & 128) printf("PgLookup: answer %lx (sz %x) (incl. ds base)\n", real_addr, (pgentry_size) ? (*pgentry_size) : -1);

    return 0;
}
template <typename EXTENSION>
void VTLB2<arch::x86, EXTENSION>::reset_cr3(const l4_addr_t new_cr3) {

//    l4_msgtag_t msg;
//    l4_addr_t virtual_addr;
//    unsigned int i, j;
//    unsigned int size_poweroftwo;
//    if(new_cr3 == _shadow_cr3) return;
//    printf("resetting cr3\n");

//    for (i = GET_VM.mem().get_mem_size(), size_poweroftwo = 0; i; i >>= 1, size_poweroftwo++) ;

    GET_VM.mem().unmap_all();
    // msg = _vm_cap->unmap(l4_fpage(0UL, size_poweroftwo, L4_FPAGE_RWX), L4_FP_ALL_SPACES);

    // msg = _vm_cap->unmap(l4_fpage(0UL, 63, L4_FPAGE_RWX), L4_FP_ALL_SPACES);
    // mapontop(0x80008000UL, 0x1000000UL, 0);

//    msg = L4Re::Env::env()->task()->unmap(l4_fpage(GET_VM.mem().base_ptr(), size_poweroftwo, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
//    msg = GET_VM.vm_cap()->unmap(l4_fpage(GET_VM.mem().base_ptr(), size_poweroftwo, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);

//    if (l4_error(msg)) {
//        printf("L4_mem::reset_cr3 => Unmapping error.\n");
//        exit(1);
//        /*
//           } else {
//           printf("L4_mem::reset_cr3 => Memory unmapped, new cr3 %lx.\n", new_cr3);
//           */
//    }

    if (0 && _shadow_cr3 != new_cr3) {
        printf("L4_mem::reset_cr3(): New guest page table, old ptr %lx, new ptr %lx.\n", _shadow_cr3, new_cr3);
    }

    _shadow_cr3 = new_cr3;

    // walk new page table, copy valid entries to shadow ptbl

//    virtual_addr = 0UL;
//
//    for (i = 0; i < 1024; i++) {
//        l4_addr_t pa;
//        unsigned long entry = ((unsigned long *)(GET_VM.mem().base_ptr() + _shadow_cr3))[i];
//        if (entry & 1) {
//            if (entry & (1UL << 7)) {
//                printf("Vaddr %x -> SPGE entry %lx\n", (i << 22), entry);
//                virtual_addr = (l4_addr_t)(i << 22);
//                if (GET_VM.mem().phys_to_karma(entry & 0xffc00000, pa)) {
//                    // printf("Ignoring invalid 4M mapping (%lx -> %lx).\n", virtual_addr, entry);
//                } else {
//                    (void)GET_VM.mem().map(virtual_addr, pa, (1UL<<22), (entry & 2) ? 1 : 0, 0);
//                }
//            } else {
//                unsigned long entry2;
//                printf("Vaddr %x -> pdir entry %lx\n", (i << 22), entry);
//                for (j = 0; j < 1024; j++) {
//                    entry2 = ((unsigned long *)(GET_VM.mem().base_ptr() + (entry & 0xfffff000)))[j];
//                    if (entry2 & 1) {
//                        // printf("Vaddr %x -> ptbl entry %lx\n", (i << 22) | (j << 12), entry2);
//                        virtual_addr = (l4_addr_t)((i << 22) | (j << 12));
//                        if (GET_VM.mem().phys_to_karma(entry2 & 0xfffff000, pa)) {
//                            // printf("Ignoring invalid 4K mapping (%lx -> %lx).\n", virtual_addr, entry2);
//                        } else {
//                            (void)GET_VM.mem().map(virtual_addr, pa, (1UL<<12), (entry2 & 2) ? 1 : 0, 0);
//                        }
//                    }
//                }
//            }
//        }
//    }

}



static inline void inject_pf_event_svm(l4_vm_svm_vmcb_t & vmcb){
    // error code
    l4_uint64_t event_inj = vmcb.control_area.exitinfo1;
    event_inj <<= 32;
    event_inj |= vmcb.control_area.exitcode & 0x1f; // vector
    event_inj |= 0x300; // type
    event_inj |= 0x800; // error code valid
    event_inj |= 0x80000000; // valid
    vmcb.control_area.eventinj = event_inj;

    vmcb.state_save_area.cr2 = vmcb.control_area.exitinfo2;
}

inline l4_umword_t & choose_gpreg_svm(GP_REGS<arch::x86, arch::x86::svm> & gpregs, const l4_uint8_t modrm){
    switch (modrm & 0x7) {
        case 0: return gpregs.ax;
        case 1: return gpregs.cx;
        case 2: return gpregs.dx;
        case 3: return gpregs.bx;
        case 4: printf("guest tryed to access esp\n"); exit(1); break;
        case 5: return gpregs.bp;
        case 6: return gpregs.si;
        case 7: default: return gpregs.di;
    }
}

template<>
l4_umword_t & VTLB2<arch::x86, arch::x86::svm>::choose_creg(l4_vm_svm_vmcb_t & vmcb){
    switch(vmcb.control_area.exitcode & 0xf){
    case 0: // cr0
        return (l4_umword_t&)vmcb.state_save_area.cr0;
    case 3: // cr3
        return _shadow_cr3;
    case 4: // cr4
        return (l4_umword_t&)vmcb.state_save_area.cr4;
    default:
        printf("Unexpected controll register access (exitcode: 0x%llx)\n", vmcb.control_area.exitcode);
        exit(1);
    }
}

template<>
void VTLB2<arch::x86, arch::x86::svm>::handle_register_access(l4_vm_svm_vmcb_t & vmcb, gpregs_t & gpregs, l4_uint8_t * vinsn){

    l4_umword_t & gpreg = choose_gpreg_svm(gpregs, vinsn[2]);
    l4_umword_t & creg = choose_creg(vmcb);

    if(vinsn[0] != 0x0f){
        printf("Unexpected opcode : %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx\n",
                vinsn[0], vinsn[1], vinsn[2], vinsn[3], vinsn[4], vinsn[5], vinsn[6], vinsn[7], vinsn[8], vinsn[9], vinsn[10], vinsn[11], vinsn[12], vinsn[13], vinsn[14]);
        exit(1);
    }

    if(vmcb.control_area.exitcode & 0x10){
        // WRITE access
        switch(vmcb.control_area.exitcode){
        case 0x10:
            if (vinsn[0] == 0x0f && vinsn[1] == 0x06) { // CLTS
                creg &= ~0x8;
                vmcb.state_save_area.rip += 2;
            } else {
                creg = gpreg;
                vmcb.state_save_area.rip += 3;
            }
            break;
        case 0x13:
            reset_cr3(gpreg);
            vmcb.state_save_area.rip += 3;

            break;
        case 0x14:
            if ((creg ^ gpreg) & 0x90) {
                reset_cr3(_shadow_cr3);
            }
            creg = gpreg;
            vmcb.state_save_area.rip += 3;
            // flush tlb
            vmcb.control_area.guest_asid_tlb_ctl |= (1ULL << 32);
            break;
        }
    } else {
        // READ access
        gpreg = creg;
        vmcb.state_save_area.rip += 3;
    }

}

template <typename EXTENSION>
int VTLB2<arch::x86, EXTENSION>::invalidate_address(const l4_addr_t vaddr) {
//    l4_addr_t paddr;
//    l4_size_t psize;
    l4_msgtag_t msg;
    //int lookup_ret = pglookup(1, vaddr, paddr, &psize);
    //printf("invalidate vaddr : 0x%x page size : %u lookup_ret: %u\n", vaddr, psize, lookup_ret);

    msg = GET_VM.vm_cap()->unmap(l4_fpage(vaddr & 0xffc00000, 22, L4_FPAGE_RWX), L4_FP_ALL_SPACES);
    if(l4_error(msg)){
        printf("unmap operation failed\n");
        exit(1);
    }
    return 0;
}


template<typename EXTENSION>
int VTLB2<arch::x86, EXTENSION>::shadow_fault_in(const unsigned long errcode, l4_addr_t vaddr) {
    l4_addr_t mapped_addr = 0;
    l4_size_t pgentry_size = 0;
    unsigned int rcode = pglookup((errcode & 7) | 1, vaddr,
            mapped_addr, &pgentry_size);

    // printf("Page walk: %u (%lx)\n", rcode, mapped_addr);
    if (rcode == 0) {
        vaddr &= ~(pgentry_size-1);
        mapped_addr &= ~(pgentry_size-1);
//        printf("Faulting in [vm-v %lx -> karma-v %lx] %c\n", vaddr, mapped_addr, errcode & 2 ? 'w' : 'r');
        GET_VM.mem().map(vaddr, mapped_addr, pgentry_size, (errcode & 2) ? 1 : 0, 0);

    } else {
        // printf("Walker returned errcode %u for vaddr %lx\n", rcode, vaddr);
    }

    return rcode;
}

template<>
void VTLB2<arch::x86, arch::x86::svm>::intercept(l4_vm_svm_vmcb_t & vmcb, gpregs_t & gpregs, L4_vm_driver::Exitcode & code){
    int r;
    l4_uint8_t * vinsn;
    switch(vmcb.control_area.exitcode){

    case 0x4e:
//        printf("Encountered a page fault, VM RIP = %llx, Addr = %lx, Code = %lx.\n",
//                        vmcb.state_save_area.rip,
//                        (unsigned long)vmcb.control_area.exitinfo2,
//                        (unsigned long)vmcb.control_area.exitinfo1);

        r = shadow_fault_in(vmcb.control_area.exitinfo1,
                (l4_addr_t)vmcb.control_area.exitinfo2);

        if (r) { // reinject #PF
            inject_pf_event_svm(vmcb);
            if(vmcb.control_area.exitintinfo & 0x80000000){
                code = L4_vm_driver::Error;
                printf("Fatal: Non shadow #PF while delivering event/interrupt!\n");
                return;
            }
//            printf("Reinjecting #PF (walker %u), VM RIP %llx, faultaddr %lx, code %lx --> evinj %llx. ecx: 0x%x ebx: 0x%x edx: 0x%x\n", r,
//                    vmcb.state_save_area.rip, (unsigned long)vmcb.control_area.exitinfo2,
//                    (unsigned long)vmcb.control_area.exitinfo1, vmcb.control_area.eventinj, gpregs.ecx, gpregs.ebx, gpregs.edx);
//            if(vmcb.control_area.exitinfo2 == 0x20746369){
//                enter_kdebug();
//            }
        }
        if(vmcb.control_area.exitintinfo & 0x80000000){
            vmcb.control_area.eventinj = vmcb.control_area.exitintinfo;
        }
//        else if((vmcb.control_area.exitinfo2 & 0xff000000) == 0xb7000000){
//            printf("Handled #PF (walker %u), VM RIP %llx, faultaddr %lx, code %lx --> evinj %llx. ecx: 0x%x ebx: 0x%x edx: 0x%x\n", r,
//                    vmcb.state_save_area.rip, (unsigned long)vmcb.control_area.exitinfo2,
//                    (unsigned long)vmcb.control_area.exitinfo1, vmcb.control_area.eventinj, gpregs.ecx, gpregs.ebx, gpregs.edx);
//
//        }

//        intercepts[2]++;
        code = L4_vm_driver::Handled;
        break;

    case 0x0:
    case 0x3:
    case 0x4:
    case 0x10:
    case 0x13:
    case 0x14:
//        printf("controll register access (exit code 0x%x)\n", vmcb.control_area.exitcode);
        r = pglookup(1, (l4_addr_t)vmcb.state_save_area.rip, (l4_addr_t &)vinsn, NULL);
        if (r) {
            printf("Cannot fetch VM instruction.\n");
            code = L4_vm_driver::Error;
            return;
        }
        handle_register_access(vmcb, gpregs, vinsn);
        code = L4_vm_driver::Handled;
#if 0 // this code can be used to verify instruction decoding on systems that support nrip
        if(vmcb.state_save_area.rip != vmcb.control_area.nrip){
            printf("Error ip is: 0x%llx  should be 0x%llx (exitcode 0x%llx)\n", vmcb.state_save_area.rip, vmcb.control_area.nrip, vmcb.control_area.exitcode);
            exit(1);
        }
#endif
        break;

    case 0x79:  /// INVLPG
        r = pglookup(1, (l4_addr_t)vmcb.state_save_area.rip, (l4_addr_t &)vinsn, NULL);
        if (r) {
            printf("Cannot fetch VM instruction.\n");
            code = L4_vm_driver::Error;
            return;
        }

        if (vinsn[2] == 0x38)
            invalidate_address((l4_addr_t)gpregs.ax);
        else if (vinsn[2] == 0x39)
            invalidate_address((l4_addr_t)gpregs.cx);
        else if (vinsn[2] == 0x3a)
            invalidate_address((l4_addr_t)gpregs.dx);
        else
            enter_kdebug();
        // flush tlb
        vmcb.control_area.guest_asid_tlb_ctl |= (1ULL << 32);
        vmcb.state_save_area.rip += 3;
        code = L4_vm_driver::Handled;
#if 0 // this code can be used to verify instruction decoding on systems that support nrip
        if(vmcb.state_save_area.rip != vmcb.control_area.nrip){
            printf("Error ip is: 0x%llx  should be 0x%llx (exitcode 0x%llx)\n", vmcb.state_save_area.rip, vmcb.control_area.nrip, vmcb.control_area.exitcode);
            exit(1);
        }
#endif
        break;
    }

}



//// found an intercept that we do not currently handle
//printf("Intercept (%p). VMCB @%p (gp @%p). Code: '%lx', Eip: %llx\n",
//    pthread_self(), _vmcb, _gpregs,
//    (unsigned long)vmcb.control_area.exitcode,
//    vmcb.state_save_area.rip);
//printf("Exitinfo: %lx %lx\n",
//    (unsigned long)vmcb.control_area.exitinfo1,
//    (unsigned long)vmcb.control_area.exitinfo2);
//return Error; // Intercepted_Instruction;


#endif /* TLB_HANDLING_VTLB_HPP_ */
