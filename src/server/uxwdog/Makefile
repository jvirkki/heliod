BUILD_ROOT=../../..
MODULE=uxwdog
USE_NSPR=1
USE_CLIENTLIBS=1

include $(BUILD_ROOT)/make/defines.mk

DEFINES+=-DENCRYPT_PASSWORDS -DBUILD_DLL 

LOCAL_INC+= -I..

EXE_TARGET=$(PRODUCT_WATCHDOG_BIN)
EXE_OBJS=watchdog wdsignals wdpwd wdlog wdlsmgr

ifdef FEAT_SMF
EXE_OBJS+=wdsmf wdutil
endif

ifneq ($(OS_ARCH),AIX)
EXE_LIBDIRS+=../base/$(OBJDIR)
EXE_LIBS+=libbase
else
# Link in only necessary files on AIX since the linker expects all references
# in libbase to be resolved otherwise
BASE_OBJS=unix_utils wdservermessage
EXE_NONPARSED_OBJS=$(addprefix ../base/$(OBJDIR)/, \
                   $(BASE_OBJS:=.$(OBJ)))
endif

SHIP_PRIVATE_BINARIES=$(EXE_TARGET)

include $(BUILD_ROOT)/make/rules.mk
