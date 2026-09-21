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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <devctl.h>
#include <sys/can_dcmd.h>
#include <sys/sockio.h>

#include <hw/libsockcan.h>
#include <hw/socketcan.h>
#include <sockcan_debug.h>

#include <sys/kthread.h>
#include <sys/proc.h>

#include "libcan.h"
#include "state.h"

/**
 * This module defines a libcan compatibility layer exposing it as an io-sock SocketCAN device.
 *
 * The module will by default find all CAN devices on the system and create a matching interface.
 * This interface will listen to each of the rx nodes in a separate thread to work with libcan interfaces
 * which don't support poll/select(). It is also important that one one else is using the libcan device
 * as it only supports one client per mailbox.
 *
 * @note one limitation of this implementation it is will only ever send to one tx mailbox
 */

// close is not defined in the unistd.h but the symbol is still in libc so redefine it here
extern int close(int fd);

// Used to walk the devices on shutdown
static LIST_HEAD(, libcan_dev) s_devs = LIST_HEAD_INITIALIZER(s_devs);

#define LIBCAN_MID_SHIFT 18

/**
 * @brief Checks if a mailbox has a numerical suffix.
 *
 * @param name Mailbox name.
 * @param prefix Mailbox prefix (rx or tx).
 * @param[out] out_num Mailbox number or untouched if False is returned.
 *
 * @return  True if name is prefix followed by one or more digits (e.g. "rx3" against prefix "rx")
 */
static bool has_numeric_suffix(const char *name, const char *prefix, long *out_num) {
    size_t prefix_len = strlen(prefix);
    if (strncmp(name, prefix, prefix_len) != 0 || strcmp(name, prefix) == 0) {
        // No numerical value or prefix doesn't match name
        return false;
    }

    const char *digits = name + prefix_len;
    char* end_char = NULL;
    *out_num = strtol(digits, &end_char, 10);
    return (end_char != NULL && *end_char == '\0');
}

/*
 * Scans /dev for every can<N> directory
 */
static int libcan_discover_units(int *units, size_t max_units, size_t *out_n) {
    DIR *d = opendir("/dev");
    if (d == NULL) {
        return errno;
    }

    *out_n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        long num;
        if (!has_numeric_suffix(de->d_name, "can", &num)) {
            continue;
        }

        if (*out_n >= max_units) {
            CAN_LOG_WRN("More than %zu native can<N> units found, ignoring: can%ld", max_units, num);
            continue;
        }
        units[*out_n] = (int)num;
        *out_n += 1;
    }
    closedir(d);

    return EOK;
}

/**
 * @brief Scans a given canX directory for all mailboxes
 *
 * @param can_dir Path to canX dir.
 * @param ifc libcan interface.
 */
static int libcan_scan_mailboxes(const char *can_dir, libcan_dev_t *ifc) {
    DIR *d = opendir(can_dir);
    if (d == NULL) {
        return errno;
    }

    // Find all of the mailboxes only grabbing the first tx mailbox we see.
    size_t nrx = 0;
    long tx_mailbox = -1;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        long num;
        if (has_numeric_suffix(de->d_name, "rx", &num)) {
            nrx++;
        } else if (tx_mailbox == -1 && has_numeric_suffix(de->d_name, "tx", &num)) {
            tx_mailbox = num;
        }
    }
    closedir(d);

    if (nrx == 0 || tx_mailbox == -1) {
        CAN_LOG_WRN("%s: No usable rx/tx mailboxes found (rx count=%zu, tx found=%s)",
                    can_dir, nrx, tx_mailbox == -1 ? "no" : "yes");
        return ENODEV;
    }

    libcan_rx_mailbox_t *rx = malloc(nrx * sizeof(libcan_rx_mailbox_t), M_DEVBUF, M_WAITOK | M_ZERO);
    if (rx == NULL) {
        return ENOMEM;
    }

    // Now that we have found all the mailboxes copy it to the ifc state
    snprintf(ifc->txName, sizeof(ifc->txName), "tx%ld", tx_mailbox);
    for (int i = 0; i < nrx; i++) {
        // Set all fd's to -1 before opening any so a failure doesn't cause fd=0 to be closed
        rx[i].fd = -1;
        snprintf(rx[i].name, sizeof(rx[i].name), "rx%d", i);
        rx[i].owner = ifc;
    }
    ifc->rx = rx;
    ifc->nrx = nrx;
    return EOK;
}

/**
 * @brief libcan reader thread
 *
 * We can't confirm all libcan interfaces implement poll a reader thread is used per rx mailbox.
 * These mailbox have no filtering on them we are relying on SocketCAN doing the filtering in software
 *
 */
