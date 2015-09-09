/*
 * vmcs.h - VMCS definitions for X86 virtualization interface
 *
 * (c) 2011 Matthias Lange <mlange@sec.t-labs.tu-berlin.de>,
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */

#pragma once

#include <l4/sys/vm>
#include <malloc.h>
#include <string.h>
#include "l4_vcpu.h"

// 16 bit width
enum
  {
	VMX_VPID           = 0x0,
	VMX_GUEST_ES_SEL   = 0x800,
	VMX_GUEST_CS_SEL   = 0x802,
	VMX_GUEST_SS_SEL   = 0x804,
	VMX_GUEST_DS_SEL   = 0x806,
	VMX_GUEST_FS_SEL   = 0x808,
	VMX_GUEST_GS_SEL   = 0x80a,
	VMX_GUEST_LDTR_SEL = 0x80c,
	VMX_GUEST_TR_SEL   = 0x80e,
	VMX_HOST_ES_SEL    = 0xc00,
	VMX_HOST_CS_SEL    = 0xc02,
	VMX_HOST_SS_SEL    = 0xc04,
	VMX_HOST_DS_SEL    = 0xc06,
	VMX_HOST_FS_SEL    = 0xc08,
	VMX_HOST_GS_SEL    = 0xc0a,
	VMX_HOST_TR_SEL    = 0xc0c,
  };

// 64 bit width
enum
  {
	VMX_IO_BITMAP_ADDR_A            = 0x2000,
	VMX_IO_BITMAP_ADDR_B            = 0x2002,
	VMX_MSR_BITMAPS                 = 0x2004,
	VMX_EXIT_MSR_STORE_ADDR         = 0x2006,
	VMX_EXIT_MSR_LOAD_ADDR          = 0x2008,
	VMX_ENTRY_MSR_LOAD_ADDR         = 0x200a,
	VMX_EXEC_VMCS_PTR               = 0x200c,
	VMX_TSC_OFFSET                  = 0x2010,
	VMX_VIRT_APIC_ADDR              = 0x2012,
	VMX_APIC_ACCESS_ADDR            = 0x2014,
	VMX_EPT_PTR                     = 0x201a,
	VMX_GUEST_PHYS_ADDR             = 0x2400,
	VMX_VMCS_LINK_PTR               = 0x2800,
	VMX_GUEST_IA32_DEBUGCTL         = 0x2802,
	VMX_GUEST_IA32_PAT              = 0x2804,
	VMX_GUEST_IA32_EFER             = 0x2806,
	VMX_GUEST_IA32_PERF_GLOBAL_CTRL = 0x2808,
	VMX_GUEST_PDPTE0                = 0x280a,
	VMX_GUEST_PDPTE1                = 0x280c,
	VMX_GUEST_PDPTE2                = 0x280e,
	VMX_GUEST_PDPTE3                = 0x2810,
	VMX_HOST_IA32_PAT               = 0x2c00,
	VMX_HOST_IA32_EFER              = 0x2c02,
	VMX_HOST_IA32_PERF_GLOBAL_CTRL  = 0x2c04,
  };

