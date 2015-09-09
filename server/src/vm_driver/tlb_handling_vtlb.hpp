/*
 * tlb_handling_vtlb.hpp - TODO enter description
 * 
 * (c) 2011 Divij Gupta <>
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * vtlb.hpp
 *
 *  Created on: 28.10.2010
 */

#ifndef TLB_HPP_
#define TLB_HPP_


#include <l4/sys/vm.h>

#include "../util/local_statistics.hpp"

#include "pageDB.hpp"

struct page_bits {
    unsigned int write;
    unsigned int dirty;
    unsigned int accessed;   //not used/needed currently
    unsigned int global;
};


template <typename ARCH, typename EXTENSION>
class VTLB_EXTENSION_SPECIFICS{

};

template <typename ARCH, typename EXTENSION>
class VTLB{

};


template <typename EXTENSION>
class CDregs // controll/debug register tracker
{
};

template <>
class CDregs<arch::x86::svm>
{
public:
    l4_umword_t cr0;
    l4_umword_t cr3;
    l4_umword_t cr4;
    l4_umword_t dr6;
    l4_umword_t dr7;
public:
    inline void toVMCX(l4_vm_svm_vmcb_t & vmcb){
        vmcb.state_save_area.cr4 = cr4;
        vmcb.state_save_area.cr3 = cr3;
        vmcb.state_save_area.cr0 = cr0;
        vmcb.state_save_area.dr7 = dr7;
        vmcb.state_save_area.dr6 = dr6;
    }
    inline void fromVMCX(const l4_vm_svm_vmcb_t & vmcb){
        cr4 = vmcb.state_save_area.cr4;
        cr3 = vmcb.state_save_area.cr3;
        cr0 = vmcb.state_save_area.cr0;
        dr7 = vmcb.state_save_area.dr7;
        dr6 = vmcb.state_save_area.dr6;
    }
};
template <>
class CDregs<arch::x86::vmx>
{
public:
    l4_umword_t cr0;
    l4_umword_t cr3;
    l4_umword_t cr4;
    l4_umword_t dr7;

    inline void toVMCX(Vmcs & vmcs){
        vmcs.vmwrite<VMX_GUEST_CR4>(cr4);
        vmcs.vmwrite<VMX_GUEST_CR3>(cr3);
        vmcs.vmwrite<VMX_GUEST_CR0>(cr0);
        vmcs.vmwrite<VMX_GUEST_DR7>(dr7);
    }
    inline void fromVMCX(const Vmcs & vmcs){
        cr4 = vmcs.vmread<VMX_GUEST_CR4>();
        cr3 = vmcs.vmread<VMX_GUEST_CR3>();
        cr0 = vmcs.vmread<VMX_GUEST_CR0>();
        dr7 = vmcs.vmread<VMX_GUEST_DR7>();
    }
};


template <typename EXTENSION>
class VTLB<arch::x86, EXTENSION>{
public:

    typedef typename VMCX<arch::x86, EXTENSION>::type vmcx_t;
    typedef GP_REGS<arch::x86, EXTENSION> gpregs_t;

protected:
    CDregs<EXTENSION> _cdregs;

    shadowPageDB _shadowPageDB;

    bool _guest_enabled_paging; //check whether the guest enabled paging for shadow page tables

public:
    inline void init(vmcx_t & vmcx){
        VTLB_EXTENSION_SPECIFICS<arch::x86, EXTENSION>::init(vmcx);
        _guest_enabled_paging = false;

        _cdregs.fromVMCX(vmcx);
    }

    void unmap_violation_page(l4_addr_t mem_addr) {
        _shadowPageDB.unmap(mem_addr);
    }

    //unmap the regions that you have mapped on cr3 loads
    void tlb_flush_emulation() {
        _shadowPageDB.flush(!(_cdregs.cr4 & 0x80));
    }