static void libcan_reader_thread(void *arg) {
    libcan_rx_mailbox_t *mbox = arg;
    libcan_dev_t *ifc = mbox->owner;
    CAN_MSG msg;
    int rc;

    while (true) {
        rc = devctl(mbox->fd, CAN_DEVCTL_READ_CANMSG_EXT, &msg, sizeof(msg), NULL);
        if (rc != EOK) {
            CAN_LOG_NTC("[%s/%s]: Reader thread exiting. Error=%s", ifc->dev.ifp->if_xname, mbox->name, strerror(rc));
            break;
        }

        bool is_ext = msg.ext.is_extended_mid != 0;
        bool is_rtr = msg.ext.is_remote_frame != 0;

        // We have no way of knowing if it is a FD frame so just try our best based on the msg size.
        bool is_fd = msg.len > CAN_MSG_DATA_MAX_CAN;

        canid_t can_id;
        if (is_ext) {
            // Extended message flag the can_id
            can_id = (msg.mid & CAN_EFF_MASK) | CAN_EFF_FLAG;
        } else {
            can_id = (msg.mid >> LIBCAN_MID_SHIFT) & CAN_SFF_MASK;
        }
        if (is_rtr) {
            can_id |= CAN_RTR_FLAG;
        }

        struct mbuf *m = m_gethdr(M_NOWAIT, MT_DATA);
        if (m == NULL) {
            CAN_LOG_ERR("[%s/%s]: Unable to allocate mbuf for received frame. Frame dropped.", ifc->dev.ifp->if_xname, mbox->name);
            if_inc_counter(ifc->dev.ifp, IFCOUNTER_IQDROPS, 1);
            continue;
        }

        if (is_fd) {
            canfd_frame_t frame = {0};
            frame.can_id = can_id;
            frame.len = msg.len;
            memcpy(frame.data, msg.dat, msg.len);
            memcpy(mtod(m, void *), &frame, sizeof(frame));
            m->m_len = m->m_pkthdr.len = sizeof(frame);
        } else {
            can_frame_t frame = {0};
            frame.can_id = can_id;
            frame.len = msg.len;
            memcpy(frame.data, msg.dat, msg.len);
            memcpy(mtod(m, void *), &frame, sizeof(frame));
            m->m_len = m->m_pkthdr.len = sizeof(frame);
        }

        can_tag_data_t tag = {.flags = is_fd ? CANMSG_FLAG_FD_FRAME : 0, .sender = NULL};
        sockcan_output(&ifc->dev, m, &tag);
    }

    kthread_exit();
}

/**
 * @brief Transmits a message to libcan.
 *
 * @note libcan already will have internal queuing in the resource manager, so no reason to duplicate it here.
 *       Just provide the message directly to the interface.
 *
 * @param ifp Target interface.
 * @param m CAN Message.
 */
static int libcan_transmit(struct ifnet *ifp, struct mbuf *m) {
    libcan_dev_t *ifc = ifp->if_softc;
    canfd_frame_t frame = {0};
    CAN_MSG msg = {0};
    int rc;

    memcpy(&frame, mtod(m, void *), m->m_len);
    bool is_ext = (frame.can_id & CAN_EFF_FLAG) != 0;
    if (is_ext) {
        msg.mid = frame.can_id & CAN_EFF_MASK;
    } else {
        msg.mid = (frame.can_id & CAN_SFF_MASK) << LIBCAN_MID_SHIFT;
    }
    msg.ext.is_extended_mid = is_ext ? 1 : 0;
    msg.ext.is_remote_frame = ((frame.can_id & CAN_RTR_FLAG) != 0 ) ? 1 : 0;
    msg.len = frame.len;
    memcpy(msg.dat, frame.data, frame.len);

    rc = devctl(ifc->txFd, CAN_DEVCTL_WRITE_CANMSG_EXT, &msg, sizeof(msg), NULL);
    if (rc == EAGAIN) {
        CAN_LOG_WRN("[%s/%s]: tx mailbox full. Frame dropped.", ifc->dev.ifp->if_xname, ifc->txName);
        rc = ENOBUFS;
    } else if (rc != EOK) {
        CAN_LOG_ERR("[%s/%s]: Unable to write frame. Error=%s", ifc->dev.ifp->if_xname, ifc->txName, strerror(rc));
    } else {
        if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
        rc = EOK;
    }

    m_freem(m);
    return rc;
}

/**
 * See note in @libcan_transmit of why no queue is used
 */
static void libcan_qflush(struct ifnet *ifp) {
}

/**
 * @brief ioctl messages for libcan
 *
 * @note because this is just a translation layer there is no hardware to start so just pass along messages.
 */
