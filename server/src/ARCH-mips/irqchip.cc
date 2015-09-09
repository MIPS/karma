/*
 * Derived from linux arch/mips/kvm/kvm_mipsvz.c
 *
 * Copyright (C) 2015 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * This file incorporates work covered by the following copyright notice:
 */

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2012-2013 Cavium, Inc.
 */

#include <l4/sys/types.h>
#include <l4/re/mem_alloc>
#include <l4/re/env>
#include <l4/re/dataspace>
#include <l4/re/util/cap_alloc>
#include <l4/cxx/minmax>
#include <pthread.h>

#include "irqchip.h"
#include "vmm.h"
#include "../l4_vm.h"
#include "../util/debug.h"

#include "inst.h"
#include "mipsvzregs.h"

typedef l4_uint8_t u8;
typedef l4_int8_t s8;
typedef l4_uint32_t u32;
typedef l4_uint64_t u64;

struct irq_level {
    u32 irq;
    u32 level;
};

struct mipsvz_szreg {
    u8 size;
    s8 reg; /* negative value indicates error */
    bool sign_extend;
};

struct mipsvz_irqchip {
    u32 num_irqs;
    u32 num_cpus;
    // - raw registers: size = words_per_reg = num_irqs/32
    // reg0: reg_raw                    (aka rw_reg_base)
    // reg1: reg_raw_w1s
    // reg2: reg_raw_w1c
    // - per cpu registers: size words_per_reg
    // reg3: cpu[num_cpus].reg_src      (aka cpu_first_reg)
    // reg4: cpu[num_cpus].reg_en
    // reg5: cpu[num_cpus].reg_en_w1s
    // reg6: cpu[num_cpus].reg_en_w1c
};

static struct mipsvz_irqchip *irq_chip;
static pthread_spinlock_t irq_chip_lock;

#if 0 // used for mipsvz_emulate_io()
static struct mipsvz_szreg mipsvz_get_load_params(u32 insn)
{
    struct mipsvz_szreg r;
    r.size = 0;
    r.reg = -1;
    r.sign_extend = false;

    if ((insn & 0x80000000) == 0)
        goto out;

    switch ((insn >> 26) & 0x1f) {
    case 0x00: /* LB */
        r.size = 1;
        r.sign_extend = true;
        break;
    case 0x04: /* LBU */
        r.size = 1;
        break;
    case 0x01: /* LH */
        r.size = 2;
        r.sign_extend = true;
        break;
    case 0x05: /* LHU */
        r.size = 2;
        break;
    case 0x03: /* LW */
        r.size = 4;
        r.sign_extend = true;
        break;
    case 0x07: /* LWU */
        r.size = 4;
        break;
    case 0x17: /* LD */
        r.size = 8;
        break;
    default:
        goto out;
    }
    r.reg = (insn >> 16) & 0x1f;

out:
    return r;
}
#endif

static struct mipsvz_szreg mipsvz_get_store_params(u32 insn)
{
    struct mipsvz_szreg r;
    r.size = 0;
    r.reg = -1;
    r.sign_extend = false;

    if ((insn & 0x80000000) == 0)
        goto out;

    switch ((insn >> 26) & 0x1f) {
    case 0x08: /* SB */
        r.size = 1;
        break;
    case 0x09: /* SH */
        r.size = 2;
        break;
    case 0x0b: /* SW */
        r.size = 4;
        break;
    case 0x1f: /* SD */
        r.size = 8;
        break;
    default:
        goto out;
    }
    r.reg = (insn >> 16) & 0x1f;

out:
    return r;
}

int mipsvz_create_irqchip()
{
    int ret = 0;
    void *base = 0;
    unsigned long size = L4_PAGESIZE;
    L4::Cap<L4Re::Dataspace> irq_ds;

    // KYMAXXX TODO
    //mutex_lock(&kvm->lock);

    if (irq_chip) {
        ret = -L4_EEXIST;
        goto out;
    }

    irq_ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if(!irq_ds.is_valid())
    {
        karma_log(ERROR, "[Irq_chip] Error allocating capability\n");
        ret = -L4_ENOMEM;
        goto out;
    }

    if((ret = L4Re::Env::env()->mem_alloc()->alloc(size, irq_ds,
        L4Re::Mem_alloc::Continuous|L4Re::Mem_alloc::Pinned)))
    {
        karma_log(ERROR, "[Irq_chip] Error allocating enough memory\n");
        goto out;
    }

    if((ret = L4Re::Env::env()->rm()->attach(&base, size,
                                            L4Re::Rm::Search_addr,
                                            irq_ds,
                                            0,
                                            L4_PAGESHIFT)))
    {
        karma_log(ERROR, "[Irq_chip] Error attaching irqchip memory ds\n");
        goto out;
    }

    // map readonly into guest address space
    GET_VM.mem().map((l4_addr_t)0x1e010000, (l4_addr_t)base, size, 0, 0);

    karma_log(DEBUG,
            "[Irq_chip] attached irqchip memory address %p, size=%#lx\n",
            base, size);

    irq_chip = (struct mipsvz_irqchip*)base;
    irq_chip->num_irqs = 64;
    irq_chip->num_cpus = cxx::max(8U, MAX_CPUS);

    pthread_spin_init(&irq_chip_lock, 0);

out:
    // KYMAXXX TODO
    //mutex_unlock(&kvm->lock);

    if (ret)
        karma_log(ERROR, "[Irq_chip] Error creating irqchip: %s\n", l4sys_errtostr(ret));

    return ret;
}