    bool page_table_walker(l4_addr_t fault_addr,
            unsigned long long& guest_phys_addr, struct page_bits* pb,
            unsigned long &size) {
        char *base_pointer = (char*) GET_VM.mem().base_ptr();
        unsigned long page_dir_entry_addr = 0x0;
        guest_phys_addr = 0x0;

        karma_log(DEBUG, "cr3 val is %lx \n", _cdregs.cr3);

        // Step 1 resolve page directory entry address
        unsigned long linear_addr_for_pde = fault_addr & 0xffc00000; //only bits 31-22
        unsigned long cur_cr3_req_bits = _cdregs.cr3 & 0xfffff000; //has bits 12-31 of the cr3 register, rest of the bits are 0
        unsigned long linear_addr_req_bits_for_pde = linear_addr_for_pde >> 20; //has bits in pos 2 to 11, rest are 0
        page_dir_entry_addr |= cur_cr3_req_bits;
        page_dir_entry_addr |= linear_addr_req_bits_for_pde;

        unsigned long page_dir_entry = *((unsigned int*) (base_pointer
                + page_dir_entry_addr));

        karma_log(DEBUG, "page dir entry val is %lx \n", page_dir_entry);

        if ((page_dir_entry & 0x1) != 0x1) {
            karma_log(TRACE, "doesn't ref a page table or a large page\n");
            return false;
        }

        if ((page_dir_entry & 0x80) == 0x80) {
            // large page
            guest_phys_addr |= (fault_addr & 0x3fffff);
            guest_phys_addr |= (page_dir_entry & 0xffc00000);
            guest_phys_addr |= ((page_dir_entry & 0x1fe000) << 19);

            //check bits type for struct page_bits
            if ((page_dir_entry & 0x2) == 0x2)
                pb->write = 1;
            else
                pb->write = 0;

            if ((page_dir_entry & 0x20) == 0x20)
                pb->accessed = 1;
            else
                pb->accessed = 0;

            if ((page_dir_entry & 0x40) == 0x40)
                pb->dirty = 1;
            else
                pb->dirty = 0;
            pb->global = (page_dir_entry & 0x100) == 0x100;
            //end setting of page bits

            size = 0x400000;
            karma_log(DEBUG, "detected 4MB page returning \n");
            return true;
        }

        // Step 2 resolve page table entry address
        unsigned long page_table_entry_addr = 0x0;

        unsigned long pde_req_bits = page_dir_entry & 0xfffff000; //only bits 31-12
        unsigned long linear_addr_for_pte = fault_addr & 0x3ff000; //only bits 21-12
        unsigned long linear_addr_req_bits_for_pte = linear_addr_for_pte >> 10; //has bits in pos 2 to 11, rest are 0
        page_table_entry_addr |= pde_req_bits;
        page_table_entry_addr |= linear_addr_req_bits_for_pte;

        unsigned long page_table_entry = *((unsigned int*) (base_pointer
                + page_table_entry_addr));

        karma_log(DEBUG, "page table entry val is %lx \n", page_table_entry);

        if ((page_table_entry & 0x1) == 0x1) {
            karma_log(DEBUG, "contains a mapping to a page \n");
        } else {
            karma_log(DEBUG, "does not contain a mapping to a page \n");
            return false;
        }

        // Step 3 get the final guest physical i.e. vmm virtual address

        unsigned long pte_req_bits = page_table_entry & 0xfffff000; //only bits 31-12
        unsigned long linear_addr_req_bits_for_gpa = fault_addr & 0xfff;
        guest_phys_addr |= pte_req_bits;
        guest_phys_addr |= linear_addr_req_bits_for_gpa;

        //check bits type for struct page_bits
        if ((page_table_entry & 0x2) == 0x2)
            pb->write = 1;
        else
            pb->write = 0;

        if ((page_table_entry & 0x20) == 0x20)
            pb->accessed = 1;
        else
            pb->accessed = 0;

        if ((page_table_entry & 0x40) == 0x40)
            pb->dirty = 1;
        else
            pb->dirty = 0;
        pb->global = (page_table_entry & 0x100) == 0x100;
        //end setting of page bits

        karma_log(DEBUG, "Successfully resolved gpa \ngpa= (%llx)\n", guest_phys_addr);

        size = 0x1000;
        return true;
    }

    inline void resume(){
        if(_guest_enabled_paging)
            current_vcpu().resumeUser(_shadowPageDB.cap());
    }

    inline void intercept(vmcx_t & vmcx, gpregs_t & gpregs, L4_vm_driver::Exitcode & code){
        VTLB_EXTENSION_SPECIFICS<arch::x86, EXTENSION>::intercept(*this, vmcx, _cdregs, gpregs, code);
    }

    void create_fiasco_mapping(l4_addr_t dest, l4_addr_t guest_phy_addr, struct page_bits pb, unsigned long size, bool global) {
        unsigned int poweroftwo = (size == 0x1000? 12 : 22);

        unsigned int write_flag;
        l4_addr_t source;

        if(pb.write==0)
            write_flag = 0;
        if(pb.write==1)
        {
            if(pb.dirty==1)
                write_flag = 1;
            else write_flag = 0;
        }

        GET_VM.mem().phys_to_karma(guest_phy_addr, source);

        _shadowPageDB.insert(dest, source, poweroftwo, write_flag, global);
    }



