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

/*
 * Dummy CAN "hardware" driver implementation.
 *
 * This driver mimics a real hardware interface which sends a can frame
 * configured by the config file.
 */

#include <sys/sockio.h>

// SocketCAN specific includes
#include "dummy.h"

#include <hw/libsockcan.h>
#include <sockcan_debug.h>
#include <hw/socketcan.h>

/*
 * Multiple dummy "chips" can be created, each becoming its own canN interface.
 * A unit is instantiated only if its config says so, so nothing is created unless
 * the operator asks. Config is read via the unit-indexed hint API:
 *
 *   hint.dummy.<unit>.create="true"   -> create canN
 *   hint.dummy.<unit>.fd="true"       -> emit CAN FD frames instead of Classic
 *   hint.dummy.<unit>.rate_ms="500"   -> emit a frame every 500 ms (default 1000)
 *
 * (resource_string_value("dummy", unit, "create", ...) reads the kenv variable
 * hint.dummy.<unit>.create -- the "hint." prefix is supplied by the API, so the
 * config file uses it but this code does not.)
 */
#define DUMMY_MAX_UNITS 8

typedef struct dummy_dev {
    sockcan_dev_t  dev;
    LIST_ENTRY(dummy_dev) link;
    struct callout tick;
    uint32_t       seq;
    int            fd;         /* emit CAN FD frames instead of Classic */
    int            rate_ticks; /* callout period, derived from rate_ms */
} dummy_dev_t;

#define DUMMY_DEFAULT_RATE_MS 1000

// Used to walk the devices on shutdown
static LIST_HEAD(, dummy_dev) s_devs = LIST_HEAD_INITIALIZER(s_devs);

/* True iff hint.dummy.<unit>.<key> exists and is set to the string "true". */
static int dummy_hint_true(int unit, const char *key) {
    const char *val = NULL;
    if (resource_string_value("dummy", unit, key, &val) != 0 || val == NULL) {
        return 0;
    }
    return strcmp(val, "true") == 0;
}

/*
 * TX path which gets called when a new can frame should be sent to the hardware.
 * In this case we don't have any backing hardware so the message just gets printed.
 *
 * @note Depending on your device you may want to have a TX queue to hold messages when waiting to be sent.
 */
static int dummy_transmit(struct ifnet *ifp, struct mbuf *m) {
    CAN_TRACE();
    sockcan_log_frame("dummy_transmit", ifp, m);
    m_freem(m);

    // Increment how many frames have been sent to hardware
    if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
    return 0;
}

/*
 * dummy_device doesn't buffer any frames so there is nothing to flush.
 */
static void dummy_qflush(struct ifnet *ifp) {
}

/*
 * Because we are a dummy device we don't actually have any bittimings to set
 * but is left as an example of how the function can be used.
 */
static int dummy_bittiming(sockcan_dev_t *dev, can_bittiming_t *bt, bool set) {
    if (set) {
        dev->nominal_bt = *bt;
    } else {
        *bt = dev->nominal_bt;
    }
    sockcan_log_bittiming(set ? "dummy set nominal timing" : "dummy get nominal timing", dev->ifp, bt);
    return 0;
}

static int dummy_data_bittiming(sockcan_dev_t *dev, can_bittiming_t *bt, bool set) {
    if (set) {
        dev->data_bt = *bt;
    } else {
        *bt = dev->data_bt;
    }
    sockcan_log_bittiming(set ? "dummy set data timing" : "dummy get data timing", dev->ifp, bt);
    return 0;
}

/*
 * Stand-in for the chip's RX interrupt: manufacture a frame and push it up the
 * stack, then re-arm for the configured period. FD instances emit a canfd_frame with a
 * payload larger than Classic CAN allows; others emit a Classic frame.
 */
static void dummy_tick(void *arg) {
    dummy_dev_t   *in  = arg;
    can_tag_data_t tag = {0};

    // Make sure the interface is running.
    if (!(in->dev.ifp->if_drv_flags & IFF_DRV_RUNNING)) {
        return;
    }

    struct mbuf *m = m_gethdr(M_NOWAIT, MT_DATA);
    if (m != NULL) {
        uint32_t seq = in->seq++;

        if (in->fd) {
            struct canfd_frame frame = {0};
            frame.can_id             = 0x123;
            frame.len                = 16; /* > 8 bytes: only valid for CAN FD */
            memcpy(frame.data, &seq, sizeof(seq));
            memcpy(mtod(m, void *), &frame, sizeof(frame));
            m->m_len = m->m_pkthdr.len = sizeof(frame);
            tag.flags |= CANMSG_FLAG_FD_FRAME;
        } else {
            can_frame_t frame = {0};
            frame.can_id      = 0x123;
            frame.can_dlc     = sizeof(uint32_t);
            memcpy(frame.data, &seq, sizeof(seq));
            memcpy(mtod(m, void *), &frame, sizeof(frame));
            m->m_len = m->m_pkthdr.len = sizeof(frame);
        }

        sockcan_output(&in->dev, m, &tag);
    }

    // Callouts are one-shot, so the next frame only happens if we ask for it.
    callout_reset(&in->tick, in->rate_ticks, dummy_tick, in);
}

