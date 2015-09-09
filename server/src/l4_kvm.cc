/*
 * l4_kvm.cc - TODO enter description
 * 
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

//NEEDS TO BE PROPERLY ADAPTED FOR INTEL
//#define DEBUG_LEVEL TRACE
//#include "l4_kvm.h"
//
//#include <l4/re/env>
//#include <l4/re/namespace>
//#include <l4/re/util/cap_alloc>
//#include <l4/sys/vm>
//
//#include "l4_vm.h"
//
//#include "debug.h"
//
//L4_kvm::L4_kvm(L4::Cap<L4::Vm> vm_cap)
//  : _vm_cap(vm_cap) {
//    in_use=0;
//    counter =0;
//    _phys_intr =0;
//    _last_id = 0;
//    flushtlb = true;
//    osec = 0;
//    counter = 0;
//}
//
//l4_umword_t L4_kvm::read(l4_umword_t addr) {
//    counter++;
//    l4_msgtag_t msg;
//    switch(addr)
//    {
//        case L4_KVM_CREATE:
//            debug(TRACE, "L4_KVM_CREATE\n");
//            if(in_use)
//                return 1;
//            in_use=1;
//            _vm_cap = L4Re::Util::cap_alloc.alloc<L4::Vm>();
//            if(!_vm_cap.is_valid())
//            {
//                printf("L4_kvm: Error allocating vm cap.\n");
//                return 1;
//            }
//
//            msg = L4Re::Env::env()->factory()->create_vm(_vm_cap);
//            if(l4_error(msg))
//            {
//                printf("L4_kvm: Error getting vm cap.\n");
//                return 1;
//            }
//            return 0;
//            break;
//        case L4_KVM_DESTROY:
////            printf("L4_kvm: Deleting vm cap.\n");
////            delete _vm_cap;
//            L4Re::Env::env()->task()->unmap(
//                l4_obj_fpage(_vm_cap.cap(), 0, L4_FPAGE_RWX),
//                L4_FP_ALL_SPACES);
//            in_use = 0;
//            break;
//        default:
//            break;
//    }
//    return -1;
//}
//
//
//struct karma_vmrun_data {
//    l4_vm_gpregs_t gpregs;
//    unsigned vmcb;
//};
//
//struct karma_kvm_map {
//    unsigned long target_addr;
//    unsigned long fault_addr;
//    unsigned largepage;
//};
//
//void L4_kvm::write(l4_umword_t addr, l4_umword_t value) {
//    counter++;
//    struct karma_vmrun_data *kvd = 0;
//    struct karma_kvm_map *kkm = 0;
//    l4_vm_svm_vmcb_t *_vmcb = 0;
//    //struct l4_vm_vmx_vmcs *_vmcs = 0;
//	Vmcs *_vmcs = 0;
//    //XXX: make compiler happy
//    (void)_vmcs;
//    l4_addr_t from = 0, to = 0;
//    unsigned poweroftwo = 0;
//    l4_msgtag_t msg;
//    switch(addr)
//    {
//        case L4_KVM_VMRUN:
//            debug(TRACE, "VMRUN\n");
//            kvd = (struct karma_vmrun_data*)(GET_VM.mem().base_ptr() + value);
//            _vmcb = (l4_vm_svm_vmcb_t*)(kvd->vmcb -0xc0000000 + GET_VM.mem().base_ptr());
//            debug(TRACE, "kvd->vmcb = %x\n", kvd->vmcb);
//           //if(flushtlb)
////              _vmcb->control_area.guest_asid_tlb_ctl = (1ULL << 32);
//            // KVM may only use ASID 5
//            if(_vmcb->control_area.guest_asid_tlb_ctl & (1ULL << 32))
//                flushtlb = true;
//            else
//                flushtlb = false;
//            _vmcb->control_area.guest_asid_tlb_ctl = 5ULL;
//            _vmcb->state_save_area.efer = 0x1000;
//            if(flushtlb)
//                _vmcb->control_area.guest_asid_tlb_ctl |= (1ULL << 32);
//            debug(TRACE, "eip = 0x%08llx\n", _vmcb->state_save_area.rip);
//
//            memcpy(_vm_cap->gpregs(), &kvd->gpregs, sizeof(l4_vm_gpregs_t));
//
//            msg = _vm_cap->run(l4_fpage((unsigned long)_vmcb, 12,0), l4_utcb());
//            if(l4_error(msg))
//            {
//                printf("L4_kvm: Error running vm.\n");
//                break;
//            }
//
//            memcpy(&kvd->gpregs, _vm_cap->gpregs(), sizeof(l4_vm_gpregs_t));
//
//            printf("exit_code = %llx\n", _vmcb->control_area.exitcode);
//
//            if(_vmcb->control_area.exitcode == 0x60)
//                _phys_intr = 1;
////            if(_vmcb->control_area.exitcode == 0x62) //smc
////                goto vmrun;
//            if(_vmcb->control_area.exitcode == 0x81 && kvd->gpregs.ecx == 0x8) // print timestamp
//            {
//                GET_VM.print_time();
//                _vmcb->state_save_area.rip++;
//                _vmcb->control_area.exitcode = 0x60; // let kvm think it was a physical interrupt
//            }
//
//            break;
//        case L4_KVM_MAP:
//            debug(TRACE, "L4_KVM_MAP");
//            kkm = (struct karma_kvm_map*)(GET_VM.mem().base_ptr() + value);
//
//            from = (kkm->fault_addr + GET_VM.mem().base_ptr());
//            to = kkm->target_addr;
//            //kkm->largepage=1;
//            //printf("L4_kvm: MAP from %lx to %lx (largepage=%d)\n",
//            //    from,
//            //    to,
//            //    kkm->largepage);
//            if(kkm->largepage)
//            {
//                poweroftwo = 22; // superpage, 4Mb
//                from &= ~(0x3fffff);
//                to &= ~(0x3fffff);
//            }
//            else
//            {
//                poweroftwo = 12; // page, 4Kb
//                from &= ~(0xfff);
//                to &= ~(0xfff);
//            }
//            msg = _vm_cap->map(L4Re::Env::env()->task(),
//                    l4_fpage(from,
//                        poweroftwo,
//                        L4_FPAGE_RW),
//                        to);
//            if(l4_error(msg))
//            {
//                printf("L4_kvm: Error mapping memory.\n");
//            }
//            break;
//        case L4_KVM_UNMAP:
//            debug(TRACE, "L4_kvm: unmap %lx\n", value);
//            msg = _vm_cap->unmap(
//                l4_fpage((l4_addr_t)value,
//                12,
//                L4_FPAGE_RWX),
//                L4_FP_ALL_SPACES);
//            if(l4_error(msg))
//                printf("L4_kvm: Error unmapping memory.\n");
//            break;
//        default:
//            break;
//    }
//}

