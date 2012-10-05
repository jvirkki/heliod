# The libhttpdaemon library contains driver functions for accepting the
# requests off the wire and dispatching them

BUILD_ROOT=../../..
USE_NSPR=1
USE_CLIENTLIBS=1
USE_XERCESC=1
USE_XALANC=1
USE_SMARTHEAP=1

# disable position independent code to improve execution speed
NO_KPIC=1

include ${BUILD_ROOT}/make/defines.mk

MODULE=LibHttpDaemon

all::

LOCAL_INC+=-I.. -I../public
LOCAL_INC+=-I../../support
LOCAL_DEF+= -DBUILD_DLL
LOCAL_DEF+= -DJNI_MD_SYSNAME=\"$(JNI_MD_SYSNAME)\" -DJNI_MD_SYSNAME64=\"$(JNI_MD_SYSNAME64)\" -DJNI_MD_LIBTYPE=\"$(JNI_MD_LIBTYPE)\"

# pick up object list from here
include daemonobjs.mk

AR_TARGET=httpdaemon
AR_OBJS=$(DAEMONOBJS)

EXPORT_LIBRARIES=$(AR_TARGET)


# this should always be last!
include ${BUILD_ROOT}/make/rules.mk
