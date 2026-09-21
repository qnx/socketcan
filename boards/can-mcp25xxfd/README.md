# mcp25xxfd

This is a SocketCAN compatible driver for the Microchip MCP25xxFD CAN Controller.

The datasheet for the MCP2518FD member of the family, as well as the general
family documentation/datasheet can be found here:
[MCP2518FD Datasheet](https://www.microchip.com/en-us/product/mcp2518fd#Documentation)

See below for specifics about the known HW that uses the MCP2518FD.

**NOTE** At this time, the driver works on the Raspberry Pi 4 and Pi 5 platforms.
         It was developed for the Waveshare 2-CH CAN FD HAT but in theory should
         work for any other HW connected to an RPI4/RPI5 that uses the MCP2518FD.

# SPI Configuration

The MCP25xxFD uses SPI to communicate. The configuration for the QNX SPI driver
will need to be updated to add devices, one for each MCP25xxFD that is
connected to the SPI bus(es).

The configuration file for the SPI driver is often found at
`/system/etc/config/spi/spi.conf`. However a non-standard location may be
used. Check how the SPI driver was started to be sure.

The SPI configuration for each MCP25xxFD device should be of the form:
```
[dev]
parent_busno=PARENT_BUS_NUMBER
devno=DEVNO
name=NAME
clock_rate=17000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1
```
Where:
**PARENT_BUS_NUMBER** is the number of the SPI bus the MCP25xxFD is
connected with as defined by the SPI driver's configuration file.

**DEVNO** The unique, on this bus, device number. This usually corresponds to
          how the various CS (ChipSelect) lines should be made active to
          enable the desired device. See the documentation for your platform's
          specific SPI driver for details.

**NAME** A unique name of the device. It will be used to name the node that
is created in the filesystem.

## SPI0 example
For example, if `PARENT_BUS_NUMBER` is 0 (spi0) and you want to add
sections for two MCP25xxFD devices you might use a configuration like:
```
[dev]
parent_busno=0
devno=0
name=dev0
clock_rate=17000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1

[dev]
parent_busno=0
devno=1
name=dev1
clock_rate=17000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1
```

This will cause the QNX SPI driver to create two nodes at:
`/dev/io-spi/spi0/dev0` and `/dev/io-spi/spi0/dev1`.

## SPI0 and SPI1 example
For example, if you have two MCP25xxFD devices, one on SPI0 and the other on
SPI1. You might have a configuration like:

```
[bus]
busno=0
name=spi0
base=<spi0_base_address>
irq=<spi0_irq>
input_clock=200000000
bs=cs_max=1

[bus]
busno=1
name=spi1
base=<spi1_base_address>
irq=<spi1_irq>
input_clock=200000000
bs=cs_max=1

[dev]
parent_busno=0
devno=0
name=dev0
clock_rate=17000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1

[dev]
parent_busno=1
devno=0
name=dev0
clock_rate=17000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1
```

This will cause the QNX SPI driver to create two nodes at:
`/dev/io-spi/spi0/dev0` and `/dev/io-spi/spi1/dev1`.

**NOTE** The MCP2518FD requires that its SPI clock_rate be less than or equal
         to 0.85 * (FSYSCLK/2). FSYSCLK is equal to the oscillator frequency
         which depends on the HW platform. For the Waveshare 2-CH CAN FD HAT
         it is 40Mhz. Thus the SPI clock_rate must be less than or equal to
         17MHz. The maximum SPI clock_rate that MCP2518FD supports, regardless
         of FSYSCLK is also 17MHz.

## SPI Base and IRQ

For reference, here are the current base addresses and IRQs for the various SPI
buses available on the supported hardware. These are subject to change with
newer HW revisions.

### RPI4
| SPI Bus | Base Address | IRQ |
|---------|--------------|-----|
| SPI0    |  0xfe204000  | 150 |
| SPI1    |  0xfe215000  | 125 |
| SPI2    |  0xfe215000  | 125 |
| SPI3    |  0xfe204600  | 150 |
| SPI4    |  0xfe204800  | 150 |
| SPI5    |  0xfe204a00  | 150 |
| SPI6    |  0xfe204c00  | 150 |

### RPI5
| SPI Bus | Base Address | IRQ |
|---------|--------------|-----|
| SPI0    | 0x1f00050000 | 179 |
| SPI1    | 0x1f00054000 | 180 |
| SPI2    | 0x1f00058000 | 181 |
| SPI3    | 0x1f0005c000 | 182 |
| SPI4    | 0x1f00060000 | 183 |
| SPI5    | 0x1f00064000 | 184 |

# SW Interface

This driver exposes a SocketCAN compatible interface. The goal is that an
application developed on Linux targeting SocketCAN should just work after
recompiling for QNX.

**NOTE** The SocketCAN implementation is not complete at this time. As it is
         still a work in progress, expect missing functionality. For instance
         only RAW sockets are supported, BCM sockets are not.

**NOTE** Not all the constant values are the same between Linux and QNX
         SocketCAN. For example, Linux defines PF_CAN with the value 29. On
         QNX, PF_CAN is defined with the value 44. If you use the supplied
         headers that should be handled automatically. However if you use
         the Linux headers there will be problems.

For details please see:

[SocketCAN](https://docs.kernel.org/networking/can.html)

# Configuration

The driver is configured through a config file, passed via io-sock's
`-o config=<path>` option. Each
MCP25xxFD chip to bring up, called a unit, is configured by a set of
`hint.mcp25xxfd.<unit>.<key>="value"` lines, `<unit>` being 0, 1, 2, and so
on. See the driver's "use" information (`use devs-can-mcp25xxfd`) for the
for full details.

## Required hints per unit
`dev` and `gpio` MUST be provided for a unit to be created.

`dev="<path_to_device>"`
Path to the SPI device node for the MCP25xxFD the driver will control.
The path will depend on the SPI configuration. Using the same terms as in the
[SPI Configuration](#SPI-Configuration) section above it would be:
`/dev/io-spi/spi<PARENT_BUS_NUMBER>/<NAME>`

gpio=<gpio_num>
The GPIO number used for the MCP25xxFD's INT pin. The driver will automatically
configure the pin.

### Optional but common hints
The following hints are all optional but are often used. They all have
default values if not specified.

bps=<bps>
Sets the Nominal/Arbitration bitrate of the CAN bus. Supported values are
defined by the CAN specification. Typically is one of 250000, 500000, or
1000000 for a bitrate of 250Kbps, 500Kbps, or 1Mbps respectively.

Defaults to 1000000.

osc=<frequency>
Sets the frequency of the oscillator feeding the MCP25xxFD's OSCx pins.

Defaults to 40000000.

dbps=<bps>
Sets the Data bitrate of the CAN FD bus. Supported values are
defined by the CAN specification.

Defaults to 5000000.

fd=[on|non-iso|off]
Sets the CAN FD operating mode of the MCP25xxFD. This MUST be set to either
'on' or 'non-iso' if you want CAN FD support. Note that 'non-iso' is for
an earlier version of CAN FD and is probably not what you want.

Defaults to 'off'.

# Hardware

## Waveshare 2-CH CAN FD HAT

The [Waveshare 2-CH CAN FD HAT](https://www.waveshare.com/product/raspberry-pi/hats/interface-power/2-ch-can-fd-hat.htm)
is a board designed for the RPI series of devices. It makes use of the RPI's 40
pin GPIO header to connect to the RPI's CPU.

**NOTE** At this time, the driver only works on the RaspberryPi 4,
         Raspberry Pi 5, and their variants.

The [Wiki](https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT) is a good resource
for information about the HW. It is Linux specific but information about the
HW applies to QNX as well. The SW information can generally be disregarded.

Two MCP2518FD chips are used on the Waveshare 2-CH CAN FD HAT to manage two
independent CAN buses. By default the MCP2518FD chips are connected to
different SPI busses: SPI0 and SPI1. This can be changed by moving some 0Ohm
resistors on the HW. See [here](https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT#Single_SPI_Mode)
for details. This is required on QNX to use both MCP2518FD chips as there is
currently no SPI1 support.

Once the SPI bus is configured to add support for one (or both) MCP2518FD
chips on the Waveshare, this driver can be loaded with a config file to
expose a SocketCAN interface. A single instance of the driver MUST handle
all connected MCP2518FD chips.

The Waveshare uses a 40MHz clock to drive its MCP2518FD chips. This is the
default for this driver so you should not need to specify `osc` in most
configs.

The Waveshare is configured to use a specific, unique, GPIO for each MCP2518FD.
This configuration can be changed via a HW modification. See the Wiki for
details.

CAN0/Dev0 uses GPIO25 by default or GPIO13 with a HW mod.
CAN1/Dev1 uses GPIO24 by default or GPIO23 or GPIO22 with a HW mod.

### Examples
All these examples assume that the two SPI nodes are exposed as dev0 and dev1
for the waveshare's can0 and can1 buses respectively. It also assumes that the
required HW modifications are done in order to place the can1 bus on SPI0.
Each is a config file, loaded with `-o config=<path>` (see
`boards/can-mcp25xxfd/mcp25xxfd.conf` for a full working example).

Bringing up just the CAN0 device on the waveshare using defaults for all
optional values:
```
hint.mcp25xxfd.0.dev="/dev/io-spi/spi0/dev0"
hint.mcp25xxfd.0.gpio="25"
```

Bringing up both the CAN0 and CAN1 devices on the waveshare, using defaults
for all optional values:
```
hint.mcp25xxfd.0.dev="/dev/io-spi/spi0/dev0"
hint.mcp25xxfd.0.gpio="25"

hint.mcp25xxfd.1.dev="/dev/io-spi/spi1/dev0"
hint.mcp25xxfd.1.gpio="24"
```

Bringing up both CAN0 and CAN1 where CAN0 is CAN-FD capable but CAN1 is not:
```
hint.mcp25xxfd.0.dev="/dev/io-spi/spi0/dev0"
hint.mcp25xxfd.0.gpio="25"
hint.mcp25xxfd.0.fd="on"

hint.mcp25xxfd.1.dev="/dev/io-spi/spi1/dev0"
hint.mcp25xxfd.1.gpio="24"
```

Bringing up both CAN0 and CAN1 where both are CAN-FD capable, but CAN1 is
using the older non-iso version of CAN-FD:
```
hint.mcp25xxfd.0.dev="/dev/io-spi/spi0/dev0"
hint.mcp25xxfd.0.gpio="25"
hint.mcp25xxfd.0.fd="on"

hint.mcp25xxfd.1.dev="/dev/io-spi/spi1/dev0"
hint.mcp25xxfd.1.gpio="24"
hint.mcp25xxfd.1.fd="non-iso"
```