    inline void page_fault_handler(vmcx_t & vmcx, l4_addr_t fault_addr, unsigned long error_code = 0) {
        //printf("%s, _guest_enabled_paging=%d\n", __func__, _guest_enabled_paging);
        if (!_guest_enabled_paging){
            printf("#PF while guest paging was not yet enabled.\n");
            exit(1);
        } else {
            unsigned long long guest_phy_addr;
            unsigned long size = 0;
            page_bits pb;
            pb.accessed = 0;
            pb.dirty = 0;
            pb.write = 0;
            pb.global = 0;
            bool page_table_walker_result = page_table_walker(fault_addr,
                    guest_phy_addr, &pb, size);
            karma_log(DEBUG, "gva= (%lx) gpa= (%llx) write bit: %d dirty bit:%d error code: %lx walker %hhu\n", fault_addr, guest_phy_addr, pb.write, pb.dirty, error_code, page_table_walker_result);

            if ((error_code & 0x1) == 0x1) {
                //page protection violation
                if (pb.write == 0) {
                    //actual fault violation, writing to a page which is only readable
                    karma_log(DEBUG, "pf inject (rights) %lx ec: %lx\n", fault_addr, error_code);
                    inject_page_fault_to_guest(vmcx);
                } else {
                    //detecting the need to set the dirty bit in the guest page table
                    // and remapping the page in the shadow page table as rw instead of ro
                    modify_guest_dirty_bit(fault_addr);
                    pb.dirty = 1;
                    karma_log(DEBUG, "remap dirty page %lx->%llx w:d=%d:%d ec: %lx\n",
                            fault_addr, guest_phy_addr, pb.write, pb.dirty, error_code);
                    unmap_violation_page(fault_addr);
                    create_fiasco_mapping(fault_addr, guest_phy_addr, pb, size, pb.global);
                }
            } else {
                //page not present
                if (page_table_walker_result) {
                    //create mapping in the shadow page table
                    create_fiasco_mapping(fault_addr, guest_phy_addr, pb, size, pb.global);
                } else {
                    //couldnt find mapping in gpt, inject page fault to the guest
                    karma_log(DEBUG, "pf inject (not present)  %lx ec: %lx\n", fault_addr, error_code);
                    inject_page_fault_to_guest(vmcx);
                }
            }
        }
    }

    void modify_guest_dirty_bit(l4_addr_t fault_addr) {
        char *base_pointer = (char*) GET_VM.mem().base_ptr();
        unsigned long page_dir_entry_addr = 0x0;

        unsigned long linear_addr_for_pde = fault_addr & 0xffc00000;
        unsigned long cur_cr3_req_bits = _cdregs.cr3 & 0xfffff000;
        unsigned long linear_addr_req_bits_for_pde = linear_addr_for_pde >> 20;
        page_dir_entry_addr |= cur_cr3_req_bits;
        page_dir_entry_addr |= linear_addr_req_bits_for_pde;

        unsigned long page_dir_entry = *((unsigned int*) (base_pointer
                + page_dir_entry_addr));

        if ((page_dir_entry & 0x80) == 0x80) {
            // large page
            //modifying the dirty bit
            page_dir_entry |= 0x40;
            *((unsigned int*) (base_pointer + page_dir_entry_addr))
                    = page_dir_entry;

            return;
        }

        unsigned long page_table_entry_addr = 0x0;

        unsigned long pde_req_bits = page_dir_entry & 0xfffff000;
        unsigned long linear_addr_for_pte = fault_addr & 0x3ff000;
        unsigned long linear_addr_req_bits_for_pte = linear_addr_for_pte >> 10;
        page_table_entry_addr |= pde_req_bits;
        page_table_entry_addr |= linear_addr_req_bits_for_pte;

        unsigned long page_table_entry = *((unsigned int*) (base_pointer
                + page_table_entry_addr));

        //modifying the dirty bit
        page_table_entry |= 0x40;
        *((unsigned int*) (base_pointer + page_table_entry_addr))
                = page_table_entry;

        return;
    }

    void inject_page_fault_to_guest(vmcx_t & vmcx) {
        VTLB_EXTENSION_SPECIFICS<arch::x86, EXTENSION>::inject_page_fault_to_guest(vmcx);
    }

    inline bool did_guest_enable_paging() const{
        return _guest_enabled_paging;
    }
    inline void set_guest_paging_enabled() {
        _guest_enabled_paging = true;
    }

    static int disassembly_hook(ud_t *context) {
        unsigned long *aux_info = (unsigned long *) ud_get_user_opaque_data(
                context);
        karma_log(DEBUG, "disassembly_hook called, rip=%lx\n", aux_info[0]);
        return (int) *(char *) (aux_info[1] + (aux_info[0]++));
    }

    static int paged_disassembly_hook(ud_t *context) {
        unsigned long *aux_info = (unsigned long *) ud_get_user_opaque_data(
                context);
        VTLB *tlb_handling = (VTLB *) aux_info[2];

        unsigned long long phys_addr;
        unsigned long size;
        struct page_bits pb;

        bool rv = tlb_handling->page_table_walker((l4_addr_t) aux_info[0], phys_addr,
                &pb, size);

        if (!rv) {
            //should never happen
            printf("paged address for diassembly could not be resolved \n");
            throw L4_PANIC_EXCEPTION;
        }

        karma_log(DEBUG, "page_disassembly_hook called, rip=%lx (%llx)\n", aux_info[0],
                phys_addr);

        aux_info[0]++;

        return (int) *(char *) (aux_info[1] + phys_addr);
    }

    int get_instruction_size_type(unsigned int& reg_type, uint8_t mode_bits, l4_umword_t ip) {
        ud_t ud_obj;
        ud_init(&ud_obj);
        ud_set_syntax(&ud_obj, UD_SYN_ATT);
        ud_set_mode(&ud_obj, mode_bits);

        unsigned long aux_info[4];
        char instr_string[30];

        aux_info[0] = ip;
        //aux_info[0] = (unsigned long) vmcs.guest_area.rip;
        aux_info[1] = (unsigned long) GET_VM.mem().base_ptr();
        aux_info[2] = (unsigned long) this;

        if (_guest_enabled_paging)
            ud_set_input_hook(&ud_obj, &paged_disassembly_hook);
        else
            ud_set_input_hook(&ud_obj, &disassembly_hook);
        ud_set_user_opaque_data(&ud_obj, (void*) &aux_info[0]);

        int bytes = ud_disassemble(&ud_obj);
        strcpy(instr_string, ud_insn_asm(&ud_obj));
        reg_type = analyze_instr(instr_string);
        karma_log(DEBUG,"disassembled: %s reg type:%d\n", instr_string, reg_type);
        return bytes;
    }

