/*
 * Copyright (c) 2025, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MCP25xxFD_STATE_H_
#define MCP25xxFD_STATE_H_

/**
 * @file
 *
 * APIs tracking state about an MCP25xxFD
 */

#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include <hw/libsockcan.h>

#include "bit_timing.h"
#include "mcp25xxfd.h"
#include "platform.h"

enum {
    FD_MODE_OFF = 0,
    FD_MODE_ON,
    FD_MODE_NON_ISO
};

enum {
    PLATFORM_AUTO = 0,
    PLATFORM_GENERIC,
    PLATFORM_RPI4,
    PLATFORM_RPI5
};

struct mcp25xxfd_config {
    char *dev;
    int gpio;
    uint32_t osc;
    uint32_t bps;
    uint32_t dbps;
    uint32_t txFifoSize;
    uint32_t rxFifoSize;
    int fdMode;
    int platform;
};
typedef struct mcp25xxfd_config mcp25xxfd_config_t;


typedef struct mcp25xxfd_dev {
    sockcan_dev_t dev;
    LIST_ENTRY(mcp25xxfd_dev) link;
    mcp25xxfd_config_t cfg;

    // IST info
    pthread_t tid;
    int chid;
    int coid;
    int iid;
    int irq;

    int fd;

    // Update to use the bit timings in dev
    can_bit_timings_t nbt;
    can_bit_timings_t dbt;
    struct timespec epoch;

    // Bit field, bit position is the index in txLoopback
    int txSeq;
    struct mbuf *txLoopback[MCP25xxFD_MAX_FIFO_SLOTS];

    const mcp25xxfd_platform_t *platform;

    // The desired operating mode.
    int opMode;

    // TX path.
    struct buf_ring  *tx_br;
    struct taskqueue *tx_taskq;
    struct task       tx_task;
    struct mtx        io_mtx;
} mcp25xxfd_dev_t;

#define CAN_PARAM_INVALID       0xFFFFFFFFu

#endif  // MCP25xxFD_STATE_H_
