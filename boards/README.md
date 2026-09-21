# Boards

This directory contains example drivers for SocketCAN

Currently we support

| Driver                           | Implementation                | HW                                                           |
| -------------------------------- | ----------------------------- | ------------------------------------------------------------ |
| [can-vcan](./can-vcan/)          | Virtual CAN implementation    | SW                                                           |
| [can-dummy](./can-vcan/)         | Dummy CAN implementation      | SW (template for hardware drivers)                           |
| [can-libcan](./can-libcan)       | QNX's Libcan compatibly layer | SW to interface with QNX libcan hardware                     |
| [can-mcp25xxfd](./can-mcp25xxfd) | MCP25xxfd CAN FD chip         | [CAN FD HAT](https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT) |