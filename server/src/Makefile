PKGDIR	    	?= ../..
L4DIR		    ?= $(PKGDIR)/../..

TARGET	    	= karma
SYSTEMS			= x86-l4f
SRC_CC_karma	= main.cc \
				  l4_cpu_bus.cc \
				  l4_cpu.cc \
				  l4_linux.cc \
				  l4_mem.cc \
				  l4_vcpu.cc \
				  l4_vm.cc \
				  l4_vm_driver.cc \
				  devices/ahci.cc \
				  devices/apic.cc \
				  devices/bdds.cc \
				  devices/devices.cc \
				  devices/fb.cc \
				  devices/gic.cc \
				  devices/hpet.cc \
				  devices/ioport.cc \
				  devices/net.cc \
				  devices/pci.cc \
				  devices/ser.cc \
				  devices/timer.cc \
				  util/local_statistics.cc
				  
REQUIRES_LIBS	= rtc \
				  l4util \
				  l4re_c-util \
				  libstdc++ \
				  libio \
				  libpthread \
				  ankh \
				  libudis86 \
				  libvcpu \
				  cxx_libc_io \
				  libdrivers_hpet

include $(L4DIR)/mk/prog.mk