    void analyze_provided_rip(unsigned long rip) {
        ud_t ud_obj;
        ud_init(&ud_obj);
        ud_set_syntax(&ud_obj, UD_SYN_ATT);
        ud_set_mode(&ud_obj, 32);

        unsigned long aux_info[4];
        char instr_string[30];

        aux_info[0] = (unsigned long) rip;
        aux_info[1] = (unsigned long) GET_VM.mem().base_ptr();
        aux_info[2] = (unsigned long) this;

        ud_set_input_hook(&ud_obj, &paged_disassembly_hook);
        ud_set_user_opaque_data(&ud_obj, (void*) &aux_info[0]);

        ud_disassemble(&ud_obj);
        strcpy(instr_string, ud_insn_asm(&ud_obj));
        printf("disassembled: %s \n", instr_string);
    }


    static int analyze_instr(char *instr_string) {
        if (cpu_option == AMD_cpu) {
            if (strstr(instr_string, "eax") != NULL)
                return EAX_REG_AMD;
            else if (strstr(instr_string, "edx") != NULL)
                return EDX_REG_AMD;
            else if (strstr(instr_string, "ecx") != NULL)
                return ECX_REG_AMD;
            else if (strstr(instr_string, "ebx") != NULL)
                return EBX_REG_AMD;
            else if (strstr(instr_string, "ebp") != NULL)
                return EBP_REG_AMD;
            else if (strstr(instr_string, "esi") != NULL)
                return ESI_REG_AMD;
            else if (strstr(instr_string, "edi") != NULL)
                return EDI_REG_AMD;
            else if (strstr(instr_string, "clts") != NULL)
                return CLTS_INSTR_AMD;
            else
                return UNKNOWN_INSTR_AMD;
        }

        if (cpu_option == Intel_cpu) {
            if (strstr(instr_string, "eax") != NULL)
                return EAX_REG_INTEL;
            else if (strstr(instr_string, "edx") != NULL)
                return EDX_REG_INTEL;
            else if (strstr(instr_string, "ecx") != NULL)
                return ECX_REG_INTEL;
            else if (strstr(instr_string, "ebx") != NULL)
                return EBX_REG_INTEL;
            else if (strstr(instr_string, "ebp") != NULL)
                return EBP_REG_INTEL;
            else
                return -1;
        }
        return -1;
    }
    inline void sanitize_vmcx(vmcx_t & vmcx){
        VTLB_EXTENSION_SPECIFICS<arch::x86, EXTENSION>::sanitize_vmcx(vmcx, _cdregs, *this);
    }
};

template <>
class VTLB_EXTENSION_SPECIFICS<arch::x86, arch::x86::svm>{
private:
    typedef VTLB<arch::x86, arch::x86::svm> tlb_handling_t;
    typedef GP_REGS<arch::x86, arch::x86::svm> gpregs_t;
public:

    static void init(l4_vm_svm_vmcb_t & vmcb){
        vmcb.control_area.np_enable = 0;
        vmcb.control_area.intercept_rd_crX = 0x19;
        vmcb.control_area.intercept_wr_crX = 0x19;
        //intercept INVLPG/A and CR0 intercept(exit code 0x65) intructions
        vmcb.control_area.intercept_instruction0 |= 0x06000020;
    }

