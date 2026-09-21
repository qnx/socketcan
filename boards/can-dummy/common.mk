ifndef QCONFIG
QCONFIG=qconfig.mk
endif

include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=Dummy io-sock CAN hardware driver
endef

EXTRA_INCVPATH=$(PROJECT_ROOT)/../../lib/can/public/

INTERFACE_PREFIX="can"

define DRIVER_SPECIFIC_OPTIONS

config  - Path to a config file supplying the optional hints described below.
		See boards/can-dummy/candummy.conf for a working example.

By default no dummy devices are created, making the configuration mandatory for use.

To create a dummy CAN device provide the following hint
hint.dummy.<unit>.create="true"

2 more optional hints can be provided to enable FD support and change the rate at
which frames are sent
hint.dummy.<unit>.fd="true"
hint.dummy.<unit>.rate_ms="500"

The following sysctls are create by this driver:
hw.candummy

endef

include devs/devs.mk