/*
 * ioctl has interface specific options such as bringing up or down.
 *
 * The main 2 options you will want to implement is SIOCSIFFLAGS and SIOCSIFMTU if CANFD is supported.
 */
static int dummy_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data) {
    CAN_TRACE();
    struct ifreq *ifr = (struct ifreq *)data;
    dummy_dev_t  *dev = ifp->if_softc;

    switch (cmd) {
        case SIOCSIFFLAGS:
            // Real hardware would be taken in and out of its operating mode here.
            if (ifp->if_flags & IFF_UP) {
                callout_reset(&dev->tick, dev->rate_ticks, dummy_tick, dev);
                ifp->if_drv_flags |= IFF_DRV_RUNNING;
            } else {
                ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
                callout_stop(&dev->tick);
            }
            break;
        case SIOCSIFMTU:
            // Only a unit configured as FD advertises CANIFC_CAP_CANFD, so a
            // Classic one rejects the FD MTU here rather than pretending.
            if (ifr->ifr_mtu == CAN_MTU) {
                ifp->if_mtu = CAN_MTU;
                dev->dev.enabled_caps &= ~CANIFC_CAP_CANFD;
                dev->fd = 0;
            } else if (ifr->ifr_mtu == CANFD_MTU && (dev->dev.caps & CANIFC_CAP_CANFD)) {
                ifp->if_mtu = CANFD_MTU;
                dev->dev.enabled_caps |= CANIFC_CAP_CANFD;
                dev->fd = 1;
            } else {
                return EINVAL;
            }
            break;
        case SIOCADDMULTI:
        case SIOCDELMULTI:
            break;
        default:
            return EINVAL;
    }

    return 0;
}

/*
 * Start candummy. this will start a callback to send a can frame a configured amount.
 */
int candummy_attach(void) {
    CAN_TRACE();

    int ncreated = 0;

    for (int unit = 0; unit < DUMMY_MAX_UNITS; unit++) {
        if (!dummy_hint_true(unit, "create")) {
            continue;
        }

        struct ifc_data ifd = {.unit = unit};
        dummy_dev_t *dev    = (dummy_dev_t *)sockcan_alloc("can", &ifd, sizeof(*dev));
        if (dev == NULL) {
            CAN_LOG_ERR("sockcan_alloc failed for unit %d", unit);
            candummy_detach();
            return ENOMEM;
        }

        dev->fd = dummy_hint_true(unit, "fd");

        // Emission period. rate_ms -> callout ticks (hz ticks per second),
        // clamped to at least one tick.
        int rate_ms = DUMMY_DEFAULT_RATE_MS;
        resource_int_value("dummy", unit, "rate_ms", &rate_ms);
        dev->rate_ticks = (int)(((int64_t)rate_ms * hz) / 1000);
        if (dev->rate_ticks < 1) {
            dev->rate_ticks = 1;
        }

        dev->dev.ifp->if_transmit = dummy_transmit;
        dev->dev.ifp->if_qflush   = dummy_qflush;
        dev->dev.ifp->if_ioctl    = dummy_ioctl;
        dev->dev.bittiming        = dummy_bittiming;
        dev->dev.data_bittiming   = dummy_data_bittiming;
        if (dev->fd) {
            // Advertise FD capability and use the FD MTU so this is a real FD
            // interface (see the SIOCSIFMTU gate in dummy_ioctl).
            dev->dev.caps |= CANIFC_CAP_CANFD;
            dev->dev.enabled_caps |= CANIFC_CAP_CANFD;
            dev->dev.ifp->if_mtu = CANFD_MTU;
        }

        // Create the timer but this wont be started till the device is brought up.
        callout_init(&dev->tick, 1);

        LIST_INSERT_HEAD(&s_devs, dev, link);
        sockcan_register(&dev->dev);

        CAN_LOG_INFO("Created can%d (%s) every %d tick(s)", unit, dev->fd ? "FD" : "Classic", dev->rate_ticks);
        ncreated++;
    }

    return (ncreated > 0) ? 0 : ENXIO;
}

/*
 * Called by the module at unload.
 */
void candummy_detach(void) {
    dummy_dev_t *dev;
    dummy_dev_t *tmp;

    LIST_FOREACH_SAFE (dev, &s_devs, link, tmp) {
        // A bound socket keeps a pointer to this device; freeing it out from
        // under a still-open socket would leave that pointer dangling, so
        // leave the instance in place rather than tear it down while busy.
        mtx_lock(&dev->dev.mtx);
        int busy = !LIST_EMPTY(&dev->dev.connected_sockets);
        mtx_unlock(&dev->dev.mtx);
        if (busy) {
            CAN_LOG_WRN("%s still has bound sockets, leaving it in place", dev->dev.ifp->if_xname);
            continue;
        }

        callout_drain(&dev->tick);
        LIST_REMOVE(dev, link);
        if_detach(dev->dev.ifp);
        if_free(dev->dev.ifp);
        sockcan_fini(&dev->dev);
    }
}
