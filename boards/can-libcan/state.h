/*
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
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

#ifndef LIBCAN_STATE_H_
#define LIBCAN_STATE_H_

#include <pthread.h>
#include <stddef.h>

#include <hw/libsockcan.h>

#define LIBCAN_MAX_UNITS 8

typedef struct libcan_dev libcan_dev_t;


typedef struct {
    libcan_dev_t *owner;
    int fd;
    pthread_t tid;
    char name[16]; // e.g. "rx0", for logging
} libcan_rx_mailbox_t;

// One native /dev/can<unit> directory, re-exposed as SocketCAN can<unit>.
struct libcan_dev {
    sockcan_dev_t dev; // Must be first
    LIST_ENTRY(libcan_dev) link;
    int unit; // libcan unit number (/dev/can[unit])

    // TODO change to LIBCAN_MAX_RX_MB
    libcan_rx_mailbox_t *rx; // malloc'd (M_DEVBUF), sized to nrx
    size_t nrx;

    // Only 1 tx node is used
    int txFd;
    char txName[16];

    // TODO remove (in dev.ifc)
    char name[16]; // "can<unit>", for logging
};

#endif // LIBCAN_STATE_H_
