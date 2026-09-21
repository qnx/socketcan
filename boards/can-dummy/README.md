# can-dummy

A fake CAN *hardware* driver, meant as a template for a real one. Where can-vcan is a pseudo device created for local traffic through a cloner, can-dummy represents how a physical device would register and use the CAN module.

## Overview

On load it creates `canN` and starts a periodic `callout` that stands in for the chip's RX interrupt, delivering a frame a fixed interval via `sockcan_output()`.

```bash
SOCK=/can ./can-read can0
Read a CAN frame (sz=16) (ts=1784681209(s)) {id:0x123, data:0x0000000000000002}
Read a CAN frame (sz=16) (ts=1784681210(s)) {id:0x123, data:0x0000000000000003}
```

## Configuration
can-dummy relies on a configuration file to determine what CAN devices should be created. By default no CAN devices are created unless specified in the configuration file.

A sample configuration has been given at [candummy.conf](./candummy.conf) which create 2 can devices 1 Classic and one FD device. where each devices has 3 options show below. For more information see sample config
```sh
hint.dummy.<unit>.create="true"    # Required to create this unit as can<unit>
hint.dummy.<unit>.fd="true"        # emit CAN FD frames instead of Classic
hint.dummy.<unit>.rate_ms="500"    # emission period in ms (default 1000)
```

## Running

```bash
io-sock -m /data/home/qnxuser/mods-can.so -d /data/home/qnxuser/devs-can-dummy.so -o prefix=/can,config=candummy.conf

# The device must be brought up before it can be used
SOCK=/can ifconfig can0 up
```