// 32 bit width
enum
  {
	VMX_PIN_EXEC_CTRL            = 0x4000,
	VMX_PRIMARY_EXEC_CTRL        = 0x4002,
	VMX_EXCEPTION_BITMAP         = 0x4004,
	VMX_PF_ERROR_CODE_MASK       = 0x4006,
	VMX_PF_ERROR_CODE_MATCH      = 0x4008,
	VMX_CR3_TARGET_COUNT         = 0x400a,
	VMX_EXIT_CTRL                = 0x400c,
	VMX_EXIT_MSR_STORE_COUNT     = 0x400e,
	VMX_EXIT_MSR_LOAD_COUNT      = 0x4010,
	VMX_ENTRY_CTRL               = 0x4012,
	VMX_ENTRY_MSR_LOAD_COUNT     = 0x4014,
	VMX_ENTRY_INTERRUPT_INFO     = 0x4016,
	VMX_ENTRY_EXCEPTION_ERROR    = 0x4018,
	VMX_ENTRY_INSTRUCTION_LENGTH = 0x401a,
	VMX_TPR_THRESHOLD            = 0x401c,
	VMX_SECOND_EXEC_CTRL         = 0x401e,
	VMX_PLE_GAP                  = 0x4020,
	VMX_PLE_WINDOW               = 0x4022,
	VMX_INSTRUCTION_ERROR        = 0x4400,
	VMX_EXIT_REASON              = 0x4402,
	VMX_EXIT_INTERRUPT_INFO      = 0x4404,
	VMX_EXIT_INTERRUPT_ERROR     = 0x4406,
	VMX_IDT_VECTORING_INFO_FIELD = 0x4408,
	VMX_IDT_VECTORING_ERROR      = 0x440a,
	VMX_EXIT_INSTRUCTION_LENGTH  = 0x440c,
	VMX_EXIT_INSTRUCTION_INFO    = 0x440e,
	VMX_GUEST_ES_LIMIT           = 0x4800,
	VMX_GUEST_CS_LIMIT           = 0x4802,
	VMX_GUEST_SS_LIMIT           = 0x4804,
	VMX_GUEST_DS_LIMIT           = 0x4806,
	VMX_GUEST_FS_LIMIT           = 0x4808,
	VMX_GUEST_GS_LIMIT           = 0x480a,
	VMX_GUEST_LDTR_LIMIT         = 0x480c,
	VMX_GUEST_TR_LIMIT           = 0x480e,
	VMX_GUEST_GDTR_LIMIT         = 0x4810,
	VMX_GUEST_IDTR_LIMIT         = 0x4812,
	VMX_GUEST_ES_ACCESS_RIGHTS   = 0x4814,
	VMX_GUEST_CS_ACCESS_RIGHTS   = 0x4816,
	VMX_GUEST_SS_ACCESS_RIGHTS   = 0x4818,
	VMX_GUEST_DS_ACCESS_RIGHTS   = 0x481a,
	VMX_GUEST_FS_ACCESS_RIGHTS   = 0x481c,
	VMX_GUEST_GS_ACCESS_RIGHTS   = 0x481e,
	VMX_GUEST_LDTR_ACCESS_RIGHTS = 0x4820,
	VMX_GUEST_TR_ACCESS_RIGHTS   = 0x4822,
	VMX_GUEST_INTERRUPTIBILITY_STATE = 0x4824,
	VMX_GUEST_ACTIVITY_STATE         = 0x4826,
	VMX_GUEST_SMBASE                 = 0x4828,
	VMX_GUEST_IA32_SYSENTER_CS       = 0x482a,
	VMX_PREEMPTION_TIMER_VALUE       = 0x482e,
	VMX_HOST_IA32_SYSENTER_CS        = 0x4C00,
  };

