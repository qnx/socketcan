ifndef QCONFIG
QCONFIG=qconfig.mk
endif

NAME=sockcan

include $(QCONFIG)

INSTALLDIR= /lib/dll

define PINFO
PINFO DESCRIPTION=Exposes SocketCAN AF_CAN address family
endef
EXTRA_CLEAN+= $(PROJECT_ROOT)/mods-can.use

include devs/mods.mk
