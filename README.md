# SocketCAN for QNX
This project contains a QNX io-sock module implementation of Linux's SocketCAN.

## What is SocketCAN
SocketCAN is a Linux CAN implementation which lives in the networking layer. It was done this way so client queues and other mechanisms could be reused. In order to add this, a new address family AF_CAN was added. The main CAN protocol is CAN_RAW which allows you to communicate to can devices by using frames with read and write calls directly. There is also CAN_BCM but is not supported in this implementation.

## Where can I learn more
Each component contains a README.md explaining a bit about the component as well as how to use it.

## Building SocketCAN
> SocketCAN must be cross compiled on your host. Self hosted builds are not currently supported

### Building drivers
To build SocketCAN for QNX simply run the following command from the root directory
```
make
```

### Building module
The module cannot currently be built due to requiring some internal hooks. We are working to remove this dependency on internal io-sock code and in the meantime have supplied a build module in. Once the dependency has been removed the remaining source will be released.

The remaining (not internal) source code is released for reference but can't be built.

## How can I use it
QNX's SocketCAN implementation has been designed to be a drop in replacement for Linux's SocketCAN where possible but does have some differences. The main differences between Linux and our implementation is the way you set up and configure your devices.

### Missing functionality
As development continues we hope to have full support of SocketCAN, but are currently missing some features. This section details what is missing.

#### CAN_BCM
CAN_BCM is currently not supported nor planned for the future.

### Using an alterative networking stack
If you want to test out SocketCAN without affecting your existing network stack you can start a new io-sock instance with the following command. In this case /can is used for our alternative stack.
```bash
io-sock -m /data/home/qnxuser/mods-can.so -d /data/home/qnxuser/devs-can-vcan.so -o prefix=/can
```

Now your SocketCAN stack should be created and you can run commands prefixed with SOCK=<stack_path>.

For example:
```bash
SOCK=/can ifconfig
```

### Creating a CAN device
Loading a driver does not create any interfaces, so you need to create one before there is anything to read from or write to. This mirrors Linux, where you would run
`ip link add dev vcan0 type vcan`. For the vcan driver:
```bash
SOCK=/can ifconfig vcan0 create
```

Leave the unit number off to have the next free one picked for you; the allocated name is
printed:
```bash
SOCK=/can ifconfig vcan create
vcan1
```

The device is removed again with `destroy`:
```bash
SOCK=/can ifconfig vcan0 destroy
```

With a device created you can run the samples against it:
```bash
SOCK=/can ./can-write
SOCK=/can ./can-read
```

### Mounting driver
To mount a driver to an existing stack use the following:
```bash
mount -T io-sock /data/home/qnxuser/devs-can-vcan.so
```