// natural width
enum
  {
	VMX_CR0_MASK = 0x6000,
	VMX_CR4_MASK = 0x6002,
	VMX_CR0_READ_SHADOW = 0x6004,
	VMX_CR4_READ_SHADOW = 0x6006,
	VMX_CR3_TARGET_VALUE0 = 0x6008,
	VMX_CR3_TARGET_VALUE1 = 0x600a,
	VMX_CR3_TARGET_VALUE2 = 0x600c,
	VMX_CR3_TARGET_VALUE3 = 0x600e,
	VMX_EXIT_QUALIFICATION = 0x6400,
	VMX_IO_RCX             = 0x6402,
	VMX_IO_RSI             = 0x6404,
	VMX_IO_RDI             = 0x6406,
	VMX_IO_RIP             = 0x6408,
	VMX_GUEST_LINEAR_ADDR  = 0x640a,
	VMX_GUEST_CR0          = 0x6800,
	VMX_GUEST_CR3          = 0x6802,
	VMX_GUEST_CR4          = 0x6804,
	VMX_GUEST_ES_BASE      = 0x6806,
	VMX_GUEST_CS_BASE      = 0x6808,
	VMX_GUEST_SS_BASE      = 0x680a,
	VMX_GUEST_DS_BASE      = 0x680c,
	VMX_GUEST_FS_BASE      = 0x680e,
	VMX_GUEST_GS_BASE      = 0x6810,
	VMX_GUEST_LDTR_BASE    = 0x6812,
	VMX_GUEST_TR_BASE      = 0x6814,
	VMX_GUEST_GDTR_BASE    = 0x6816,
	VMX_GUEST_IDTR_BASE    = 0x6818,
	VMX_GUEST_DR7          = 0x681a,
	VMX_GUEST_RSP          = 0x681c,
	VMX_GUEST_RIP          = 0x681e,
	VMX_GUEST_RFLAGS       = 0x6820,
	VMX_GUEST_PENDING_DEBUG_EXCEPTIONS = 0x6822,
	VMX_GUEST_SYSENTER_ESP      = 0x6824,
	VMX_GUEST_SYSENTER_EIP      = 0x6826,
	VMX_HOST_CR0                = 0x6c00,
	VMX_HOST_CR3                = 0x6c02,
	VMX_HOST_CR4                = 0x6c04,
	VMX_HOST_FS_BASE            = 0x6c06,
	VMX_HOST_GS_BASE            = 0x6c08,
	VMX_HOST_TR_BASE            = 0x6c0a,
	VMX_HOST_GDTR_BASE          = 0x6c0c,
	VMX_HOST_IDTR_BASE          = 0x6c0e,
	VMX_HOST_SYSENTER_ESP       = 0x6c10,
	VMX_HOST_SYSENTER_EIP       = 0x6c12,
	VMX_HOST_RSP                = 0x6c14,
	VMX_HOST_RIP                = 0x6c16,
  };

template <unsigned int INDEX>
class Vmcs_Field_Size_Selector{
public:
    enum { size = sizeof(l4_umword_t) };
    typedef l4_umword_t type;
};
template <>
class Vmcs_Field_Size_Selector<0>{
public:
    enum { size = 2 };
    typedef l4_uint16_t type;
};
template <>
class Vmcs_Field_Size_Selector<1>{
public:
    enum { size = 8 };
    typedef l4_uint64_t type;
};
template <>
class Vmcs_Field_Size_Selector<2>{
public:
    enum { size = 4 };
    typedef l4_uint32_t type;
};

template <unsigned int FIELD>
class Vmcs_Field_Properties{
public:
    enum { is_writable = !((FIELD & 0xc00) == 0x400) };
    enum { size = Vmcs_Field_Size_Selector< ((FIELD & 0x6000) >> 13) >::size };
    typedef typename Vmcs_Field_Size_Selector< ((FIELD & 0x6000) >> 13) >::type type;
};

template <bool CONDITION>
class TAssert{
public:
    static void check(){
    };
};
template <>
class TAssert<false>{
    //if this compile time error happens you probably tried to write do a read only field
};



class Vmcs
{
public:
  Vmcs()
	{
	  _vmcs = (void *)current_vcpu().getExtState();
	}
  
  ~Vmcs() {}

template<unsigned int FIELD>
inline typename Vmcs_Field_Properties<FIELD>::type vmread() const{
    return _vmread<typename Vmcs_Field_Properties<FIELD>::type>(FIELD);
}
template<unsigned int FIELD>
inline void vmwrite(typename Vmcs_Field_Properties<FIELD>::type value){
    TAssert<Vmcs_Field_Properties<FIELD>::is_writable>::check();
    return _vmwrite(FIELD, value);
}

template< typename T >
inline T _vmread(unsigned field) const
{
  return *(T *)l4_vm_vmx_field_ptr(_vmcs, field);
}

template< typename T >
inline void _vmwrite(unsigned field, T value)
{
  *(T *)l4_vm_vmx_field_ptr(_vmcs, field) = value;
}

Vmcs& operator=(const Vmcs &src)
{
  if(this != &src)
	memcpy(_vmcs, src._vmcs, 4096);

  return *this;
}

void *vmcs(void)
{
  return _vmcs;
}

//  template< typename T >
//	inline T vmread(unsigned field);
//  
//  template< typename T >
//	inline void vmwrite(unsigned field, T value);
//  
//  Vmcs& operator=(const Vmcs &src);

private:
  void *_vmcs;
};
