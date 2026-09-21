ifndef QCONFIG
QCONFIG=qconfig.mk
endif

include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=CAN io-sock shim exposing QNX native libcan / dev-can-* mailboxes as SocketCAN interfaces
endef

EXTRA_INCVPATH=$(PROJECT_ROOT)/../../lib/can/public/

INTERFACE_PREFIX="can"

define DRIVER_SPECIFIC_OPTIONS

config  - Path to a config file supplying the optional hints described below.
          See boards/can-libcan/libcan.conf for a working example. This
          option, and every hint below, is optional -- with no config at
          all this driver auto-discovers every native unit that exists.

By default, on load this driver scans /dev for every can<N> directory a
native dev-can-* driver has already created, and for each one found:

  - discovers every rx<M> and tx<M> mailbox file inside it
  - opens every rx<M> file and widens its message filter (CAN_DEVCTL_SET_MFILTER)
    so it is not restricted to whatever single message ID it was nominally
    configured with, then spawns one dedicated blocking-read thread per file
    (poll/select are not supported against these mailbox files)
  - opens the lowest-numbered tx<M> file and uses it for every outgoing
    frame regardless of CAN ID (CAN_DEVCTL_WRITE_CANMSG_EXT)
  - registers the unit as a SocketCAN interface using the same number as the
    native unit, e.g. native /dev/can1 becomes SocketCAN can1

Two hints (hint.libcan.<unit>.<key>="value" in the config file) let this be
overridden:

  auto=[on|off]         Set on unit 0 only. Defaults to "on". Set to "off"
                        to disable the /dev scan entirely -- when off, only
                        units named via the dev hint below are attached.
  dev=<path>            Explicitly attaches native directory <path> (e.g.
                        /dev/can3) as SocketCAN unit <unit>. <unit> does not
                        need to match whatever native number is embedded in
                        <path> -- this is how to control SocketCAN interface
                        numbering directly instead of mirroring native
                        numbers, e.g. to avoid colliding with another board's
                        canN allocations. Only consulted when auto is off.

No bittiming support is provided -- the underlying dev-can-* driver already
owns bit-rate configuration for its controller.

The following sysctls are created by this driver:
hw.libcan

endef

include devs/devs.mk
