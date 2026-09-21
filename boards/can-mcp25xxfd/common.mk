ifndef QCONFIG
QCONFIG=qconfig.mk
endif

include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=CAN io-sock driver for the Microchip MCP25xxFD SPI CAN-FD controller family
endef

EXTRA_INCVPATH=$(PROJECT_ROOT)/../../lib/can/public/

INTERFACE_PREFIX="can"

define DRIVER_SPECIFIC_OPTIONS

config  - Path to a config file supplying the per-unit hints described below.
          See boards/can-mcp25xxfd/mcp25xxfd.conf for a working example.

Configuration is per-unit via the hint API, supplied through the "config"
option above. Each unit
(0, 1, 2, ...) that has at least a "dev" hint set created as its own
canN interface. Per-unit hints (hint.mcp25xxfd.<unit>.<key>="value" in the
config file):

  dev=<path>            Required. Path to the SPI node the MCP25xxFD is
                        connected through, e.g. /dev/io-spi/spi0/dev0.
                        The presence of this key gates whether the unit is
                        created at all. A unit with no "dev" hint is
                        skipped.
  gpio=<num>            Required. The GPIO connected to the MCP25xxFD
                        interrupt line. The driver configures the pin
                        itself.
  platform=[auto|rpi4|rpi5|generic]
                        The platform the driver is running on. If
                        "auto", the platform is detected automatically. If
                        "generic", the gpio number is used directly as the
                        IRQ vector instead of being resolved through a
                        platform-specific GPIO controller. Defaults to auto.
  bps=<bps>             Nominal/Arbitration bitrate. Must be > 0 and
                        <= 1000000. Defaults to 1000000 (1Mbps).
  dbps=<bps>            CAN FD data bitrate. Must be <= 8000000. Defaults to
                        5000000 (5Mbps).
  fd=[on|non-iso|off]   CAN FD operating mode. "on" is ISO 11898-1:2015 mode,
                        "non-iso" is the pre-ISO 2012 whitepaper mode, "off"
                        disables CAN FD. Defaults to "off".
  osc=<freq>            Oscillator frequency driving the MCP25xxFD OSC1 pin,
                        2000000 <= freq <= 40000000. Defaults to 40000000
                        (40MHz).
  tx_fifo_size=<num>    Slots in the HW transmit queue, 1 <= num <= 32.
                        Defaults to 6.
  rx_fifo_size=<num>    Slots in the HW receive queue, 1 <= num <= 32.
                        Defaults to 20.

Example config file bringing of 2 CAN BUS Devices CAN0 (Classic only) and CAN1 (CAN FD at 500Kbps/2Mbps):

  hint.mcp25xxfd.0.dev="/dev/io-spi/spi0/dev0"
  hint.mcp25xxfd.0.gpio="25"

  hint.mcp25xxfd.1.dev="/dev/io-spi/spi0/dev1"
  hint.mcp25xxfd.1.gpio="24"
  hint.mcp25xxfd.1.fd="on"
  hint.mcp25xxfd.1.bps="500000"
  hint.mcp25xxfd.1.dbps="2000000"

The following sysctls are created by this driver:
hw.mcp25xxfd

endef

include devs/devs.mk
