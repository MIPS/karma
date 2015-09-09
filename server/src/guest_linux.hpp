/*
 * guest_linux.hpp - TODO enter description
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
/*
 * guest_linux.hpp
 *
 *  Created on: 26.10.2010
 */

#ifndef GUEST_LINUX_HPP_
#define GUEST_LINUX_HPP_

#include "guest_init.hpp"
#include "params.h"
#include "l4_vm.h"

#include "util/debug.h"

// offsets into struct boot_params (zero page)
// Boot Params Offset (BPO)
#define BPO_HDR 0x1f1

struct setup_header
{
    l4_uint8_t  setup_sects;
    l4_uint16_t root_flags;
    l4_uint32_t syssize;
    l4_uint16_t ram_size;
#define RAMDISK_IMAGE_START_MASK    0x07FF
#define RAMDISK_PROMPT_FLAG     0x8000
#define RAMDISK_LOAD_FLAG       0x4000
    l4_uint16_t vid_mode;
    l4_uint16_t root_dev;
    l4_uint16_t boot_flag;
    l4_uint16_t jump;
    l4_uint32_t header;
    l4_uint16_t version;
    l4_uint32_t realmode_swtch;
    l4_uint16_t start_sys;
    l4_uint16_t kernel_version;
    l4_uint8_t  type_of_loader;
    l4_uint8_t  loadflags;
#define LOADED_HIGH (1<<0)
#define QUIET_FLAG  (1<<5)
#define KEEP_SEGMENTS   (1<<6)
#define CAN_USE_HEAP    (1<<7)
    l4_uint16_t setup_move_size;
    l4_uint32_t code32_start;
    l4_uint32_t ramdisk_image;
    l4_uint32_t ramdisk_size;
    l4_uint32_t bootsect_kludge;
    l4_uint16_t heap_end_ptr;
    l4_uint16_t _pad1;
    l4_uint32_t cmd_line_ptr;
    l4_uint32_t initrd_addr_max;
    l4_uint32_t kernel_alignment;
    l4_uint8_t  relocatable_kernel;
    l4_uint8_t  _pad2[3];
    l4_uint32_t cmdline_size;
    l4_uint32_t hardware_subarch;
    l4_uint64_t hardware_subarch_data;
    l4_uint32_t payload_offset;
    l4_uint32_t payload_length;
    l4_uint64_t setup_data;
} __attribute__((packed));


namespace guest{

template <>
class VMCX<guest::Linux, arch::x86, arch::x86::svm>
{
public:
    typedef l4_vm_svm_vmcb_t type;

