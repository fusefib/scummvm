MODULE := engines/wizardry

MODULE_OBJS := \
	wizardry.o \
	metaengine.o \
	data/ucsd_disk.o \
	vm/codefile.o \
	vm/machine.o \
	vm/console.o

ifeq ($(ENABLE_WIZARDRY), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

include $(srcdir)/rules.mk
DETECT_OBJS += $(MODULE)/detection.o
