/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#include <l4/sys/types.h>
#include <l4/sys/cp0_op.h>
#include <l4/re/c/rm.h>
#include <stdio.h>

#include "inst.h"
#include "mipsregs.h"
#include "mipsvzregs.h"

#include "vmm.h"
#include "irqchip.h"
#include "hypcall_ops.h"
#include "../l4_vm.h"
#include "../util/debug.h"

/*
 * Trace logging and statistic
 */
static struct vz_exit_stats vz_exit_stats;

/*
 * Emulation
 */
#define VZ_INVALID_INST         0xdeadbeef

#define COP0MT  0xffe007f8
#define MTC0    0x40800000
#define COP0MF  0xffe007f8
#define MFC0    0x40000000
#define RT      0x001f0000
#define RD      0x0000f800
#define SEL     0x00000007

#define VZ_TRACE_EXIT(vmm, e) vz_trace_exit((vmm), (e), #e)

/*
 * Prototypes
 */
void vz_trace_exit(vz_vmm_t *vmm, enum vz_exit_type_e e, const char* e_str);
void dump_vz_exit_stats(void);
static inline l4_umword_t get_insn_pc(vz_vmm_t *vmm);
l4_umword_t get_badinstr(vz_vmm_t *vmm);
l4_umword_t vz_compute_return_epc(vz_vmm_t *vmm, union mips_instruction insn);
enum emulation_result vz_update_pc(vz_vmm_t *vmm);
enum emulation_result vz_trap_handle_cache(vz_vmm_t *vmm, union mips_instruction insn);
enum emulation_result vz_trap_handle_mtc0(vz_vmm_t *vmm, union mips_instruction insn);
enum emulation_result vz_trap_handle_mfc0(vz_vmm_t *vmm, union mips_instruction insn);
enum emulation_result vz_trap_handle_gpsi(vz_vmm_t *vmm);
void vz_trap_handle_gsfc_c0_status(vz_vmm_t *vmm, int *val);
enum emulation_result vz_trap_handle_gsfc(vz_vmm_t *vmm);
enum emulation_result vz_trap_handle_hypcall(vz_vmm_t *vmm);
enum emulation_result vz_trap_default_handler(vz_vmm_t *vmm);
enum emulation_result vz_trap_handle_ge(vz_vmm_t *vmm);
enum emulation_result vz_tlb_handler(vz_vmm_t *vmm);
int vz_init(vz_vmm_t *vmm);

void vz_trace_exit(vz_vmm_t *vmm, enum vz_exit_type_e e, const char* e_str)
{
  if (LOG_LEVEL(INFO)) {
    ++vz_exit_stats.s[e].cnt;
    vz_exit_stats.s[e].str = e_str;
    karma_log(TRACE, "[%#08lx] VZ EXIT %s count:%ld\n",
            vmm_regs(vmm).ip, e_str, vz_exit_stats.s[e].cnt);
  }
}

void dump_vz_exit_stats(void)
{
  for (int e = 0; e < MAX_MIPS_EXIT_TYPES; ++e) {
    if (vz_exit_stats.s[e].str)
      printf("VZ EXIT %s count:%ld\n", vz_exit_stats.s[e].str, vz_exit_stats.s[e].cnt);
  }
}

// NOTE: does not handle 16bit ISA modes
static inline l4_umword_t get_insn_pc(vz_vmm_t *vmm)
{
  if (vmm_regs(vmm).cause & CAUSEF_BD)
    return vmm_regs(vmm).ip + 4;
  else
    return vmm_regs(vmm).ip;
}

/*
 *  Fetch the bad instruction register
 *  This code assumes that badinstr and badinstrp are supported.
 */
