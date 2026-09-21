ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

USEFILE =
EXTRA_INCVPATH=$(PROJECT_ROOT)/../../lib/can/public/
LIBS += socket

include $(MKFILES_ROOT)/qtargets.mk