    static void init(void * vmcb)
    {
        l4_vm_svm_vmcb_t * _vmcb = reinterpret_cast<l4_vm_svm_vmcb_t *>(vmcb);
//        if(params.vtlb)
//            _vmcb->control_area.np_enable = 0;
//        else
            _vmcb->control_area.np_enable = 1;
        //_vmcb->control_area.guest_asid_tlb_ctl = params.asid;

        // according to Documentation/x86/boot.txt
        // CS = __BOOT_CS,
        // DS = ES = SS = __BOOT_DS
        _vmcb->state_save_area.cs.selector = 0x10;
        _vmcb->state_save_area.cs.attrib = 0xc9a;
        _vmcb->state_save_area.cs.limit = 0xffffffff;
        _vmcb->state_save_area.cs.base = 0ULL;

        _vmcb->state_save_area.ds.selector = 0x18;
        _vmcb->state_save_area.ds.attrib = 0xc92;
        _vmcb->state_save_area.ds.limit = 0xffffffff;
        _vmcb->state_save_area.ds.base = 0ULL;

        _vmcb->state_save_area.es.selector = 0x18;
        _vmcb->state_save_area.es.attrib = 0xc92;
        _vmcb->state_save_area.es.limit = 0xffffffff;
        _vmcb->state_save_area.es.base = 0ULL;

        _vmcb->state_save_area.ss.selector = 0x18;
        _vmcb->state_save_area.ss.attrib = 0xc92;
        _vmcb->state_save_area.ss.limit = 0xffffffff;
        _vmcb->state_save_area.ss.base = 0ULL;

        // those are not documented, therefore I set them to 0
        _vmcb->state_save_area.fs.selector = 0;
        _vmcb->state_save_area.fs.attrib = 0xcf3;
        _vmcb->state_save_area.fs.limit = 0xffffffff;
        _vmcb->state_save_area.fs.base = 0ULL;

        _vmcb->state_save_area.gs.selector = 0;
        _vmcb->state_save_area.gs.attrib = 0xcf3;
        _vmcb->state_save_area.gs.limit = 0xffffffff;
        _vmcb->state_save_area.gs.base = 0ULL;

        _vmcb->state_save_area.gdtr.selector = 0;
        _vmcb->state_save_area.gdtr.attrib = 0;
        _vmcb->state_save_area.gdtr.limit = 0x3f;
        _vmcb->state_save_area.gdtr.base = (unsigned long) GET_VM.os().get_gdt();

        _vmcb->state_save_area.ldtr.selector = 0;
        _vmcb->state_save_area.ldtr.attrib = 0;
        _vmcb->state_save_area.ldtr.limit = 0xff;
        _vmcb->state_save_area.ldtr.base = (unsigned long) GET_VM.os().get_idt();

        _vmcb->state_save_area.tr.selector = 0x28;
        _vmcb->state_save_area.tr.attrib = 0x8b;
        _vmcb->state_save_area.tr.limit = 0x67;
        _vmcb->state_save_area.tr.base = 0;

        _vmcb->state_save_area.g_pat = 0x0007040600010406ULL;

        // TODO: set correctly
        _vmcb->state_save_area.cpl = 0;
        _vmcb->state_save_area.efer = 0x1000; //sVm set
        //set the sycall enter/exit bit (by Divij)
        //_vmcb->state_save_area.efer |= 0x1;

        _vmcb->state_save_area.cr4 = 0x090;//690
        _vmcb->state_save_area.cr3 = 0;
        // PG[31] = 0, WP[16] = 1, NE[5]  = 1, ET[4]  = 1
        // TS[3]  = 1, MP[1]  = 1, PE[0]  = 1
        _vmcb->state_save_area.cr0 = 0x3b;//0x1003b;
        _vmcb->state_save_area.dr7 = 0;
        _vmcb->state_save_area.dr6 = 0;
        _vmcb->state_save_area.rflags = 0;

        // set instruction pointer to start of bzImage
        struct setup_header *lx_setup_header =
            (struct setup_header*)(GET_VM.mem().base_ptr() + BPO_HDR);
        //possible that code_32 is not at the right position
        _vmcb->state_save_area.rip = lx_setup_header->code32_start;
        //printf("set entry point to %p (size=%d)\n",
        // _vmcb->state_save_area.rip, sizeof(_vmcb->state_save_area.rip));
        karma_log(INFO, "set entry point to %08llx (size=%d)\n",
                _vmcb->state_save_area.rip, sizeof(_vmcb->state_save_area.rip));
        // TODO: set stack pointer
        _vmcb->state_save_area.rsp = 0x9a000;


        // now setup all the things we want to intercept
        //    _vmcb->control_area.intercept_rd_crX = 0;
        //    _vmcb->control_area.intercept_wr_crX = 0;

        // intercept accesses to cr0, cr3, cr4
        // running with shadow page tables
        if(params.vtlb)
        {
//            _vmcb->control_area.intercept_rd_crX = 0x19;
//            _vmcb->control_area.intercept_wr_crX = 0x19;
        }
        else
        {
            _vmcb->control_area.intercept_rd_crX = 0;
            _vmcb->control_area.intercept_wr_crX = 0;
        }

        _vmcb->control_area.intercept_rd_drX = 0;
        _vmcb->control_area.intercept_wr_drX = 0;
        // intercept page fault...
        _vmcb->control_area.intercept_exceptions = 0;
        // intercept all idt writes
        _vmcb->control_area.intercept_instruction0 = 0xd944001f; // enabled correct irq injection behaviour
        //_vmcb->control_area.intercept_instruction0 = 0xf944000f;//0x810f;
        _vmcb->control_area.intercept_instruction1 = ~0;//0xfff;

        if(params.vtlb)
        {
//            //intercept INVLPG/A and CR0 intercept(exit code 0x65) intructions
//            _vmcb->control_area.intercept_instruction0 |= 0x6000020;
        }

    }
};

template <>
class VMCX<guest::Linux, arch::x86, arch::x86::vmx>
{
public:
    typedef Vmcs type;