static int libcan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data) {
    struct ifreq *ifr = (struct ifreq *)data;
    libcan_dev_t *ifc = ifp->if_softc;

    switch (cmd) {
        case SIOCSIFFLAGS:
            if (ifp->if_flags & IFF_UP) {
                ifp->if_drv_flags |= IFF_DRV_RUNNING;
            } else {
                ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
            }
            break;
        case SIOCSIFMTU:
            if (ifr->ifr_mtu == CAN_MTU) {
                ifp->if_mtu = CAN_MTU;
                ifc->dev.enabled_caps &= ~CANIFC_CAP_CANFD;
            } else if (ifr->ifr_mtu == CANFD_MTU && (ifc->dev.caps & CANIFC_CAP_CANFD)) {
                ifp->if_mtu = CANFD_MTU;
                ifc->dev.enabled_caps |= CANIFC_CAP_CANFD;
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
 * @brief shut down libcan shim.
 */
static void libcan_teardown_one(libcan_dev_t *ifc) {
    for (size_t i = 0; i < ifc->nrx; i++) {
        if (ifc->rx[i].fd != -1) {
            close(ifc->rx[i].fd); // unblocks the reader thread's blocked devctl
            ifc->rx[i].fd = -1;
        }
        if (ifc->rx[i].tid != 0) {
            pthread_join(ifc->rx[i].tid, NULL);
            ifc->rx[i].tid = 0;
        }
    }
    if (ifc->rx != NULL) {
        free(ifc->rx, M_DEVBUF);
        ifc->rx = NULL;
        ifc->nrx = 0;
    }
    if (ifc->txFd != -1) {
        close(ifc->txFd);
        ifc->txFd = -1;
    }
}

/*
 * hint.libcan.<unit>.fd="on" marks a unit as CAN FD capable. It has to be
 * configured rather than discovered: CAN_DEVCTL_GET_INFO carries no FD field,
 * so the native API offers no way to ask.
 */
static bool libcan_hint_fd(int unit) {
    const char *val;
    if (resource_string_value("libcan", unit, "fd", &val) != 0 || val == NULL) {
        return false;
    }
    return strcmp(val, "on") == 0;
}

/**
 * @brief Creates a libcan interface for a given directory.
 *
 * @param unit What SocketCAN unit the interface will be.
 * @param can_dir libcan can directory (/dev/canX).
 */
static libcan_dev_t *libcan_attach_one(int unit, const char *can_dir) {
    char path[64];
    int rc;

    struct ifc_data ifd = {.unit = unit};
    libcan_dev_t *ifc = (libcan_dev_t *)sockcan_alloc("can", &ifd, sizeof(*ifc));
    if (ifc == NULL) {
        CAN_LOG_ERR("can%d: sockcan_alloc failed", unit);
        return NULL;
    }

    ifc->txFd = -1;

    if ((rc = libcan_scan_mailboxes(can_dir, ifc)) != EOK) {
        CAN_LOG_WRN("%s: Skipping. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        goto exit;
    }

    for (size_t i = 0; i < ifc->nrx; i++) {
        snprintf(path, sizeof(path), "%s/%s", can_dir, ifc->rx[i].name);
        ifc->rx[i].fd = open(path, O_RDWR);
        if (ifc->rx[i].fd == -1) {
            rc = errno;
            CAN_LOG_ERR("%s: open(%s) failed. Error=%s", ifc->dev.ifp->if_xname, path, strerror(rc));
            goto exit;
        }

        uint32_t wildcard = 0x0u;
        if ((rc = devctl(ifc->rx[i].fd, CAN_DEVCTL_SET_MFILTER, &wildcard, sizeof(wildcard), NULL)) != EOK) {
            CAN_LOG_ERR("%s: SET_MFILTER(%s) failed. Error=%s", ifc->dev.ifp->if_xname, path, strerror(rc));
            goto exit;
        }

        struct thread *newtd;
        if (kthread_add(libcan_reader_thread, &ifc->rx[i], &proc0, &newtd, KTHREAD_JOIN, 0,
                         "libcan_%s_%s", ifc->dev.ifp->if_xname, ifc->rx[i].name) != 0) {
            rc = errno;
            CAN_LOG_ERR("%s: kthread_add(%s) failed. Error=%s", ifc->dev.ifp->if_xname, path, strerror(rc));
            goto exit;
        }
        ifc->rx[i].tid = (pthread_t)newtd->td_pt_tid;
    }

    snprintf(path, sizeof(path), "%s/%s", can_dir, ifc->txName);
    ifc->txFd = open(path, O_RDWR);
    if (ifc->txFd == -1) {
        rc = errno;
        CAN_LOG_ERR("%s: open(%s) failed. Error=%s", ifc->dev.ifp->if_xname, path, strerror(rc));
        goto exit;
    }

    // bit timings are ignored because they are already configured in libcan.
    ifc->dev.ifp->if_transmit = libcan_transmit;
    ifc->dev.ifp->if_qflush = libcan_qflush;
    ifc->dev.ifp->if_ioctl = libcan_ioctl;
    ifc->dev.caps = 0;
    ifc->dev.enabled_caps = 0;
    if (libcan_hint_fd(unit)) {
        ifc->dev.caps |= CANIFC_CAP_CANFD;
        ifc->dev.enabled_caps |= CANIFC_CAP_CANFD;
        ifc->dev.ifp->if_mtu = CANFD_MTU;
    }

    LIST_INSERT_HEAD(&s_devs, ifc, link);
    sockcan_register(&ifc->dev);
    return ifc;

exit:
    libcan_teardown_one(ifc);
    if_free(ifc->dev.ifp);
    sockcan_fini(&ifc->dev);
    return NULL;
}

/**
 * @brief Checks if auto-discovery is enabled.
 *
 * @note can be disabled with hint.libcan.0.auto="off"
 * @return true if enabled, false otherwise.
 */
static bool libcan_auto_enabled(void) {
    const char *val = NULL;
    if (resource_string_value("libcan", 0, "auto", &val) == 0 && val != NULL) {
        return strcmp(val, "off") != 0;
    }
    return true; // defaults to true
}

/**
 * Attaches a libcan instance to a specific SocketCAN interface.
 */
static bool libcan_hint_dev(int unit, char *dev_path, size_t dev_path_len) {
    const char *val;
    if (resource_string_value("libcan", unit, "dev", &val) != 0 || val == NULL) {
        return false;
    }
    snprintf(dev_path, dev_path_len, "%s", val);
    return true;
}

/**
 * @brief Startup libcan shim.
 *
 * There are 2 modes of operation
 * - First is auto discovery where we just find all libcan devices on startup and add them all (this is the default)
 *      This can be disabled by hint.libcan.0.auto=off
 * - Second we use hint for which devices should be mapped.
 */
int libcan_attach(void) {
    int rc;
    int nattached = 0;

    // First check if discovery is enabled.
    if (libcan_auto_enabled()) {
        int discovered[LIBCAN_MAX_UNITS];
        size_t ndiscovered = 0;

        if ((rc = libcan_discover_units(discovered, LIBCAN_MAX_UNITS, &ndiscovered)) != EOK) {
            CAN_LOG_ERR("libcan: Unable to scan /dev for can<N> units. Error=%s", strerror(rc));
            return rc;
        }
        if (ndiscovered == 0) {
            CAN_LOG_WRN("libcan: No native can<N> units found under /dev");
            return ENXIO;
        }

        for (size_t i = 0; i < ndiscovered; i++) {
            char can_dir[32];
            snprintf(can_dir, sizeof(can_dir), "/dev/can%d", discovered[i]);

            if (libcan_attach_one(discovered[i], can_dir) == NULL) {
                continue;
            }
            nattached++;
        }
    } else {
        CAN_LOG_INFO("libcan: Auto-discovery disabled (hint.libcan.0.auto=off), using explicit hints only");

        for (int unit = 0; unit < LIBCAN_MAX_UNITS; unit++) {
            char dev_path[64];
            if (!libcan_hint_dev(unit, dev_path, sizeof(dev_path))) {
                continue;
            }

            if (libcan_attach_one(unit, dev_path) == NULL) {
                continue;
            }
            nattached++;
        }
    }

    return (nattached > 0) ? EOK : ENXIO;
}

/**
 * @brief Shut down code for libcan.
 *
 * Just clean up the interfaces
 */
void libcan_detach(void) {
    libcan_dev_t *ifc;
    libcan_dev_t *tmp;

    LIST_FOREACH_SAFE (ifc, &s_devs, link, tmp) {
        mtx_lock(&ifc->dev.mtx);
        bool busy = !LIST_EMPTY(&ifc->dev.connected_sockets);
        mtx_unlock(&ifc->dev.mtx);
        if (busy) {
            CAN_LOG_WRN("%s still has bound sockets, leaving it in place", ifc->dev.ifp->if_xname);
            continue;
        }

        libcan_teardown_one(ifc);
        LIST_REMOVE(ifc, link);
        if_detach(ifc->dev.ifp);
        if_free(ifc->dev.ifp);
        sockcan_fini(&ifc->dev);
    }
}
