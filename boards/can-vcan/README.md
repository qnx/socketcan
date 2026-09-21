# can-vcan

A virtual CAN device for SocketCAN. It has no hardware behind it, so it is registered
as a plain module rather than a bus driver, and every interface it provides comes from
an `if_clone` cloner.

## Creating an interface

After the module is loaded devices can be created though ifconfig as follows
```bash
SOCK=/can ifconfig vcan create
SOCK=/can ifconfig vcan0 up
```

Which will let the io-sock system pick the unit number itself (normally 0)


To create a specific device append the desired unit number
```bash
SOCK=/can ifconfig vcan4 create up
```
would create vcan4 and start it in one command.


To remove an interface run
```bash
SOCK=/can ifconfig vcan0 destroy
```

A created interface comes up with an MTU of `CAN_MTU` (16, the size of a
`struct can_frame`):

```bash
SOCK=/can ifconfig vcan0
vcan0: flags=1<UP> metric 0 mtu 16
	groups: vcan
	nd6 options=21<PERFORMNUD,AUTO_LINKLOCAL>
```