    static void init(void * vmcs)
    {
        Vmcs * _vmcs = reinterpret_cast<Vmcs*>(vmcs);
        if(!params.vtlb)
        {
            karma_log(ERROR, "Error. Karma on Intel-VT requires --vtlb\n");
            exit(1);
        }

        _vmcs->vmwrite<VMX_PF_ERROR_CODE_MASK>((l4_uint32_t)0);
        _vmcs->vmwrite<VMX_PF_ERROR_CODE_MATCH>((l4_uint32_t)0);

         // set external interupt exiting, MNI exiting and Virtual NMI's
        _vmcs->vmwrite<VMX_PIN_EXEC_CTRL>((l4_uint32_t)(0x29 | 0x16));
        //    //pins 1,2 and 4 are reserved and must be set to 1

         //exits on hlt, invlpg, r/w of CR3, r/w of DR
        _vmcs->vmwrite<VMX_PRIMARY_EXEC_CTRL>((l4_uint32_t)(0x818280 | 0x401e172 | 0x400));
    //    // bits 1, 4-6, 8, 13-16 and 26 need to be set to 1

        _vmcs->vmwrite<VMX_SECOND_EXEC_CTRL>((l4_uint32_t)0);
        _vmcs->vmwrite<VMX_EXCEPTION_BITMAP>((l4_uint32_t)0x6140);
        _vmcs->vmwrite<VMX_CR3_TARGET_COUNT>((l4_uint32_t)0);
        _vmcs->vmwrite<VMX_CR0_MASK>((l4_umword_t)~0);
        _vmcs->vmwrite<VMX_CR4_MASK>((l4_umword_t)~0);
        _vmcs->vmwrite<VMX_VMCS_LINK_PTR>(~(0ULL));

         // set bits 0-11 and 13-17
        _vmcs->vmwrite<VMX_EXIT_CTRL>((l4_uint32_t)0x36dff);
        _vmcs->vmwrite<VMX_EXIT_MSR_STORE_COUNT>((l4_uint32_t)0);
        _vmcs->vmwrite<VMX_EXIT_MSR_LOAD_COUNT>((l4_uint32_t)0);

         //set bits 0-8 and 12
        _vmcs->vmwrite<VMX_ENTRY_CTRL>((l4_uint32_t)0x11ff);
        _vmcs->vmwrite<VMX_ENTRY_MSR_LOAD_COUNT>((l4_uint32_t)0);

         //bits 4 to 11, 13 and 15 to 63 are reserved and must be set to 0
        _vmcs->vmwrite<VMX_GUEST_PENDING_DEBUG_EXCEPTIONS>((l4_umword_t)0);
         //bits 4 to 31 are reserved and must be set to 0
        _vmcs->vmwrite<VMX_GUEST_INTERRUPTIBILITY_STATE>((l4_uint32_t)0);

         // according to Documentation/x86/boot.txt
         // CS = __BOOT_CS,
         // DS = ES = SS = __BOOT_DS
        _vmcs->vmwrite<VMX_GUEST_CS_SEL>((l4_uint16_t)0x8);
        _vmcs->vmwrite<VMX_GUEST_CS_ACCESS_RIGHTS>((l4_uint32_t)0xd09b);
        _vmcs->vmwrite<VMX_GUEST_CS_LIMIT>((l4_uint32_t)~0);
        _vmcs->vmwrite<VMX_GUEST_CS_BASE>((l4_umword_t)0);

        _vmcs->vmwrite<VMX_GUEST_SS_SEL>((l4_uint16_t)0x10);
        _vmcs->vmwrite<VMX_GUEST_SS_ACCESS_RIGHTS>((l4_uint32_t)0xc093);
        _vmcs->vmwrite<VMX_GUEST_SS_LIMIT>((l4_uint32_t)~0);
        _vmcs->vmwrite<VMX_GUEST_SS_BASE>((l4_umword_t)0);

        _vmcs->vmwrite<VMX_GUEST_DS_SEL>((l4_uint16_t)0x20);
        _vmcs->vmwrite<VMX_GUEST_DS_ACCESS_RIGHTS>((l4_uint32_t)0xc0f3);
        _vmcs->vmwrite<VMX_GUEST_DS_LIMIT>((l4_uint32_t)~0);
        _vmcs->vmwrite<VMX_GUEST_DS_BASE>((l4_umword_t)0);

        _vmcs->vmwrite<VMX_GUEST_ES_SEL>((l4_uint16_t)0x0);
        _vmcs->vmwrite<VMX_GUEST_ES_ACCESS_RIGHTS>((l4_uint32_t)0x14003);
        _vmcs->vmwrite<VMX_GUEST_ES_LIMIT>((l4_uint32_t)~0);
        _vmcs->vmwrite<VMX_GUEST_ES_BASE>((l4_umword_t)0);

        _vmcs->vmwrite<VMX_GUEST_FS_SEL>((l4_uint16_t)0x0);
        _vmcs->vmwrite<VMX_GUEST_FS_ACCESS_RIGHTS>((l4_uint32_t)0x1c0f3);
        _vmcs->vmwrite<VMX_GUEST_FS_LIMIT>((l4_uint32_t)~0);
        _vmcs->vmwrite<VMX_GUEST_FS_BASE>((l4_umword_t)0);

        _vmcs->vmwrite<VMX_GUEST_GS_SEL>((l4_uint16_t)0x0);
        _vmcs->vmwrite<VMX_GUEST_GS_ACCESS_RIGHTS>((l4_uint32_t)0x1c0f3);
        _vmcs->vmwrite<VMX_GUEST_GS_LIMIT>((l4_uint32_t)~0);
        _vmcs->vmwrite<VMX_GUEST_GS_BASE>((l4_umword_t)0);

        _vmcs->vmwrite<VMX_GUEST_GDTR_LIMIT>((l4_uint32_t)0x3f);
        _vmcs->vmwrite<VMX_GUEST_GDTR_BASE>((l4_umword_t)GET_VM.os().get_gdt());

        _vmcs->vmwrite<VMX_GUEST_LDTR_SEL>((l4_uint16_t)0x0);
        _vmcs->vmwrite<VMX_GUEST_LDTR_ACCESS_RIGHTS>((l4_uint32_t)0x5082);
        _vmcs->vmwrite<VMX_GUEST_LDTR_LIMIT>((l4_uint32_t)0xff);
        _vmcs->vmwrite<VMX_GUEST_LDTR_BASE>((l4_umword_t)GET_VM.os().get_idt());

        _vmcs->vmwrite<VMX_GUEST_IDTR_LIMIT>((l4_uint32_t)0xff);
        _vmcs->vmwrite<VMX_GUEST_IDTR_BASE>((l4_umword_t)GET_VM.os().get_idt());

        _vmcs->vmwrite<VMX_GUEST_TR_SEL>((l4_uint16_t)0x28);
        _vmcs->vmwrite<VMX_GUEST_TR_ACCESS_RIGHTS>((l4_uint32_t)0x508b);
        _vmcs->vmwrite<VMX_GUEST_TR_LIMIT>((l4_uint32_t)0x67);
        _vmcs->vmwrite<VMX_GUEST_TR_BASE>((l4_umword_t)0);

        _vmcs->vmwrite<VMX_GUEST_IA32_PAT>(0x7040600010406ULL);

         // TODO: set correctly
         //_vmcs->guest_area.cpl = 0;

        //vmx set
         //set the sycall enter/exit bit (by Divij)
        _vmcs->vmwrite<VMX_GUEST_IA32_EFER>((l4_uint64_t)0x1001);

        l4_umword_t eflags;
        asm volatile("pushf     \n"
                "popl %0   \n"
                : "=r" (eflags));

        // clear interrupt
        eflags = (eflags & 0xfffffdff);
        eflags &= ~(0x1 << 17);

        _vmcs->vmwrite<VMX_GUEST_RFLAGS>(eflags);
        _vmcs->vmwrite<VMX_GUEST_CR0>((l4_umword_t)0x8001003b);
        _vmcs->vmwrite<VMX_GUEST_CR4>((l4_umword_t)0x2690);
        _vmcs->vmwrite<VMX_GUEST_DR7>((l4_umword_t)0x300);

        // set instruction pointer to start of bzImage
        struct setup_header *lx_setup_header =
            (struct setup_header*)(GET_VM.mem().base_ptr() + BPO_HDR);
        //possible that code_32 is not at the right position
        _vmcs->vmwrite<VMX_GUEST_RIP>((l4_umword_t)(lx_setup_header->code32_start));

        karma_log(INFO, "set entry point to %08lx\n",
               (unsigned long)_vmcs->vmread<VMX_GUEST_RIP>());

        _vmcs->vmwrite<VMX_GUEST_RSP>((l4_umword_t)0x9a000);

    }
};

} // namespace guest

#endif /* GUEST_LINUX_HPP_ */