l4_umword_t get_badinstr(vz_vmm_t *vmm)
{
  static int init_done = 0;
  static unsigned set_bad_instr[Exc_code_LAST];
  l4_umword_t exccode = ((vmm_regs(vmm).cause & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE);

  if (!init_done) {
    set_bad_instr[Exc_code_Int]       = 0,      // Interrupt
    set_bad_instr[Exc_code_Mod]       = 1,      // TLB modification exception
    set_bad_instr[Exc_code_TLBL]      = 1,      // TLB exception (load or fetch)
    set_bad_instr[Exc_code_TLBS]      = 1,      // TLB exception (store)
    set_bad_instr[Exc_code_AdEL]      = 1,      // Address error exception (load or fetch)
    set_bad_instr[Exc_code_AdES]      = 1,      // Address error exception (store)
    set_bad_instr[Exc_code_IBE]       = 0,      // Bus error exception (load or fetch)
    set_bad_instr[Exc_code_DBE]       = 0,      // Bus error exception (store)
    set_bad_instr[Exc_code_Sys]       = 1,      // Syscall exception
    set_bad_instr[Exc_code_Bp]        = 1,      // Breakpoint exception
    set_bad_instr[Exc_code_RI]        = 1,      // Reserved instruction exception
    set_bad_instr[Exc_code_CpU]       = 1,      // Coprocessor unusable exception
    set_bad_instr[Exc_code_Ov]        = 1,      // Arithmetic overflow exception
    set_bad_instr[Exc_code_Tr]        = 1,      // Trap
    set_bad_instr[Exc_code_Res1]      = 0,      // (reserved)
    set_bad_instr[Exc_code_FPE]       = 1,      // Floating point exception
    set_bad_instr[Exc_code_Impl1]     = 0,      // (implementation-dependent 1)
    set_bad_instr[Exc_code_Impl2]     = 0,      // (implementation-dependent 2)
    set_bad_instr[Exc_code_C2E]       = 1,      // (reserved for precise coprocessor 2)
    set_bad_instr[Exc_code_TLBRI]     = 1,      // TLB exception (read)
    set_bad_instr[Exc_code_TLBXI]     = 1,      // TLB exception (execute)
    set_bad_instr[Exc_code_Res2]      = 0,      // (reserved)
    set_bad_instr[Exc_code_MDMX]      = 0,      // (MIPS64 MDMX unusable)
    set_bad_instr[Exc_code_WATCH]     = 0,      // Reference to watchHi/watchLo address
    set_bad_instr[Exc_code_MCheck]    = 0,      // Machine check exception
    set_bad_instr[Exc_code_Thread]    = 0,      // Thread exception
    set_bad_instr[Exc_code_DSPDis]    = 0,      // DSP disabled exception
    set_bad_instr[Exc_code_GE]        = 1,      // Guest Exit exception
    set_bad_instr[Exc_code_Res4]      = 0,      // (reserved)
    set_bad_instr[Exc_code_Prot]      = 0,      // Protection exception
    set_bad_instr[Exc_code_CacheErr]  = 0,      // Cache error exception
    set_bad_instr[Exc_code_Res6]      = 0,      // (reserved)
    init_done = 1;
  };

  unsigned bad_instr_valid = set_bad_instr[exccode];

  if (exccode == Exc_code_GE) {
    switch (vmm->vmstate->exit_gexccode) {
      case GUESTCTL0_GEXC_GPSI:
      case GUESTCTL0_GEXC_GSFC:
      case GUESTCTL0_GEXC_HC:
      case GUESTCTL0_GEXC_GRR:
        bad_instr_valid = 1;
        break;
      case GUESTCTL0_GEXC_GVA:
      case GUESTCTL0_GEXC_GHFC:
      case GUESTCTL0_GEXC_GPA:
      default:
        bad_instr_valid = 0;
        break;
    }
  }

  if (bad_instr_valid)
    return vmm->vmstate->exit_badinstr;
  else
    return VZ_INVALID_INST;
}

/*
 * Compute the return address and do emulate branch simulation, if required.
 * This function should be called only in branch delay slot active.
 */
l4_umword_t vz_compute_return_epc(vz_vmm_t *vmm, union mips_instruction insn)
{
#if 1 // KYMA force cpu_has_dsp false for guest
  const int cpu_has_dsp = 0;
#endif
  unsigned int dspcontrol;
  l4_umword_t *gprs = vmm_regs(vmm).r;
  l4_umword_t epc = vmm_regs(vmm).ip;
  l4_umword_t nextpc = VZ_INVALID_INST;

  if (epc & 3)
    goto unaligned;

  /*
   * Read the instruction
   */
  switch (insn.i_format.opcode) {
    /*
     * jr and jalr are in r_format format.
     */
  case spec_op:
    switch (insn.r_format.func) {
    case jalr_op:
      gprs[insn.r_format.rd] = epc + 8;
      /* Fall through */
    case jr_op:
      nextpc = gprs[insn.r_format.rs];
      break;
    }
    break;

    /*
     * This group contains:
     * bltz_op, bgez_op, bltzl_op, bgezl_op,
     * bltzal_op, bgezal_op, bltzall_op, bgezall_op.
     */
  case bcond_op:
    switch (insn.i_format.rt) {
    case bltz_op:
    case bltzl_op:
      if ((long)gprs[insn.i_format.rs] < 0)
        epc = epc + 4 + (insn.i_format.simmediate << 2);
      else
        epc += 8;
      nextpc = epc;
      break;

    case bgez_op:
    case bgezl_op:
      if ((long)gprs[insn.i_format.rs] >= 0)
        epc = epc + 4 + (insn.i_format.simmediate << 2);
      else
        epc += 8;
      nextpc = epc;
      break;

    case bltzal_op:
    case bltzall_op:
      gprs[31] = epc + 8;
      if ((long)gprs[insn.i_format.rs] < 0)
        epc = epc + 4 + (insn.i_format.simmediate << 2);
      else
        epc += 8;
      nextpc = epc;
      break;

    case bgezal_op:
    case bgezall_op:
      gprs[31] = epc + 8;
      if ((long)gprs[insn.i_format.rs] >= 0)
        epc = epc + 4 + (insn.i_format.simmediate << 2);
      else
        epc += 8;
      nextpc = epc;
      break;
    case bposge32_op:
      if (!cpu_has_dsp)
        goto sigill;

      dspcontrol = rddsp(0x01);

      if (dspcontrol >= 32) {
        epc = epc + 4 + (insn.i_format.simmediate << 2);
      } else
        epc += 8;
      nextpc = epc;
      break;
    }
    break;

    /*
     * These are unconditional and in j_format.
     */
  case jal_op:
    gprs[31] = epc + 8;
  case j_op:
    epc += 4;
    epc >>= 28;
    epc <<= 28;
    epc |= (insn.j_format.target << 2);
    nextpc = epc;
    break;

    /*
     * These are conditional and in i_format.
     */
  case beq_op:
  case beql_op:
    if (gprs[insn.i_format.rs] ==
      gprs[insn.i_format.rt])
      epc = epc + 4 + (insn.i_format.simmediate << 2);
    else
      epc += 8;
    nextpc = epc;
    break;

  case bne_op:
  case bnel_op:
    if (gprs[insn.i_format.rs] !=
      gprs[insn.i_format.rt])
      epc = epc + 4 + (insn.i_format.simmediate << 2);
    else
      epc += 8;
    nextpc = epc;
    break;

  case blez_op:     /* not really i_format */
  case blezl_op:
    /* rt field assumed to be zero */
    if ((long)gprs[insn.i_format.rs] <= 0)
      epc = epc + 4 + (insn.i_format.simmediate << 2);
    else
      epc += 8;
    nextpc = epc;
    break;

  case bgtz_op:
  case bgtzl_op:
    /* rt field assumed to be zero */
    if ((long)gprs[insn.i_format.rs] > 0)
      epc = epc + 4 + (insn.i_format.simmediate << 2);
    else
      epc += 8;
    nextpc = epc;
    break;

    /*
     * And now the FPA/cp1 branch instructions.
     */
  case cop1_op:
    karma_log(WARN, "%s: unsupported cop1_op\n", __func__);
    break;
  }

  return nextpc;

unaligned:
  karma_log(WARN, "%s: unaligned epc\n", __func__);
  return nextpc;

sigill:
  karma_log(WARN, "%s: DSP branch but not DSP ASE\n", __func__);
  return nextpc;
}

enum emulation_result vz_update_pc(vz_vmm_t *vmm)
{
  l4_umword_t branch_pc;
  enum emulation_result er = EMULATE_DONE;

  if (vmm_regs(vmm).cause & CAUSEF_BD) {
    union mips_instruction insn;
    insn.word = vmm->vmstate->exit_badinstrp;
    branch_pc = vz_compute_return_epc(vmm, insn);
    if (branch_pc == VZ_INVALID_INST) {
      er = EMULATE_FAIL;
    } else {
      vmm_regs(vmm).ip = branch_pc;
      karma_log(TRACE, "BD vz_update_pc(): New PC: %#lx\n", vmm_regs(vmm).ip);
    }
  } else
    vmm_regs(vmm).ip += 4;

  karma_log(TRACE, "vz_update_pc(): New PC: %#lx\n", vmm_regs(vmm).ip);

  return er;
}

#define MIPS_CACHE_OP_INDEX_INV         0x0

enum emulation_result
vz_trap_handle_cache(vz_vmm_t *vmm, union mips_instruction insn)
{
  enum emulation_result er;
  int offset, cache, op, base;

  base = insn.c_format.rs;
  offset = insn.c_format.simmediate;
  cache = insn.c_format.cache;
  op = insn.c_format.c_op;

  karma_log(WARN,
       "[%#lx/%#lx] VZ unsupported CACHE OP (cache: %#x, op: %#x, base[%d]: %#lx, offset: %#x\n",
       get_insn_pc(vmm), vmm_regs(vmm).r[31], cache, op, base,
       vmm_regs(vmm).r[base], offset);
  er = EMULATE_FAIL;

  if (op == MIPS_CACHE_OP_INDEX_INV) {
    /* INDEX_INV is issued by Linux on startup to invalidate the caches entirely
     * by stepping through all the ways/indexes.  INDEX_INV will not cause GPSI
     * when GuestCtl0Ext.CGI is set.
     */
    karma_log(WARN, "VZ unsupported CACHE INDEX INV operation, verify that GuestCtl0Ext.CGI=1.\n");
  }

  return er;
}

enum emulation_result
vz_trap_handle_mtc0(vz_vmm_t *vmm, union mips_instruction insn)
{
  int rt = insn.c0m_format.rt;
  int rd = insn.c0m_format.rd;
  int sel = insn.c0m_format.sel;

  if ((rd == MIPS_CP0_DATA_LO) && (sel == 3)) { // DDataLo
    vmm->vmstate->gcp0[MIPS_CP0_DATA_LO][3] = vmm_regs(vmm).r[rt];
  }
  else if ((rd == MIPS_CP0_COMPARE) && (sel == 0)) {
    karma_log(DEBUG, "[%#08lx] MTC0[%d][%d] : COMPARE %#08lx <= %#08lx\n",
        get_insn_pc(vmm), rd, sel, vmm->vmstate->gcp0[MIPS_CP0_COMPARE][0],
        vmm_regs(vmm).r[rt]);
    vmm->vmstate->gcp0[MIPS_CP0_COMPARE][0] = vmm_regs(vmm).r[rt];
  }
  else if ((rd == MIPS_CP0_CONFIG) && (sel == 6)) {
    // ignore mtc0 access
  }
  else if ((rd == MIPS_CP0_CONFIG) && (sel == 7)) {
    // ignore mtc0 access
  }
  else if ((rd == MIPS_CP0_ERRCTL) && (sel == 0)) {
    // ignore mtc0 access
  }
  else {
    karma_log(WARN, "[%#08lx] GPSI unhandled MTC0[%d][%d]\n", get_insn_pc(vmm), rd, sel);
    goto not_handled;
  }
  return EMULATE_DONE;

not_handled:
  return EMULATE_FAIL;
}

enum emulation_result
vz_trap_handle_mfc0(vz_vmm_t *vmm, union mips_instruction insn)
{
  int rt = insn.c0m_format.rt;
  int rd = insn.c0m_format.rd;
  int sel = insn.c0m_format.sel;

  if ((rd == MIPS_CP0_DATA_LO) && (sel == 3)) { // DDataLo
    vmm_regs(vmm).r[rt] = vmm->vmstate->gcp0[MIPS_CP0_DATA_LO][3];
  }
  else if ((rd == MIPS_CP0_COUNT) && (sel == 0)) {
    vmm_regs(vmm).r[rt] = vmm->vmstate->gcp0[MIPS_CP0_COUNT][0];
  }
  else if ((rd == MIPS_CP0_COMPARE) && (sel == 0)) {
    vmm_regs(vmm).r[rt] = vmm->vmstate->gcp0[MIPS_CP0_COMPARE][0];
  }
  else if ((rd == MIPS_CP0_SRSCTL) && (sel == MIPS_CP0_SRSCTL_SEL)) {
    vmm_regs(vmm).r[rt] = 0;
  }
  else if ((rd == MIPS_CP0_PRID) && (sel == MIPS_CP0_PRID_SEL)) {
    vmm_regs(vmm).r[rt] = vmm->vmstate->gcp0[MIPS_CP0_PRID][MIPS_CP0_PRID_SEL];
  }
  else if ((rd == MIPS_CP0_CONFIG) && (sel == 6)) {
    vmm_regs(vmm).r[rt] = 0;
  }
  else if ((rd == MIPS_CP0_CONFIG) && (sel == 7)) {
    vmm_regs(vmm).r[rt] = 0;
  }
  else if ((rd == MIPS_CP0_ERRCTL) && (sel == 0)) {
    vmm_regs(vmm).r[rt] = 0;
  }
  else {
    karma_log(WARN, "[%#08lx] GPSI unhandled MFC0[%d][%d]\n", get_insn_pc(vmm), rd, sel);
    goto not_handled;
  }

  karma_log(DEBUG, "[%#08lx] MFC0[%d][%d]\n", get_insn_pc(vmm), rd, sel);
  return EMULATE_DONE;

not_handled:
  return EMULATE_FAIL;
}

#define     rdpgpr_op        0x0a	/*  01010  */
#define     wrpgpr_op        0x0e	/*  01110  */

enum emulation_result
vz_trap_handle_gpsi(vz_vmm_t *vmm)
{
  enum emulation_result er = EMULATE_DONE;
  union mips_instruction insn;
  insn.word = get_badinstr(vmm);

  if (insn.c_format.opcode == cache_op) {
    VZ_TRACE_EXIT(vmm, hypervisor_gpsi_cache_exits);
    er = vz_trap_handle_cache(vmm, insn);
  }
  else if (insn.c0_format.opcode == cop0_op &&
          insn.c0_format.co == 1 &&
          insn.c0_format.func == wait_op) {
    VZ_TRACE_EXIT(vmm, hypervisor_gpsi_wait_exits);
#if 0
    er = EMULATE_WAIT;
#else
    l4_thread_yield();
    er = EMULATE_DONE;
#endif
  }
  else if (insn.c0m_format.opcode == cop0_op &&
          insn.c0m_format.func == mtc_op &&
          insn.c0m_format.code == 0) {
    VZ_TRACE_EXIT(vmm, hypervisor_gpsi_mtc0_exits);
    er = vz_trap_handle_mtc0(vmm, insn);
  }
  else if (insn.c0m_format.opcode == cop0_op &&
          insn.c0m_format.func == mfc_op &&
          insn.c0m_format.code == 0) {
    VZ_TRACE_EXIT(vmm, hypervisor_gpsi_mfc0_exits);
    er = vz_trap_handle_mfc0(vmm, insn);
  }
  else if (insn.c0m_format.opcode == cop0_op &&
          insn.c0m_format.func == rdpgpr_op &&
          insn.c0m_format.code == 0 &&
          insn.c0m_format.sel == 0) {
    vmm_regs(vmm).r[insn.c0m_format.rd] = vmm_regs(vmm).r[insn.c0m_format.rt];
  }
  else if (insn.c0m_format.opcode == cop0_op &&
          insn.c0m_format.func == wrpgpr_op &&
          insn.c0m_format.code == 0 &&
          insn.c0m_format.sel == 0) {
    vmm_regs(vmm).r[insn.c0m_format.rd] = vmm_regs(vmm).r[insn.c0m_format.rt];
  }
  else {
    karma_log(WARN, "[%#08lx] GPSI exception not supported: inst %#08lx\n",
            get_insn_pc(vmm), (l4_umword_t)insn.word);
    er = EMULATE_FAIL;
  }

  return er;
}

void
vz_trap_handle_gsfc_c0_status(vz_vmm_t *vmm, int *val)
{
  if (!(vmm->vmstate->host_cp0_guestctl0 & GUESTCTL0_SFC1) &&
          ((vmm->vmstate->gcp0[MIPS_CP0_STATUS][0] & ST0_CU1) != (*val & ST0_CU1)))
  {
    // allow guest to change Status.CU1 without root exception
    vmm->vmstate->host_cp0_guestctl0 |= GUESTCTL0_SFC1;
    vmm->vmstate->set_cp0_guestctl0 = 1;

    karma_log(INFO, "[%#08lx] Handle GSFC, Status.CU1 field change: %x => %x\n",
            get_insn_pc(vmm),
            (int)vmm->vmstate->gcp0[MIPS_CP0_STATUS][0] & ST0_CU1,
            *val & ST0_CU1);
  }

  if ((vmm->vmstate->gcp0[MIPS_CP0_STATUS][0] & ST0_FR) != (*val & ST0_FR))
  {
    // switch to Guest.Status.FR=1 not implemented
    *val &= ~ST0_FR;
    karma_log(INFO, "[%#08lx] Handle GSFC, Status.FR field change ignored: %x => %x\n",
            get_insn_pc(vmm),
            (int)vmm->vmstate->gcp0[MIPS_CP0_STATUS][0] & ST0_FR,
            *val & ST0_FR);
  }
}

enum emulation_result
vz_trap_handle_gsfc(vz_vmm_t *vmm)
{
  enum emulation_result er = EMULATE_DONE;
  union mips_instruction insn;
  insn.word = get_badinstr(vmm);

  /* complete MTC0 on behalf of guest */
  if ((insn.word & COP0MT) == MTC0) {
    int rt = insn.c0m_format.rt;
    int rd = insn.c0m_format.rd;
    int val = vmm_regs(vmm).r[rt];
    int sel = insn.c0m_format.sel;

    if ((rd == MIPS_CP0_STATUS) && (sel == 0)) {
      VZ_TRACE_EXIT(vmm, hypervisor_gsfc_cp0_status_exits);
      vz_trap_handle_gsfc_c0_status(vmm, &val);
      vmm->vmstate->gcp0[MIPS_CP0_STATUS][0] = val;
    }
    else if ((rd == MIPS_CP0_CAUSE) && (sel == 0)) {
      VZ_TRACE_EXIT(vmm, hypervisor_gsfc_cp0_cause_exits);
      vmm->vmstate->gcp0[MIPS_CP0_CAUSE][0] = val;
    }
    else if ((rd == MIPS_CP0_INTCTL) && (sel == MIPS_CP0_INTCTL_SEL)) {
      VZ_TRACE_EXIT(vmm, hypervisor_gsfc_cp0_intctl_exits);
      vmm->vmstate->gcp0[MIPS_CP0_INTCTL][MIPS_CP0_INTCTL_SEL] = val;
    }
    else {
      karma_log(WARN, "[%#08lx] Handle GSFC, unsupported field change: inst %#08lx\n",
              get_insn_pc(vmm), (l4_umword_t)insn.word);
      er = EMULATE_FAIL;
    }
  } else {
    karma_log(WARN, "[%#08lx] Handle GSFC, unrecognized instruction: inst %#08lx\n",
            get_insn_pc(vmm), (l4_umword_t)insn.word);
    er = EMULATE_FAIL;
  }

  return er;
}

enum emulation_result
vz_trap_handle_hypcall(vz_vmm_t *vmm)
{
  enum emulation_result er = EMULATE_DONE;
  union mips_instruction insn;
  insn.word = get_badinstr(vmm);
  l4_umword_t hypcall_code = (insn.word >> 11) & 0x3ff;

  karma_log(TRACE, "[%#08lx] Handle HYPCALL code %lx\n", get_insn_pc(vmm), hypcall_code);

  if (hypcall_code == HYPCALL_MAGIC_EXIT) {
    karma_log(INFO, "VM EXIT CODE!!!!\n");
    er = EMULATE_EXIT;
  } else if (hypcall_code == HYPCALL_KARMA_DEV_OP) {
    er = EMULATE_HYPCALL;
  }

  return er;
}

enum emulation_result
vz_trap_default_handler(vz_vmm_t *vmm)
{
  l4_umword_t gexccode = vmm->vmstate->exit_gexccode;

  switch (vmm->vmstate->exit_gexccode) {
    case GUESTCTL0_GEXC_GPSI:
    case GUESTCTL0_GEXC_GSFC:
    case GUESTCTL0_GEXC_HC:
    case GUESTCTL0_GEXC_GRR:

      union mips_instruction insn;
      insn.word = get_badinstr(vmm);

      karma_log(WARN, 
        "[%#08lx] Guest Exception Code %lx:%s not yet handled: "
        "inst %#08lx Guest Status %#08lx\n",
            get_insn_pc(vmm), gexccode,
             (gexccode == GUESTCTL0_GEXC_GPSI ? "GPSI" :
              (gexccode == GUESTCTL0_GEXC_GSFC ? "GSFC" :
               (gexccode == GUESTCTL0_GEXC_HC ? "HC" :
                (gexccode == GUESTCTL0_GEXC_GRR ? "GRR" :
                  "RESV")))), (l4_umword_t)insn.word, vmm->vmstate->gcp0[MIPS_CP0_STATUS][0]);
      break;

    case GUESTCTL0_GEXC_GVA:
    case GUESTCTL0_GEXC_GHFC:
    case GUESTCTL0_GEXC_GPA:
    default:
      karma_log(WARN, 
        "[%#08lx] Guest Exception Code %lx:%s not yet handled. "
        "Guest Status %#08lx\n",
            get_insn_pc(vmm), gexccode,
             (gexccode == GUESTCTL0_GEXC_GVA ? "GVA" :
              (gexccode == GUESTCTL0_GEXC_GHFC ? "GHFC" :
               (gexccode == GUESTCTL0_GEXC_GPA ? "GPA" :
                "RESV"))), vmm->vmstate->gcp0[MIPS_CP0_STATUS][0]);
      break;
  }

  return EMULATE_FAIL;
}

enum emulation_result vz_trap_handle_ge(vz_vmm_t *vmm)
{
  enum emulation_result er = EMULATE_DONE;
  l4_umword_t gexccode = vmm->vmstate->exit_gexccode;

#if 0
  karma_log(DEBUG, "Hypervisor Guest Exit. GExcCode %lx:%s\n",
        gexccode,
         (gexccode == GUESTCTL0_GEXC_GPSI ? "GPSI" :
          (gexccode == GUESTCTL0_GEXC_GSFC ? "GSFC" :
           (gexccode == GUESTCTL0_GEXC_HC ? "HC" :
            (gexccode == GUESTCTL0_GEXC_GRR ? "GRR" :
             (gexccode == GUESTCTL0_GEXC_GVA ? "GVA" :
              (gexccode == GUESTCTL0_GEXC_GHFC ? "GHFC" :
               (gexccode == GUESTCTL0_GEXC_GPA ? "GPA" :
                "RESV"))))))));
#endif

  switch (gexccode) {
    case GUESTCTL0_GEXC_GPSI:
      VZ_TRACE_EXIT(vmm, hypervisor_gpsi_exits);
      er = vz_trap_handle_gpsi(vmm);
      break;
    case GUESTCTL0_GEXC_GSFC:
      VZ_TRACE_EXIT(vmm, hypervisor_gsfc_exits);
      er = vz_trap_handle_gsfc(vmm);
      break;
    case GUESTCTL0_GEXC_HC:
      VZ_TRACE_EXIT(vmm, hypervisor_hc_exits);
      er = vz_trap_handle_hypcall(vmm);
      break;
    case GUESTCTL0_GEXC_GRR:
      VZ_TRACE_EXIT(vmm, hypervisor_grr_exits);
      er = vz_trap_default_handler(vmm);
      break;
    case GUESTCTL0_GEXC_GVA:
      VZ_TRACE_EXIT(vmm, hypervisor_gva_exits);
      er = vz_trap_default_handler(vmm);
      break;
    case GUESTCTL0_GEXC_GHFC:
      VZ_TRACE_EXIT(vmm, hypervisor_ghfc_exits);
      er = vz_trap_default_handler(vmm);
      break;
    case GUESTCTL0_GEXC_GPA:
      VZ_TRACE_EXIT(vmm, hypervisor_gpa_exits);
      er = vz_trap_default_handler(vmm);
      break;
    default:
      VZ_TRACE_EXIT(vmm, hypervisor_resv_exits);
      er = vz_trap_default_handler(vmm);
      break;
  }

  return er;
}

/* simplistic conversion of pfa guest PA to root VA */
#define STACK_GUESTPA_TO_ROOTVA(_pfa) \
            ((_pfa) - vmm->vm_guest_mapbase + vmm->host_addr_mapbase)
#define IS_KSEG1(_pfa) \
    (((unsigned long)(_pfa) & ~(unsigned long)MIPS_PHYS_MASK) == MIPS_KSEG1_START)

enum emulation_result vz_tlb_handler(vz_vmm_t *vmm)
{
  enum emulation_result er = EMULATE_DONE;

  l4_addr_t pfa = vmm_regs(vmm).badvaddr;
  l4_addr_t pfa_rootva;
  l4_addr_t addr;
  unsigned long size = 1;
  l4_addr_t offset;
  unsigned flags;
  l4re_ds_t ds;
  const int map_write = 1;
  const int map_not_io = 0;

  int result;

  pfa_rootva = STACK_GUESTPA_TO_ROOTVA(pfa);
  addr = pfa_rootva;

  if (params.paravirt_guest)
  {
    l4_addr_t pfa_phys = pfa;

    if (IS_KSEG1(pfa))
        pfa_phys = MIPS_KSEG1_TO_PHYS(pfa);

    if (pfa_phys >= 0x10000000) {
      l4_umword_t exccode = ((vmm_regs(vmm).cause & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE);
      unsigned int write = (exccode == Exc_code_TLBS) || (exccode == Exc_code_Mod);

      if (pfa_phys < 0x1e000000) {
        // continue
      } else if (pfa_phys < 0x1e010000) {
        //return mipsvz_emulate_io(regs, write, pfa, kvm, vcpu);
        if (EMULATE_DONE != vz_update_pc(vmm))
          er = EMULATE_FAIL;
        return er;
      } else if (pfa_phys < 0x1e020000) {
        if (!mipsvz_write_irqchip(vmm, write, pfa_phys))
          er = EMULATE_FAIL;

        if (EMULATE_DONE != vz_update_pc(vmm))
          er = EMULATE_FAIL;
        return er;
      } else {
        // continue
      }
    }
  }

  /* lookup root VA in region manager, addr and size are overwritten */
  result = l4re_rm_find(&addr, &size, &offset, &flags, &ds);
  if (result) {
    karma_log(WARN, "vz_tlb_handler: address %08lx is not mappable to VM. Err %s\n",
        pfa, l4sys_errtostr(result));
    er = EMULATE_FAIL;
  } else if (flags & L4RE_RM_READ_ONLY) {
    karma_log(WARN, "vz_tlb_handler: address %08lx is readonly.\n", pfa);
    er = EMULATE_FAIL;
  } else {
    karma_log(TRACE, "vz_tlb_handler: mapping address %08lx to %08lx\n",
            pfa, pfa_rootva);
    // KYMA TODO expand page fault handling
    /* just map the one page containing the pfa */
    GET_VM.mem().map((pfa & L4_PAGEMASK), (pfa_rootva & L4_PAGEMASK),
            L4_PAGESIZE, map_write, map_not_io);
  }
  return er;
}

enum emulation_result vz_trap_handler(vz_vmm_t *vmm)
{
  enum emulation_result er = EMULATE_DONE;
  l4_umword_t exccode = ((vmm_regs(vmm).cause & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE);

  switch (exccode) {
    case Exc_code_Int:
      VZ_TRACE_EXIT(vmm, int_exits);
      break;

    case Exc_code_CpU:
      VZ_TRACE_EXIT(vmm, cop_unusable_exits);
      er = vz_update_pc(vmm);
      break;

    case Exc_code_Mod:
      VZ_TRACE_EXIT(vmm, tlbmod_exits);
      er = vz_tlb_handler(vmm);
      break;

    case Exc_code_TLBS:
      VZ_TRACE_EXIT(vmm, tlbmiss_st_exits);
      er = vz_tlb_handler(vmm);
      break;

    case Exc_code_TLBL:
      VZ_TRACE_EXIT(vmm, tlbmiss_ld_exits);
      er = vz_tlb_handler(vmm);
      break;

    case Exc_code_GE:
      er = vz_trap_handle_ge(vmm);
      if (EMULATE_DONE != vz_update_pc(vmm))
          er = EMULATE_FAIL;
      break;

    default:
      karma_log(WARN, "Exception Code %lx, not supported\n", exccode);
      er = EMULATE_FAIL;
      break;
  }

  if (er == EMULATE_FAIL)
      karma_log(ERROR,
          "Emulation failed. @ PC %#08lx EPC %#08lx exccode %lx inst %#08lx BadVaddr %#08lx Status %#08lx\n",
          get_insn_pc(vmm), vmm->vmstate->gcp0[MIPS_CP0_EXC_PC][0], exccode,
          get_badinstr(vmm), vmm_regs(vmm).badvaddr,
          vmm->vmstate->gcp0[MIPS_CP0_STATUS][0]);

  return er;
}

/*
 * Sanitize Guest Config register values
 *
 * Some of the root-writeable and guest-writeable config fields documented
 * below may in fact optionally be readonly or writeable on different cores.
 * Attempts to set or clear these fields may be ignored by hardware.
 */
static inline void vz_sanitize_guest_config(l4_umword_t* config)
{
  /*
   * Root-writeable guest fields Config: M,MT
   * Guest-writeable guest fields Config: K23,KU,Impl,K0
   *  - try to force any writeable Config.Impl fields to zero
   *
   *  disallow changes to Config.Impl(16:24) such as
   *    BM  - burst mode
   *    MM  - merge mode
   *    WC  - write control for config.MT
   *    MDU - iterative multiply/divide unit
   *    SB  - simple bus transfer only
   *    UDI - user defined instructions
   *    DSP - data scratch pad RAM
   *    ISP - instr scratch pad RAM
   *
   * Have config1, standard TLB, cacheable, noncoherent
   */
  *config = MIPS_CONF_M | CONF_CM_CACHABLE_NONCOHERENT |
      (MIPS_CONF_MT & (1 << 7)); /* standard TLB */
}

static inline void vz_sanitize_guest_config1(l4_umword_t* config)
{
  /*
   * Root-writeable guest fields Config1: M,MMUSize,C2,MD,PC,WR,CA,FP
   *  - do not allow changes to MMUSize, WR not yet supported
   *
   * Have config2, no coprocessor2 attached, no MDMX support attached,
   * no performance counters, no watch registers, no code compression,
   * enable FPU
   */
  // KYMA TODO Karma does not know the original tlbsize, just leave it
  *config &= ~( /* MIPS_CONF1_TLBS | */
      MIPS_CONF1_C2 | MIPS_CONF1_MD | MIPS_CONF1_PC |
      MIPS_CONF1_WR | MIPS_CONF1_CA);
  *config |= MIPS_CONF_M | MIPS_CONF1_FP /* |
      ((current_cpu_data.vz.tlbsize - 1) << MIPS_CONF1_TLBS_SHIFT) */;
}

static inline void vz_sanitize_guest_config2(l4_umword_t* config)
{
  /*
   * Root-writeable guest fields Config2: M
   * Guest-writeable guest fields Config2: TU,SU
   *
   * Have config3, preserve TU,SU
   */
  *config |= MIPS_CONF_M;
}

static inline void vz_sanitize_guest_config3(l4_umword_t* config)
{
  /*
   * Root-writeable guest fields Config3: M,MSAP,BPG,ULRI,DSP2P,DSPP,
   *      CTXTC,ITL,LPA,VEIC,VInt,SP,CDMM,MT,SM,TL
   * Guest-writeable guest fields Config3: ISAOnExc
   *
   * Have config4, allow having userlocal, allow changes to ISAOnExc
   * Disable all other root-writable options: no DSP ASE, no large
   * physaddr (PABITS), no external interrupt controller, no vectored
   * interrupts, no 1kb pages, no SmartMIPS ASE, no trace logic
   */
  *config &= MIPS_CONF3_ISA_OE; /* zero all except ISAOnExc */
  *config |= MIPS_CONF_M | MIPS_CONF3_ULRI;
}

static inline void vz_sanitize_guest_config4(l4_umword_t* config)
{
  /*
   * Root-writeable guest fields Config4: M,VTLBSizeExt,MMUSizeExt
   * Guest-writeable guest fields Config4: FTLBPageSize
   *
   * Have config5, zero other fields
   *
   * TODO: Implement guest VTLB/FTLB support
   */
  *config = MIPS_CONF_M;
}

static inline void vz_sanitize_guest_config5(l4_umword_t* config)
{
  /*
   * Root-writeable guest fields Config5: MRP (optionally)
   * Guest-writeable guest fields Config5: K,CV,MSAEn,UFR
   *
   * Force to zero
   */
  *config = 0;
}

static inline void vz_sanitize_guest_config6(l4_umword_t* config)
{
  /* Implementation specific, leave unchanged */
  (void)config;
}

static inline void vz_sanitize_guest_config7(l4_umword_t* config)
{
  /* Set Wait IE/IXMT Ignore in Config7, IAR, Clear AR */
  *config = (MIPS_CONF7_WII | MIPS_CONF7_IAR);
}

static void vz_sanitize_guest_config_regs(vz_vmm_t* vmm)
{
  vz_sanitize_guest_config(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][0]));
  vz_sanitize_guest_config1(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][1]));
  vz_sanitize_guest_config2(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][2]));
  vz_sanitize_guest_config3(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][3]));
  vz_sanitize_guest_config4(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][4]));
  vz_sanitize_guest_config5(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][5]));
  vz_sanitize_guest_config6(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][6]));
  vz_sanitize_guest_config7(&(vmm->vmstate->gcp0[MIPS_CP0_CONFIG][7]));
}