    static void intercept(tlb_handling_t & tlb_handling, l4_vm_svm_vmcb_t & vmcb, CDregs<arch::x86::svm> & cdregs, gpregs_t & gpregs, L4_vm_driver::Exitcode & code){
        // debug
        if (vmcb.control_area.exitcode == 0x41) {
            karma_log(INFO, "debug exception intercept eip=%llx, eax=%x\n",
                    vmcb.state_save_area.rip, (unsigned int)gpregs.ax);
            tlb_handling.analyze_provided_rip(vmcb.state_save_area.rip);
            // intercept iret
            vmcb.control_area.intercept_instruction0 |= 1 << 20;
        }
        if(vmcb.control_area.exitcode == 0x7d){ // task switch
            vmcb.state_save_area.rip++;
            code = L4_vm_driver::Handled;
            return;
        }
        if (vmcb.control_area.exitcode == 0x79
                || vmcb.control_area.exitcode == 0x7a) // INVLPG/A instruction
        {
            //_vmcb->control_area.intercept_instruction0 =
            //    _vmcb->control_area.intercept_instruction0 &~ 0x2000000;
            unsigned int reg_type;
            int skip_bytes = tlb_handling.get_instruction_size_type(reg_type, 32, vmcb.state_save_area.rip);
            invalidate_tlb_entry(tlb_handling, reg_type, gpregs, vmcb);
            vmcb.state_save_area.rip += skip_bytes;
            code = L4_vm_driver::Handled;
            return;
        }
        if (vmcb.control_area.exitcode == 0x74) // iret
        {
            karma_log(DEBUG, "iret intercept eip=%llx\n", vmcb.state_save_area.rip);

            unsigned long long phys_addr;
            unsigned long size;
            struct page_bits pb;

            bool rv = tlb_handling.page_table_walker(
                    (l4_addr_t) vmcb.state_save_area.rsp, phys_addr, &pb,
                    size);

            if (!rv) {
                //should never happen
                printf(
                        "Error: paged address for diassembly could not be resolved \n");
            } else {
                //printing the stack
                karma_log(DEBUG, "%08lx %08lx %08lx %08lx %08lx\n",
                        ((unsigned long *) GET_VM.mem().base_ptr()) [phys_addr / 4],
                        ((unsigned long *) GET_VM.mem().base_ptr()) [phys_addr / 4 + 1],
                        ((unsigned long *) GET_VM.mem().base_ptr()) [phys_addr / 4 + 2],
                        ((unsigned long *) GET_VM.mem().base_ptr()) [phys_addr / 4 + 3],
                        ((unsigned long *) GET_VM.mem().base_ptr()) [phys_addr / 4 + 4]);
            }

//            vmcb.control_area.intercept_instruction0
//                    = _vmcb->control_area.intercept_instruction0 & ~0x100000;
            code = L4_vm_driver::Handled;
            return;
        }
        if (vmcb.control_area.exitcode == 0x4e) {
            if (!params.vtlb) {
                printf(
                        "Encountered a page fault intercept. Since we are not using a vtlb, this is an error!\n");
                throw L4_EXCEPTION("page fault without vtlb");
            }
            karma_log(TRACE, "Encountered a page fault.\nAddress: %lx\tCode: %lx\n",
                    (unsigned long)vmcb.control_area.exitinfo2,
                    (unsigned long)vmcb.control_area.exitinfo1);
            GET_LSTATS_ITEM(pf_while_x).addMeasurement((vmcb.state_save_area.rflags & 0x200) && !(vmcb.control_area.interrupt_shadow & 0x1), 0);

            // qemu and maybe certain hardware does not reset the eventinj valid flag
            vmcb.control_area.eventinj = 0;
            tlb_handling.page_fault_handler(vmcb, vmcb.control_area.exitinfo2, vmcb.control_area.exitinfo1 /*error_code*/);
            if(vmcb.control_area.exitintinfo & 0x80000000){
                if(vmcb.control_area.eventinj & 0x80000000){
                    code = L4_vm_driver::Error;
                    printf("Fatal: Non shadow #PF while delivering event/interrupt! exitinfo1 0x%llx exitinfo2 0x%llx exitintinfo 0x%llx gtd_base 0x%llx idt_base 0x%llx ldt_base 0x%llx\n", vmcb.control_area.exitinfo1, vmcb.control_area.exitinfo2, vmcb.control_area.exitintinfo, vmcb.state_save_area.gdtr.base, vmcb.state_save_area.idtr.base, vmcb.state_save_area.ldtr.base);
                    return;
                }
                vmcb.control_area.eventinj = vmcb.control_area.exitintinfo;
            }
            code = L4_vm_driver::Handled;
            return;
        }
        //cr3 guest page table write
        if ((vmcb.control_area.exitcode == 0x0)
                || (vmcb.control_area.exitcode == 0x3)
                || (vmcb.control_area.exitcode == 0x4)
                || (vmcb.control_area.exitcode == 0x26)
                || (vmcb.control_area.exitcode == 0x27)
                || (vmcb.control_area.exitcode == 0x10)
                || (vmcb.control_area.exitcode == 0x13)
                || (vmcb.control_area.exitcode == 0x14)
                || (vmcb.control_area.exitcode == 0x36)
                || (vmcb.control_area.exitcode == 0x37)
                || (vmcb.control_area.exitcode == 0x65)) {

            unsigned int reg_type;
            int skip_bytes = tlb_handling.get_instruction_size_type(reg_type, 32, vmcb.state_save_area.rip);
            register_read_write_handler(tlb_handling, vmcb, reg_type, cdregs, gpregs);
            vmcb.state_save_area.rip += skip_bytes;
            code = L4_vm_driver::Handled;
            return;
        }
    }

    inline void static inject_page_fault_to_guest(l4_vm_svm_vmcb_t & vmcb) {
        unsigned long long eve_inj = 0x0;
        unsigned long long idt_bit_vector = 0x0e;
        unsigned long long error_code = vmcb.control_area.exitinfo1;
        unsigned long long error_code_req_bits = error_code << 32;
        eve_inj |= idt_bit_vector;
        eve_inj |= (3ULL << 8); // type 3: hardware exception
        eve_inj |= (1ULL << 11); // error code required
        eve_inj |= (1ULL << 31); // inject exception
        eve_inj |= error_code_req_bits;

        GET_LSTATS_ITEM(pf_while_x).addMeasurement(2 + ((vmcb.state_save_area.rflags & 0x200) && !(vmcb.control_area.interrupt_shadow & 0x1)), 0);

        if(vmcb.control_area.eventinj & 0x80000000){
            printf("Trying to inject #PF while event pending 0x%llx\n", vmcb.control_area.eventinj);
            exit(1);
        }
        vmcb.control_area.eventinj = eve_inj;
        //_vmcb->control_area.interrupt_ctl = 0;
        //set the cr2 value for the guest
        vmcb.state_save_area.cr2 = vmcb.control_area.exitinfo2;
    }

#define _TO_STRING(x) #x
#define TO_STRING(x) _TO_STRING(x)

