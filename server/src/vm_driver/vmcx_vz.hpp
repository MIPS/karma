/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#ifndef VMCX_VZ_HPP_
#define VMCX_VZ_HPP_

#include <string.h>
#include <l4/sys/vm.h>
#include "../l4_cpu.h"
#include "../l4_os.h"
#include "vmm.h"
#include "../util/debug.h"
#include "mipsregs.h"

class VMCX {
public:
    typedef l4_vm_vz_vmcb_t type;

    inline static l4_vm_vz_vmcb_t * create(unsigned id)
    {
        L4_vm &vm = GET_VM;
        L4_vcpu &vcpu = current_vcpu();
        l4_vm_vz_vmcb_t* vmm = new l4_vm_vz_vmcb_t;

        memset(vmm, 0, sizeof(l4_vm_vz_vmcb_t));
        vmm->id = id;
        vmm->vmstate = reinterpret_cast<l4_vm_vz_state_t *>(vcpu.getExtState());
        vmm->vcpu = vcpu.getVcpuState();
        vmm->vmtask = vm.vm_cap().cap();
        vmm->vm_guest_mem_size = vm.mem().get_mem_size();
        vmm->vm_guest_entrypoint = vm.os().get_guest_entrypoint();
        vmm->vm_guest_mapbase = 0;
        vmm->host_addr_mapbase = vm.mem().base_ptr();

        if (vz_init(vmm)) {
            karma_log(ERROR, "vz_init failed\n");
            return (l4_vm_vz_vmcb_t*)NULL;
        }

        vm.os().guest_os_init(vmm);

        return vmm;
    }

    inline static bool hlt_condition(l4_vm_vz_vmcb_t & vmcb){
        (void)vmcb;
        //l4_umword_t cause = vmm_regs(&vmcb).cause;
        if (!GET_VM.gic().pending() /*&& !(cause & CAUSEF_TI)*/)
          return true;
        else
          return false; // KYMA TODO implement hlt_condition
    }
    inline static void set_ip_and_sp(l4_vm_vz_vmcb_t & vmcb, const l4_umword_t ip, const l4_umword_t sp) {
        vmm_regs(&vmcb).ip = ip;
        vmm_regs(&vmcb).sp = sp;
    }

    inline static l4_umword_t current_ip(l4_vm_vz_vmcb_t & vmcb) {
        return vmm_regs(&vmcb).ip;
    }
    inline static l4_umword_t exit_code(l4_vm_vz_vmcb_t & vmcb) {
        return vmcb.vmstate->exit_reason;
    }
};

#endif /* VMCX_VZ_HPP_ */

