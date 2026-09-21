ifndef QCONFIG
QCONFIG=qconfig.mk
endif

include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=Sample io-sock fdt mac driver
endef

EXTRA_INCVPATH=$(PROJECT_ROOT)/../../lib/can/public/

INTERFACE_PREFIX="can"

define DRIVER_SPECIFIC_OPTIONS

The following sysctls are create by this driver:
hw.sample.debug

endef

include devs/devs.mk