    inline static l4_umword_t & choose_gpreg(const unsigned int reg_type, gpregs_t & gpregs){
        switch(reg_type){
        case EAX_REG_AMD:
            return gpregs.ax;
        case EBX_REG_AMD:
            return gpregs.bx;
        case ECX_REG_AMD:
            return gpregs.cx;
        case EDX_REG_AMD:
            return gpregs.dx;
        case EBP_REG_AMD:
            return gpregs.bp;
        case ESI_REG_AMD:
            return gpregs.si;
        case EDI_REG_AMD:
            return gpregs.di;
        }
        printf("@" __FILE__ ":" TO_STRING(__LINE__) ": Unknown operand requested %u\n", reg_type);
        enter_kdebug();
        static l4_umword_t dummy;
        return dummy;
    }

    inline static l4_umword_t & choose_creg(const l4_uint64_t exitcode, CDregs<arch::x86::svm> & cdregs){
        if(exitcode & 0x20){
            // debug register
            switch(exitcode & 0xf){
            case 6:
                return cdregs.dr6;
            case 7:
                return cdregs.dr7;
            }
       } else {
            // control register
            switch(exitcode & 0xf){
            case 0:
                return cdregs.cr0;
            case 3:
                return cdregs.cr3;
            case 4:
                return cdregs.cr4;
            }

        }
        //printf("@" __FILE__ ":" TO_STRING(__LINE__) ": Unknown operand requested %u\n", reg_type);
        enter_kdebug();
        static l4_umword_t dummy;
        return dummy;
    }

    inline static void register_read_write_handler(tlb_handling_t & tlb_handling, l4_vm_svm_vmcb_t & vmcb, unsigned int reg_type, CDregs<arch::x86::svm> & cdregs, gpregs_t & gpregs) {
        if (reg_type == UNKNOWN_INSTR_AMD) {
            printf(
                    "unknown read/write register type or instruction for modification\n\n");
            throw L4_PANIC_EXCEPTION;
        }
        if(vmcb.control_area.exitcode == 0x65){
            printf("VMEXIT_CR0_SEL_WRITE occurred");
        }
        if(vmcb.control_area.exitcode & 0x10){
            // write access
            if (vmcb.control_area.exitcode == 0x10){ //|| vmcb.control_area.exitcode == 0x65) {

                tlb_handling.set_guest_paging_enabled();

                unsigned long long cr0_val = 0;
                if(reg_type == CLTS_INSTR_AMD){
                    cr0_val = cdregs.cr0;
                    cr0_val = cr0_val & ~0x8;
                } else {
                    cr0_val = choose_gpreg(reg_type, gpregs);
                }

                cdregs.cr0 = cr0_val;
            } else {
                l4_umword_t & src_reg = choose_gpreg(reg_type, gpregs);
                l4_umword_t & dest_reg = choose_creg(vmcb.control_area.exitcode, cdregs);

                dest_reg = src_reg;

                // special cases
                switch(vmcb.control_area.exitcode){
                case 0x13: // write cr3
                case 0x14: // write cr4
                    // flush tlb on next resume
                    vmcb.control_area.guest_asid_tlb_ctl = 0x100000000ULL;

                    tlb_handling.tlb_flush_emulation();
                    break;
                }
            }
        } else {
            // read access
            l4_umword_t & dest_reg = choose_gpreg(reg_type, gpregs);
            l4_umword_t & src_reg = choose_creg(vmcb.control_area.exitcode, cdregs);

            dest_reg = src_reg;

            if(vmcb.control_area.exitcode == 0x0)
                // if we read the cr0 register we have do make a special check
                if (!tlb_handling.did_guest_enable_paging()){
                    dest_reg &= ~0x80000000;
                }
        }
    }

   static void invalidate_tlb_entry(tlb_handling_t & tlb_handling, unsigned int reg_type, gpregs_t & gpregs, l4_vm_svm_vmcb_t __attribute__((unused)) & vmcb) {
       // flush tlb on next resume
       vmcb.control_area.guest_asid_tlb_ctl = 0x100000000ULL;
        //virtual address of the page to be invalidated
        l4_addr_t mem_addr = choose_gpreg(reg_type, gpregs);
        tlb_handling.unmap_violation_page(mem_addr);
    }