static void mipsvz_write_irqchip_w1x(u32 *irqchip_regs, int words_per_reg,
                     unsigned int base, unsigned int offset,
                     u32 val, u32 mask)
{
    int type = (offset - base) / words_per_reg;
    int idx = (offset - base) % words_per_reg;

    if (type == 0)  /* Set */
        irqchip_regs[base + idx] = (irqchip_regs[base + idx] & ~mask) | (val & mask);
    else if (type == 1) /* W1S*/
        irqchip_regs[base + idx] |= (val & mask);
    else if (type == 2) /* W1C*/
        irqchip_regs[base + idx] &= ~(val & mask);

    /* Make the W1S and W1C reg have the same value as the base reg. */
    irqchip_regs[base + idx + 1 * words_per_reg] = irqchip_regs[base + idx];
    irqchip_regs[base + idx + 2 * words_per_reg] = irqchip_regs[base + idx];
}

static void mipsvz_write_irqchip_reg(u32 *irqchip_regs, unsigned int offset, u32 val, u32 mask)
{
    unsigned int numbits = irqchip_regs[0];
    int words_per_reg = numbits / 32;
    int reg, reg_offset;
    int rw_reg_base = 2;

    if (offset <= 1 || offset >= (irqchip_regs[1] * (words_per_reg + 1) * 4))
        return; /* ignore the write */

    reg = (offset - rw_reg_base) / words_per_reg;
    reg_offset = (offset - rw_reg_base) % words_per_reg;

    if (reg_offset == 0)
        mask &= ~0x1ffu; /* bits 8..0 are ignored */

    if (reg <= 2) { /* Raw */
        mipsvz_write_irqchip_w1x(irqchip_regs, words_per_reg, rw_reg_base, offset, val, mask);
    } else {
        /* Per CPU enables */
        int cpu_first_reg = rw_reg_base + 3 * words_per_reg;
        int cpu = (reg - 3) / 4;
        int cpu_reg = (reg - 3) % 4;

        if (cpu_reg != 0)
            mipsvz_write_irqchip_w1x(irqchip_regs, words_per_reg,
                         cpu_first_reg + words_per_reg + cpu * 4 * words_per_reg,
                         offset, val, mask);
    }
}

/* Returns a bit mask of vcpus where the */
static u32 mipsvz_write_irqchip_new_irqs(u32 *irqchip_regs)
{
    u32 r = 0;
    int rw_reg_base = 2;
    unsigned int numbits = irqchip_regs[0];
    int numcpus = irqchip_regs[1];
    int words_per_reg = numbits / 32;
    int cpu;
    L4_vm &vm = GET_VM;

    for (cpu = 0; cpu < numcpus; cpu++) {
        int cpu_base = rw_reg_base + (3 * words_per_reg) + (cpu * 4 * words_per_reg);
        int word;
        u32 combined = 0;
        for (word = 0; word < words_per_reg; word++) {
            /* SRC = EN & RAW */
            irqchip_regs[cpu_base + word] = irqchip_regs[cpu_base + words_per_reg + word] & irqchip_regs[rw_reg_base + word];
            combined |= irqchip_regs[cpu_base + word];
        }

        if (vm.valid_cpuid_index(cpu)) {
            u32 injected_ipx;
            l4_vm_vz_state_t *vmstate =
                reinterpret_cast<l4_vm_vz_state_t *>(vm.cpu(cpu).getExtState());
            u32 old_injected_ipx = vmstate->injected_ipx;

            if (combined)
                injected_ipx = CAUSEF_IP2;
            else
                injected_ipx = 0;

            if (injected_ipx != old_injected_ipx) {
                r |= 1 << cpu;
                vmstate->injected_ipx = injected_ipx;
                vmstate->set_injected_ipx = 1;
            }
        }
    }
    return r;
}

static void mipsvz_assert_irqs(u32 effected_cpus)
{
    int me;
    L4_vm &vm = GET_VM;

    if (!effected_cpus)
        return;

    // KYMAXXX TODO preempt disable
    //me = get_cpu();
    me = current_cpu().id();

    for (unsigned int i = 0; i < vm.nr_available_cpus(); ++i) {
        if (!((1 << vm.cpu(i).id()) & effected_cpus))
            continue;

        // injected_ipx will be applied when vcpu resumes
        if (me != vm.cpu(i).id())
            vm.cpu(i).vcpu().trip();
    }

    // KYMAXXX TODO preempt enable
    //put_cpu();
}

