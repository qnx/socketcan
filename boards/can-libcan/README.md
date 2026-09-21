# can-libcan

A SocketCAN proxy implementation for QNX's native libcan framework.

# Overview

On startup by default this driver will scan for `/dev/canX` devices. For every
device found a resulting SocketCAN interface will be created.

You can also provide a configuration file to map specific libcan devices to SocketCAN interfaces.

Once devices are created a wildcard filter is applied to each RX mailbox with `CAN_DEVCTL_SET_MFILTER`
and a single TX mailbox is opened for writing back to devices.

# Testing

This libcan driver has only been tested as working with [can-mcp2515](https://gitlab.com/qnx/projects/drivers/can-mcp2515) libcan driver.

# Configuration
If you do not wish to load auto discover all libcan devices or need a specific mapping a configuration can be used to limit libcan scope.

A [sample configuration](./libcan.conf) file has been provided which shows off how to load specific devices and change mapping.


io-sock -m ${PWD}/mods-can.so -d ${PWD}/devs-can-libcan.so  -o prefix=/can
SOCK=/can ifconfig can0 up
SOCK=/can ifconfig can1 up
SOCK=/can ./candump can0
SOCK=/can ./can-read can0

# Known limitations

SocketCAN libcan proxy must be the only consumer of the libcan device or messages may be dropped.
- libcan expects only one consumer per mailbox so if 2 connect messages may get lost between the multiple clients

Bittiming is ignored.
- libcan already takes care of configuration of the device so all device options such as bittiming or enabling FD should be done on libcan configuration.
