PKGDIR	    	?= ../..
L4DIR		    ?= $(PKGDIR)/../..

TARGET	    	= karma karma_linux karma_fiasco starter
SYSTEMS			= mips-l4f

SRC_CC_mips     = $(addprefix ARCH-mips/, vmm.cc loader.cc irqchip.cc)
PRIVATE_INCDIR  = $(SRC_DIR)/ARCH-$(ARCH) $(PKGDIR)/include

SRC_CC_common	= main.cc \
				  $(SRC_CC_$(ARCH)) \
				  l4_cpu.cc \
				  l4_mem.cc \
				  l4_vcpu.cc \
				  l4_vm.cc \
				  l4_vm_driver.cc \
				  devices/mem.cc \
				  devices/bdds.cc \
				  devices/devices.cc \
				  devices/fb.cc \
				  devices/gic.cc \
				  devices/pci.cc \
				  devices/ser.cc \
				  devices/timer.cc \
				  devices/shmem.cc \
				  thread.cc \
				  util/local_statistics.cc

# select default karma OS
OS              = linux

SRC_CC_linux    = l4_linux.cc linux_prom.cc
SRC_CC_fiasco   = l4_fiasco.cc

SRC_CC_karma_linux = $(SRC_CC_common) \
                     $(SRC_CC_linux)

SRC_CC_karma_fiasco = $(SRC_CC_common) \
                     $(SRC_CC_fiasco)

SRC_CC_karma    = $(SRC_CC_karma_$(OS))

SRC_CC_starter	= starter.cc devices/shmem.cc

REQUIRES_LIBS	= rtc \
				  l4util \
				  l4sys \
				  l4re_c-util \
				  libstdc++ \
				  libio \
				  libpthread \
				  shmc \
				  libvcpu \
				  cxx_libc_io

include $(L4DIR)/mk/prog.mk