bool mipsvz_write_irqchip(vz_vmm_t *vmm, unsigned int write, l4_addr_t address)
{
    struct mipsvz_szreg szreg;
    u64 val;
    u64 mask;
    u32 *irqchip_regs;
    u32 insn = vmm->vmstate->exit_badinstr;
    int offset = address - 0x1e010000;
    u32 effected_cpus;

    if (!write || !irq_chip) {
        karma_log(ERROR, "Error: Read fault in irqchip\n");
        return false;
    }

    szreg = mipsvz_get_store_params(insn);
    if (szreg.reg < 0) {
        karma_log(ERROR, "Error: Bad insn on store emulation: %08x\n", insn);
        return false;
    }
    val = vmm_regs(vmm).r[szreg.reg];
    mask = ~0ul >> (64 - (szreg.size * 8));
    val &= mask;
    val <<= 8 * (offset & 7);
    mask <<= 8 * (offset & 7);

    irqchip_regs = reinterpret_cast<u32*>(irq_chip);

    // KYMAXXX TODO
    //mutex_lock(&kvm->lock);

    pthread_spin_lock(&irq_chip_lock);

    if (szreg.size == 8) {
        offset &= ~7;
        mipsvz_write_irqchip_reg(irqchip_regs, offset / 4 + 1,
                     (u32)(val >> 32), (u32)(mask >> 32));
        mipsvz_write_irqchip_reg(irqchip_regs, offset / 4 ,
                     (u32)val, (u32)mask);
    } else {
        if (offset & 4) {
            val >>= 32;
            mask >>= 32;
        }
        offset &= ~3;
        mipsvz_write_irqchip_reg(irqchip_regs, offset / 4 ,
                     (u32)val, (u32)mask);
    }

    effected_cpus = mipsvz_write_irqchip_new_irqs(irqchip_regs);

    pthread_spin_unlock(&irq_chip_lock);

    mipsvz_assert_irqs(effected_cpus);

    // KYMAXXX TODO
    //mutex_unlock(&kvm->lock);

    return true;
}

static int mipsvz_irq_line(struct irq_level &irq_level)
{
    u32 *irqchip_regs;
    u32 mask, val;
    unsigned int numbits;
    int i;
    u32 effected_cpus;
    int ret = 0;

    if (!irq_chip)
        return -L4_ENODEV;

    irqchip_regs = reinterpret_cast<u32*>(irq_chip);
    numbits = irqchip_regs[0];

    if (irq_level.irq < 9)
        goto out; /* Ignore */
    if (irq_level.irq >= numbits) {
        ret = -L4_EINVAL;
        goto out;
    }

    // KYMAXXX TODO
    //mutex_lock(&kvm->lock);

    mask = 1ull << (irq_level.irq % 32);
    i = irq_level.irq / 32;
    if (irq_level.level)
        val = mask;
    else
        val = 0;

    pthread_spin_lock(&irq_chip_lock);

    mipsvz_write_irqchip_reg(irqchip_regs, 2 + i, val, mask);
    effected_cpus = mipsvz_write_irqchip_new_irqs(irqchip_regs);

    pthread_spin_unlock(&irq_chip_lock);

    mipsvz_assert_irqs(effected_cpus);

    // KYMAXXX TODO
    //mutex_unlock(&kvm->lock);

out:
    return ret;
}

// irqchip_irq is based on arch/mips/invlude/asm/mach-paravirt/irq.h
#define MIPS_CPU_IRQ_BASE 1
#define MIPS_IRQ_PCIA (MIPS_CPU_IRQ_BASE + 8)
#define MIPS_IRQ_PCIB (MIPS_CPU_IRQ_BASE + 9)
#define MIPS_IRQ_PCIC (MIPS_CPU_IRQ_BASE + 10)
#define MIPS_IRQ_PCID (MIPS_CPU_IRQ_BASE + 11)
#define MIPS_IRQ_MBOX0 (MIPS_CPU_IRQ_BASE + 32)
#define MIPS_IRQ_MBOX1 (MIPS_CPU_IRQ_BASE + 33)

int mipsvz_karma_irq_line(enum karma_device_irqs karma_irq, unsigned int level)
{
    unsigned int irqchip_irq = karma_irq + MIPS_IRQ_PCID + 1;
    struct irq_level irq_level;
    int ret;

    if (irqchip_irq >= MIPS_IRQ_MBOX0) {
        ret = -L4_EINVAL;
        goto out;
    }

    irq_level.irq = irqchip_irq;
    irq_level.level = level;
    ret = mipsvz_irq_line(irq_level);

out:
    if (ret)
        karma_log(ERROR, "[Irq_chip] Error: failed to set karma_irq %d: %s\n",
                karma_irq, l4sys_errtostr(ret));

    return ret;
}