int vz_init(vz_vmm_t *vmm)
{
  l4_umword_t guestctl0;
  l4_umword_t guestctl0ext;

  vmm->utcb = l4_utcb();
  karma_log(DEBUG, "VMM: utcb = %p\n", vmm->utcb);

  /* Validate vmstate version */
  if (vmm->vmstate->version != L4_VM_STATE_VERSION) {
      karma_log(ERROR,
          "vz_init failed. VM state version does not match kernel. Version mismatch: %ld, expected %d\n",
          vmm->vmstate->version, L4_VM_STATE_VERSION);
      return -1;
  }

  /* Validate vmstate before first use */
  if (vmm->vmstate->sizeof_vmstate != sizeof(*vmm->vmstate)) {
      karma_log(ERROR,
          "vz_init failed. VM state does not match kernel. Size mismatch: %ld, expected %d\n",
          vmm->vmstate->sizeof_vmstate, sizeof(*vmm->vmstate));
      return -1;
  }

  /* Selectively enable/disable guest virtualization features */
  guestctl0 = vmm->vmstate->host_cp0_guestctl0;
  guestctl0 |= GUESTCTL0_CP0 |
               GUESTCTL0_AT3 |
               GUESTCTL0_CG |
               GUESTCTL0_CF;

  /* Allow guest access to timer */
  guestctl0 |= GUESTCTL0_GT;

  /* Apply change to register at next vcpu_resume_commit() */
  vmm->vmstate->host_cp0_guestctl0 = guestctl0;
  vmm->vmstate->set_cp0_guestctl0 = 1;

  if (guestctl0 & GUESTCTL0_G0E) {
      guestctl0ext = vmm->vmstate->host_cp0_guestctl0ext | GUESTCTL0Ext_CGI;
      // enable guest field change exceptions; disable individual field change
      // exceptions in the GSFC handler as necessary.
      guestctl0ext &= ~GUESTCTL0Ext_FCD;

      vmm->vmstate->host_cp0_guestctl0ext = guestctl0ext;
      vmm->vmstate->set_cp0_guestctl0ext = 1;
  }

  /* Emulate/Initialize guest PRId */
#if 1 // KYMA force Guest VM to be 24Kc
  vmm->vmstate->gcp0[MIPS_CP0_PRID][MIPS_CP0_PRID_SEL] = 0x00019300;
#else
  l4_umword_t readval;
  long result = fiasco_mips_cp0_get(vmm->vmtask,
      MIPS_CP0_PRID, MIPS_CP0_PRID_SEL, &readval, vmm->utcb);
  if (result) {
      karma_log(ERROR, "Failed to get prid register: %s\n", l4sys_errtostr(result));
      return -1;
  }
  vmm->vmstate->gcp0[MIPS_CP0_PRID][MIPS_CP0_PRID_SEL] = readval;
#endif

  /* Modify guest cp0 config registers. Changes are applied during VM resume. */
  vz_sanitize_guest_config_regs(vmm);

  /* set guest vcpu id as EBase.CPUNum */
  vmm->vmstate->gcp0[MIPS_CP0_PRID][MIPS_CP0_EBASE_SEL] &= ~0x3ff;
  vmm->vmstate->gcp0[MIPS_CP0_PRID][MIPS_CP0_EBASE_SEL] |= vmm->id;

  return 0;
}
