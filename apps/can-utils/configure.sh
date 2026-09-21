#!/bin/bash

if [ -z "$1" ]; then
    echo "ERROR: Need to specify arch: aarch64 or x86_64"
    exit 1
elif [ "$1" != "aarch64" -a "$1" != "x86_64" ]; then
    echo "ERROR: Invalid architecture: $1"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

EXTRA_INCVPATH=("$SCRIPT_DIR/include")

cmake \
      -DCMAKE_C_FLAGS="${EXTRA_INCVPATH[@]/#/-isystem } -include ${EXTRA_INCVPATH}/linux/can.h" \
      -DCMAKE_TOOLCHAIN_FILE=$SCRIPT_DIR/$1-qnx.cmake \
      -DBUILD_SHARED_LIBS=OFF \
      -DENABLE_IP_SERVER=OFF \
      -DENABLE_GATEWAY=OFF \
      -DENABLE_MEASUREMENT=OFF \
      -DENABLE_ISOTP=OFF \
      -DENABLE_LOG_FILE=OFF \
      -DENABLE_SLCAN=OFF \
      -DENABLE_MCP251XFD=OFF \
      -DENABLE_J1939=OFF \
      -DENABLE_ISOBUSFS=OFF \
      $SCRIPT_DIR/can-utils
