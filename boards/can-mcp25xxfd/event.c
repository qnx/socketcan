/**
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "event.h"

#include <errno.h>
#include <semaphore.h>
#include <sockcan_debug.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iomsg.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/siginfo.h>
#include <unistd.h>

#include "mcp25xxfd.h"
#include "state.h"

#define CANIFC_EVENT_INTR     0
#define CANIFC_EVENT_SHUTDOWN 1
#define CAN_PRIORITY          21

/**
 * @brief CAN event thread for GPIO interrupts
 *
 * @param arg mcp25xxfd_dev_t CAN device.
 */
static void can_event_thread(void *arg) {
    mcp25xxfd_dev_t *dev = arg;
    struct _pulse    pulse;
    int              sival;
    bool             running = true;

    while (running) {
        if (MsgReceivePulse(dev->chid, &pulse, sizeof(pulse), NULL) == -1) {
            break;
        }

        switch (pulse.code) {
            case CANIFC_EVENT_INTR:
                sival = pulse.value.sival_int;
                mcp25xxfd_event_cb(dev, pulse.code, sival);

                if (pulse.code == CANIFC_EVENT_INTR) {
                    InterruptUnmask(dev->irq, dev->iid);
                }
                break;
            case CANIFC_EVENT_SHUTDOWN:
                running = false;
                break;
        }
    }

    ConnectDetach(dev->coid);
    ChannelDestroy(dev->chid);
    kthread_exit();
}

/**
 * @brief register interrupt callback.
 *
 * @param arg mcp25xxfd_dev_t CAN device,
 *
 * @return 0 if success, otherwise an associated error code.
 */
int can_register_event(mcp25xxfd_dev_t *dev) {
    struct sigevent ev;
    /*
     * One thread per interface. dev->tid is zero-initialized (static storage) until
     * pthread_create below sets it, so a non-zero value here means a thread already
     * exists for this instance.
     */
    if (dev->tid != 0) {
        return EAGAIN;
    }

    // Created here, before the event thread is spawned, so dev->coid is
    // guaranteed valid by the time SIGEV_PULSE_INIT below uses it.
    dev->chid = ChannelCreate(_NTO_CHF_PRIVATE | _NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK);
    dev->coid = ConnectAttach(0, 0, dev->chid, _NTO_SIDE_CHANNEL, 0);

    // Create the event thread using KTHREAD allows pthread_join to be used
    struct thread *newtd;
    if (kthread_add(can_event_thread, dev, &proc0, &newtd, KTHREAD_JOIN, 0, "mcp25xxfd_intr") != 0) {
        return errno;
    }
    dev->tid = (pthread_t)newtd->td_pt_tid;

    SIGEV_PULSE_INIT(&ev, dev->coid, CAN_PRIORITY, CANIFC_EVENT_INTR, dev->irq);
    dev->iid = InterruptAttachEvent(dev->irq, &ev, _NTO_INTR_FLAGS_TRK_MSK);
    if (dev->iid == -1) {
        int err = errno;
        CAN_LOG_ERR("%s: InterruptAttach failed(%d)", __func__, err);
        return err;
    }

    return 0;
}

void can_unregister_event(mcp25xxfd_dev_t *dev) {
    if (dev->tid == 0) {
        // can_register_event was never called (or this already ran) for this instance.
        return;
    }

    InterruptDetach(dev->iid);
    MsgSendPulse(dev->coid, CAN_PRIORITY, CANIFC_EVENT_SHUTDOWN, 0);
    pthread_join(dev->tid, NULL);
    dev->tid = 0;
}
