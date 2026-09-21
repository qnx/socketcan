/*
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
 *
 * BlackBerry Limited and its licensors  retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from BlackBerry Limited
 * is strictly prohibited.
 */

/*
 * vcan driver implementation.
 *
 * vcan is a virtual CAN interface: it has no hardware, so a "transmit" is simply
 * echoed back to local listeners via the sockcan loopback path. Interfaces are
 * created on demand as an if_clone cloner ("ifconfig vcanN create"), mirroring
 * Linux. The io-sock module registration that drives this lives in vcan_module.c.
 */

#include <sys/sockio.h>

#include "vcan.h"

#include <hw/libsockcan.h>
#include <sockcan_debug.h>
#include <hw/socketcan.h>

static struct if_clone *s_vcan_cloner;

static int vcan_transmit(struct ifnet *ifp, struct mbuf *m) {
    CAN_TRACE();
    sockcan_log_frame("vcan_transmit", ifp, m);
    m_freem(m);
    // Nothing is deferred, so the frame is done the moment we return.
    if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
    return 0;
}

static void vcan_qflush(struct ifnet *ifp) {
}

/*
 * if_ioctl for vcan. Supports
 *   SIOCSIFFLAGS  start/stop the hardware, then track IFF_DRV_RUNNING
 *   SIOCSIFMTU    CAN_MTU turns CAN FD off, CANFD_MTU turns it on
 */
static int vcan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data) {
    struct ifreq  *ifr = (struct ifreq *)data;
    sockcan_dev_t *dev = ifp->if_softc;

    switch (cmd) {
        case SIOCSIFFLAGS:
            if (ifp->if_flags & IFF_UP) {
                ifp->if_drv_flags |= IFF_DRV_RUNNING;
            } else {
                ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
            }
            break;
        case SIOCSIFMTU:
            // Classic CAN is always valid; CAN FD only if this device supports it.
            if (ifr->ifr_mtu == CAN_MTU) {
                ifp->if_mtu = CAN_MTU;
                dev->enabled_caps &= ~CANIFC_CAP_CANFD;
            } else if (ifr->ifr_mtu == CANFD_MTU && (dev->caps & CANIFC_CAP_CANFD)) {
                ifp->if_mtu = CANFD_MTU;
                dev->enabled_caps |= CANIFC_CAP_CANFD;
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

static int vcan_create(struct if_clone *ifc, char *name, size_t len, struct ifc_data *ifd, struct ifnet **ifpp) {
    // vcan has state so just use sockcan_dev_t
    sockcan_dev_t *dev = sockcan_alloc("vcan", ifd, sizeof(sockcan_dev_t));
    if (dev == NULL) {
        return errno;
    }
    dev->ifp->if_transmit = vcan_transmit;
    dev->ifp->if_qflush   = vcan_qflush;
    dev->ifp->if_ioctl    = vcan_ioctl;
    dev->caps |= CANIFC_CAP_CANFD;

    *ifpp = dev->ifp;
    return sockcan_register(dev);
}

static int vcan_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags) {
    if (ifp->if_dunit == 0 && (flags & IFC_F_FORCE) == 0) return (EINVAL);

    sockcan_dev_t *dev = ifp->if_softc;
    // A bound socket keeps a pointer to this device, so refuse to tear it down
    // while any are still attached rather than leave them dangling.
    if (dev != NULL) {
        mtx_lock(&dev->mtx);
        int busy = !LIST_EMPTY(&dev->connected_sockets);
        mtx_unlock(&dev->mtx);
        if (busy) {
            return EBUSY;
        }
    }

    if_detach(ifp);
    if_free(ifp);
    if (dev != NULL) {
        sockcan_fini(dev);
    }
    return (0);
}

int vcan_attach(void) {
    CAN_TRACE();
    /**
     * Creates a clone device so vcan devices can be created with
     * ifconfig vcan create
     */
    struct if_clone_addreq req = {
        .create_f  = vcan_create,
        .destroy_f = vcan_destroy,
        .flags     = IFC_F_AUTOUNIT,
    };
    s_vcan_cloner = ifc_attach_cloner("vcan", &req);
    return (s_vcan_cloner != NULL) ? 0 : ENXIO;
}

void vcan_detach(void) {}
