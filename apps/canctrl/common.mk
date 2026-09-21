ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

NAME=canctrl
USEFILE =
EXTRA_INCVPATH=$(PROJECT_ROOT)/../../lib/can/public/
LIBS += socket

include $(MKFILES_ROOT)/qtargets.mk