   inline static void sanitize_vmcx(l4_vm_svm_vmcb_t & vmcb, CDregs<arch::x86::svm> & cdregs, tlb_handling_t __attribute__((unused)) & tlb_handling) {

       //set the TS bit to 0
       cdregs.cr0 &= 0xFFFFFFF7;
       //enable paging
       cdregs.cr0 |= 0x80000001;
       //set the MP and NE bits for cr0
       cdregs.cr0 |= 0x22;
       //clear the EM bit for cr0
       cdregs.cr0 &= 0xFFFFFFFB;
       //set the OSFXSR bit and OSXMMEXCPT bit in cr4 to enable fast CPU save and restore
       cdregs.cr4 |= 0x600;

       cdregs.toVMCX(vmcb);

       vmcb.control_area.intercept_exceptions = 0xffffffff;

       // intercept accesses to cr0, cr3 and cr4
       vmcb.control_area.intercept_rd_crX = 0x19;
       vmcb.control_area.intercept_wr_crX = 0x19;

   }


};

template <>
class VTLB_EXTENSION_SPECIFICS<arch::x86, arch::x86::vmx>{
private:
    typedef GP_REGS<arch::x86, arch::x86::vmx> gpregs_t;

    inline static void advance_rIP(Vmcs & vmcs) {
        vmcs.vmwrite<VMX_GUEST_RIP> (vmcs.vmread<VMX_GUEST_RIP> ()
                + vmcs.vmread<VMX_EXIT_INSTRUCTION_LENGTH>());
    }

public:
    typedef VTLB<arch::x86, arch::x86::vmx> tlb_handling_t;

    static void init(Vmcs __attribute__((unused)) & vmcb){
    }

    static L4_vm_driver::Exitcode intercept(tlb_handling_t & tlb_handling, Vmcs & vmcs, CDregs<arch::x86::vmx> & cdregs, gpregs_t & gpregs, L4_vm_driver::Exitcode & code){
        l4_uint32_t exit_reason = vmcs.vmread<VMX_EXIT_REASON> ();
        l4_uint32_t exit_interrupt_info = vmcs.vmread<VMX_EXIT_INTERRUPT_INFO>();

        switch (exit_reason) {
        case 0: // exception or NMI
            switch ((exit_interrupt_info & 0xff)) {
            case 0x6:
                printf("Invalid opcode @ 0x%lx\n",
                        vmcs.vmread<VMX_GUEST_RIP> ());
                code = L4_vm_driver::Error;
                break;
            case 0x8:
                printf("Double page fault encountered.\n");
                code = L4_vm_driver::Error;
                break;
            case 0xd:
                printf("exit_interrupt_info=0x%x\n", exit_interrupt_info);
                printf("General protection fault encountered.\n");
                code = L4_vm_driver::Error;
                break;
            case 0xe:
                tlb_handling.page_fault_handler(vmcs, vmcs.vmread<VMX_EXIT_QUALIFICATION>(), vmcs.vmread<VMX_EXIT_INTERRUPT_ERROR>());
//                intercepts[2]++;
                code = L4_vm_driver::Handled;
                break;
            }
            break;
        case 14: // invlpg
            tlb_handling.unmap_violation_page(vmcs.vmread<VMX_EXIT_QUALIFICATION>());
            advance_rIP(vmcs);
            code = L4_vm_driver::Handled;
            break;
        case 28: // control register access
            register_read_write_handler(tlb_handling, vmcs, cdregs, gpregs);
        case 29: // debug register access (ignore debugging in the guest will not work)
            advance_rIP(vmcs);
            code = L4_vm_driver::Handled;
            break;
        }

        return code;
    }

    inline void static inject_page_fault_to_guest(Vmcs & vmcs) {
        l4_uint32_t eve_inj = 0x0;
        l4_uint32_t idt_bit_vector = 0x0e;
        eve_inj |= idt_bit_vector;
        eve_inj |= (3UL << 8); // type 3: hardware exception
        eve_inj |= (1UL << 11); // error code required
        eve_inj |= (1UL << 31); // inject exception

        vmcs.vmwrite<VMX_ENTRY_INTERRUPT_INFO> (eve_inj);
        vmcs.vmwrite<VMX_ENTRY_EXCEPTION_ERROR> (vmcs.vmread<VMX_EXIT_INTERRUPT_ERROR> ());
        //vmcs.entry_control.interruption_information = eve_inj;
        //vmcs.entry_control.exception_error = vmcs.exit_information.interruption_error;
        //need to explicitly set the cr2 register with the page fault address
        vmcs.vmwrite<L4_VM_VMX_VMCS_CR2> (vmcs.vmread<VMX_EXIT_QUALIFICATION> ());
        //vmcs.cr2 = vmcs.exit_information.exit_qualification;
    }

    inline static l4_umword_t & choose_cdreg(Vmcs & vmcs, CDregs<arch::x86::vmx> & cdregs){
        // this Funktion should be called only for one of two exit reasons (28 or 29)
        unsigned int cr_dr_num = (vmcs.vmread<VMX_EXIT_QUALIFICATION> () & 0xf);

        if(vmcs.vmread<VMX_EXIT_REASON>() == 28){
            // controll register
            switch(cr_dr_num){
            case 0:
                return cdregs.cr0;
            case 3:
                return cdregs.cr3;
            case 4:
                return cdregs.cr4;
            }
        } else {  //if(vmcs.vmread<VMX_EXIT_REASON>() == 29){
            // debug register
            switch(cr_dr_num){
            case 7:
                return cdregs.dr7;
            }
        }
        printf("unknown control/debug register %u (exitreason %u rip 0x%lu) ", cr_dr_num, vmcs.vmread<VMX_EXIT_REASON>(), vmcs.vmread<VMX_GUEST_RIP>());
        exit(1);
    }

