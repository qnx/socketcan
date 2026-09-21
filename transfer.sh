#!/bin/bash
TARGET_IP=${TARGET_IP:-qnxpi.local}
TARGET_USER=qnxuser

ssh ${TARGET_USER}@${TARGET_IP} mkdir -p SocketCAN
scp lib/can/nto/aarch64/dll.le/mods-can.so \
    boards/can-vcan/nto/aarch64/dll.le/devs-can-vcan.so \
    boards/can-dummy/nto/aarch64/dll.le/devs-can-dummy.so \
    boards/can-mcp25xxfd/nto/aarch64/dll.le/devs-can-mcp25xxfd.so \
    boards/can-libcan/nto/aarch64/dll.le/devs-can-libcan.so \
    sample/can-read/nto/aarch64/o.le/can-read \
    sample/can-write/nto/aarch64/o.le/can-write \
    apps/canctrl/nto/aarch64/o.le/canctrl \
    boards/can-dummy/candummy.conf \
    boards/can-mcp25xxfd/etc/rpi4-mcp25xxfd.conf \
    boards/can-mcp25xxfd/etc/rpi5-mcp25xxfd.conf \
    boards/can-libcan/libcan.conf \
    ${TARGET_USER}@${TARGET_IP}:SocketCAN
