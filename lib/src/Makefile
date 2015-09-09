PKGDIR      ?= ../..
L4DIR       ?= $(PKGDIR)/../..

TARGET          = libkarma.a
PC_FILENAME    := karmalib
SYSTEMS         = mips mips-l4f
SRC_CC         += uart_karma.cc hypercall.cc
CXXFLAGS       += -DL4_NO_RTTI -fno-exceptions -fno-rtti

PRIVATE_INCDIR += $(SRC_DIR)/../../include

include $(L4DIR)/mk/lib.mk