    inline static l4_umword_t & choose_gpreg(Vmcs & vmcs, gpregs_t & gpregs){
        unsigned int gpr_num = ((vmcs.vmread<VMX_EXIT_QUALIFICATION> () & 0xf00) >> 8);
        switch(gpr_num){
        case EAX_REG_INTEL:
            return gpregs.ax;
            break;
        case ECX_REG_INTEL:
            return gpregs.cx;
            break;
        case EDX_REG_INTEL:
            return gpregs.dx;
            break;
        case EBX_REG_INTEL:
            return gpregs.bx;
            break;
        case ESI_REG_INTEL:
            return gpregs.si;
            break;
        }
        printf("unknown gpreg number %u\n", gpr_num);
        exit(1);
    }

    static void register_read_write_handler(tlb_handling_t & tlb_handling, Vmcs & vmcs, CDregs<arch::x86::vmx> & cdregs, gpregs_t & gpregs) {
        unsigned int access_type;

        access_type = ((vmcs.vmread<VMX_EXIT_QUALIFICATION> () & 0x30) >> 4);
        if(vmcs.vmread<VMX_EXIT_REASON> () == 29)
            access_type &= 0x1; // if we handle a debug register all other bits are meaningless

        if(access_type < 2){
            l4_umword_t & gpreg = choose_gpreg(vmcs, gpregs);
            l4_umword_t & cdreg = choose_cdreg(vmcs, cdregs);
            if(access_type == 0){ // write
                cdreg = gpreg;

                if(&cdreg == &cdregs.cr0) // if writing to cr0
                    tlb_handling.set_guest_paging_enabled();

                if(&cdreg == &cdregs.cr3 || &cdreg == &cdregs.cr4){ // if writing to cr4 or cr3
                    tlb_handling.tlb_flush_emulation();
                }

           } else { // read
                gpreg = cdreg;

                if(&cdreg == &cdregs.cr0) // if reading from cr0
                    if(tlb_handling.did_guest_enable_paging())
                        gpreg &= ~0x80000000;
            }
        } else if(access_type == 2){ // clts clear task switch
            cdregs.cr0 &= ~0x8;
        }
    }

    inline static void sanitize_vmcx(Vmcs & vmcs, CDregs<arch::x86::vmx> & cdregs, tlb_handling_t & tlb_handling) {
        //VMX_EXIT_REASON ist read only vmcs.vmwrite<VMX_EXIT_REASON>((l4_uint32_t)0);
        //vmcs.exit_information.exit_reason=0;

        //set the TS bit to 0
        cdregs.cr0 &= 0xFFFFFFF7;
        //enable paging
        cdregs.cr0 |= 0x80000001;
        //set the MP and NE bits for cr0
        cdregs.cr0 |= 0x22;
        //clear the EM bit for cr0
        cdregs.cr0 &= 0xFFFFFFFB;
        //set the OSFXSR bit and OSXMMEXCPT bit in cr4 to enable fast CPU save and restore
        cdregs.cr4 |= 0x600;
        //set the VMX bit
        cdregs.cr4 |= (0x1 << 13);

        vmcs.vmwrite<VMX_GUEST_CR0> (cdregs.cr0);
        vmcs.vmwrite<VMX_GUEST_CR3> (cdregs.cr3);
        vmcs.vmwrite<VMX_GUEST_CR4> (cdregs.cr4);
        vmcs.vmwrite<VMX_GUEST_DR7> (cdregs.dr7);

        vmcs.vmwrite<VMX_CR0_READ_SHADOW> (cdregs.cr0);
        //vmcs.execution_control.cr0_read_shadow = reg_tracker.cr0;
        //set the paging bit to be 0 in the read shadow so that, on the paging bit being enabled by cr0 we can intercept it
        if (!tlb_handling.did_guest_enable_paging())
            vmcs.vmwrite<VMX_CR0_READ_SHADOW> (vmcs.vmread<VMX_CR0_READ_SHADOW> () & 0x7fffffff);

        vmcs.vmwrite<VMX_CR4_READ_SHADOW> (cdregs.cr4);

        //exits on hlt, invlpg, r/w of CR3, r/w of DR
        l4_uint32_t procbased_ctrl = vmcs.vmread<VMX_PRIMARY_EXEC_CTRL> ();
        vmcs.vmwrite<VMX_PRIMARY_EXEC_CTRL> (procbased_ctrl | 0x818280);
        //vmcs.execution_control.primary_proc_exec_ctls = 0x818280;
        // bits 1, 4-6, 8, 13-16 and 26 need to be set to 1, test whether this is enforced by the kernel
        procbased_ctrl = vmcs.vmread<VMX_PRIMARY_EXEC_CTRL> ();
        vmcs.vmwrite<VMX_PRIMARY_EXEC_CTRL> (procbased_ctrl | 0x401e172);

    }



};
#endif /* VTLB_HPP_ */
