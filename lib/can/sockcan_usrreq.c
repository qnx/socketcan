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

#include "sockcan_usrreq.h"

#include "hw/libsockcan.h"
#include "sockcan_debug.h"
#include "sockcan_domain.h"

#define IFC_IS_CAN_MTU(_ifp) (_ifp->if_mtu == CAN_MTU || _ifp->if_mtu == CANFD_MTU)

/**
 * Uses a 0 mask to allow all frames.
 *
 * SocketCAN docs mention by default all non error frames should be expected.
 * @note error frames are filtered by CAN_ERROR_FILTER.
 */
struct can_filter g_default_can_filter = {.can_id = 0, .can_mask = 0};

static void sockcan_unbind(struct can_pcb *pcb);

static int sockcan_bind(struct socket *so, struct sockaddr *nam, struct thread *td) {
    CAN_TRACE();
    // This is where we will need to find and decide the sockcan interface which this connection should
    // be attached to

    struct can_pcb      *pcb  = sotocanpcb(so);
    struct sockaddr_can *addr = (struct sockaddr_can *)nam;

    if (nam->sa_len != sizeof(struct sockaddr_can) || addr->can_family != AF_CAN) {
        CAN_LOG_DEBUG("bind() rejected len=%d family=%d", nam->sa_len, addr->can_family);
        return EINVAL;
    }

    /**
     * ifindex=0 is a special case where you allow and send to all devices
     */
    if (addr->can_ifindex == CAN_IFC_BIND_ANY) {
        // Wild card bind (receives messages from all CAN devices)
        // Note to write sendto() must be used to target a specific device
        //
        // If it was previously bound, unbind it first.
        sockcan_unbind(pcb);
        pcb->device = NULL;
        pcb->ifc    = CAN_IFC_BIND_ANY;
        sockcan_any_insert(pcb);
    } else {
        struct ifnet *net = ifnet_byindex_ref(addr->can_ifindex);
        if (net == NULL) {
            return ENODEV;
        }
        /**
         * We cannot validate that this is a CAN interface using the IFT number so do some sanity checks
         * to validate using the MTU
         */
        if (net->if_softc == NULL || net->if_type != IFT_OTHER || !IFC_IS_CAN_MTU(net)) {
            CAN_LOG_DEBUG("bind() rejected: %s is not CAN device", net->if_xname);
            if_rele(net);
            return EAFNOSUPPORT;
        }

        // The interface must be administratively up before a socket can bind to
        // it, matching `ifconfig <if> up`. Bring it up first, or bind fails here.
        if (!(net->if_flags & IFF_UP)) {
            CAN_LOG_DEBUG("bind() rejected: %s is down", net->if_xname);
            if_rele(net);
            return ENETDOWN;
        }

        // If it was previously bound, unbind it first.
        sockcan_unbind(pcb);

        // Reference is held for the lifetime of the binding and released in detach/close.
        pcb->ifc    = addr->can_ifindex;
        pcb->device = net->if_softc;

        mtx_lock(&pcb->device->mtx);
        LIST_INSERT_HEAD(&pcb->device->connected_sockets, pcb, dev_socks);
        mtx_unlock(&pcb->device->mtx);
    }

    // Move the socket into the connected state.
    soisconnected(so);

    return 0;
}

/**
 * Detach a socket from the device it was bound to: drop it from the device's socket
 * list, release the interface reference taken in bind, and mark it unbound. Safe to
 * call more than once (close then detach) and on a socket that was never bound.
 */
static void sockcan_unbind(struct can_pcb *pcb) {
    sockcan_dev_t *dev = pcb->device;
    if (dev != NULL) {
        mtx_lock(&dev->mtx);
        LIST_REMOVE(pcb, dev_socks);
        mtx_unlock(&dev->mtx);
        if_rele(dev->ifp);
        pcb->device = NULL;
    } else if (pcb->ifc == CAN_IFC_BIND_ANY) {
        // Bind-any sockets have no device; they live on the global list instead.
        sockcan_any_remove(pcb);
    }
    pcb->ifc = CAN_IFC_BIND_UNBOUND;
}

static void sockcan_close(struct socket *so) {
    CAN_TRACE();
    sockcan_unbind(sotocanpcb(so));
    soisdisconnected(so);
}

/**
 * When a new socket is connected
 */
static int sockcan_attach(struct socket *so, int proto, struct thread *td) {
    CAN_TRACE();
    soreserve(so, 20 * 1024, 20 * 1024);
    struct can_pcb *pcb = malloc(sizeof(struct can_pcb), M_DEVBUF, M_WAITOK | M_ZERO);
    if (pcb == NULL) {
        CAN_LOG_ERR("Failed to alloc sockcan PCB");
        return ENOMEM;
    }
    pcb->so        = so;
    pcb->ifc       = CAN_IFC_BIND_UNBOUND;
    pcb->sock_opts = SOCK_OPTS_LOOPBACK;

    // By default there is one filter that accepts all non-error frames.
    pcb->nfilter = 1;
    pcb->pfilter = &g_default_can_filter;

    so->so_pcb = pcb;
    return 0;
}

static void sockcan_detach(struct socket *so) {
    CAN_TRACE();
    struct can_pcb *pcb = sotocanpcb(so);
    if (pcb == NULL) {
        return;
    }

    // close is not guaranteed to have run, so make sure the binding is gone first.
    sockcan_unbind(pcb);
    so->so_pcb = NULL;

    if (pcb->pfilter != &g_default_can_filter) {
        free(pcb->pfilter, M_DEVBUF);
        pcb->pfilter = NULL;
    }

    free(pcb, M_DEVBUF);
}

/**
 * Validates a frame from a client before any driver sees it.
 */
static int sockcan_validate_frame(const void *buf, bool is_fd) {
    const can_frame_t *f  = buf;
    canid_t            id = f->can_id;

    // Error frames are reported by the stack, never transmitted by a client.
    if (id & CAN_ERR_FLAG) {
        return EINVAL;
    }

    // The id has to fit the width its EFF flag claims.
    canid_t idmask = (id & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK;
    if (id & ~(idmask | CAN_EFF_FLAG | CAN_RTR_FLAG)) {
        return EINVAL;
    }

    // Bounds what a driver may copy out of data[].
    if (is_fd) {
        if (((const canfd_frame_t *)buf)->len > CANFD_MAX_DLEN) {
            return EMSGSIZE;
        }
    } else if (f->len > CAN_MAX_DLEN) {
        return EMSGSIZE;
    }

    return 0;
}

static int sockcan_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr, struct mbuf *control,
                        struct thread *td) {
    CAN_TRACE();

    if (m == NULL) {
        CAN_LOG_ERR("Null m");
        return EINVAL;
    }

    struct can_pcb *pcb = sotocanpcb(so);

    // Resolve the interface to transmit on. A socket bound to a specific device
    // uses that one. A bind-any (ifindex 0) socket must name the target with
    // sendto(); look it up and hold an ifnet reference for the duration of the
    // send, released before returning.
    sockcan_dev_t *dev  = pcb->device;
    struct ifnet  *held = NULL;
    int            err  = 0;
    if (dev == NULL) {
        if (pcb->ifc != CAN_IFC_BIND_ANY) {
            CAN_LOG_ERR("Socket not bound to device");
            err = ENOTCONN;
            goto error;
        }
        struct sockaddr_can *dst = (struct sockaddr_can *)addr;
        if (dst == NULL) {
            CAN_LOG_ERR("Bind-any send needs a destination interface (use sendto)");
            err = EDESTADDRREQ;
            goto error;
        }
        held = ifnet_byindex_ref(dst->can_ifindex);
        if (held == NULL) {
            err = ENODEV;
            goto error;
        }
        if (held->if_type != IFT_OTHER || held->if_softc == NULL || !IFC_IS_CAN_MTU(held)) {
            err = EAFNOSUPPORT;
            goto error;
        }
        if (!(held->if_flags & IFF_UP)) {
            err = ENETDOWN;
            goto error;
        }
        dev = held->if_softc;
    }

    // Validate the frame size against the socket and the resolved interface.
    if (m->m_len == (int)CANFD_MTU) {
        // A CAN FD frame requires both the socket to have enabled CAN FD
        // (CAN_RAW_FD_FRAMES) and the interface to be in FD mode (MTU 72).
        if (!(pcb->sock_opts & SOCK_OPTS_FD_FRAMES)) {
            CAN_LOG_ERR("CAN FD frame on a socket without CAN_RAW_FD_FRAMES");
            err = EBADMSG;
            goto error;
        } else if (dev->ifp->if_mtu != CANFD_MTU) {
            CAN_LOG_ERR("CAN FD frame but %s is not in FD mode", dev->ifp->if_xname);
            err = EMSGSIZE;
            goto error;
        }

        mtod(m, canfd_frame_t *)->flags |= CANFD_FLAGS_FDF;
    } else if (m->m_len != (int)CAN_MTU) {
        CAN_LOG_ERR("Invalid frame size %d", m->m_len);
        err = EBADMSG;
        goto error;
    }

    err = sockcan_validate_frame(mtod(m, const void *), m->m_len == (int)CANFD_MTU);
    if (err != 0) {
        goto error;
    }

    sockcan_log_frame("send", dev->ifp, m);
    // sockcan_input takes ownership of m (hands it to the driver or frees it).
    err = sockcan_input(pcb, dev, m);
    m   = NULL;

error:
    // pru_send owns m on every path, so anything not handed on is freed here.
    if (m != NULL) {
        m_freem(m);
    }
    if (held != NULL) {
        if_rele(held);
    }
    return err;
}

/**
 * Look up a CAN interface by index for the timing/devopts ioctls, holding a
 * reference on its ifnet (release with if_rele). Returns NULL, with *ifpp set to
 * NULL, if the index is not a usable CAN interface. Mirrors the validation in bind.
 */
static sockcan_dev_t *sockcan_dev_byindex(int ifindex, struct ifnet **ifpp) {
    struct ifnet *ifp = ifnet_byindex_ref(ifindex);
    if (ifp == NULL) {
        return NULL;
    }

    *ifpp = ifp;
    if (ifp->if_type != IFT_OTHER || ifp->if_softc == NULL || !IFC_IS_CAN_MTU(ifp)) {
        if_rele(ifp);
        *ifpp = NULL;
        return NULL;
    }

    return ifp->if_softc;
}

/**
 * Handle the SIOC[SG]CAN[DATA]BITTIMING ioctls. The interface is named by the
 * ifindex in the can_bittiming_t the caller passes (no bind required). SET records
 * the timing on the device and, if the driver supplied a hook, pushes it to
 * hardware (committing only if the hook succeeds); GET reads back the last-set timing.
 */
static int sockcan_bittiming_ioctl(u_long cmd, caddr_t data) {
    can_bittiming_t *bt = (can_bittiming_t *)data;
    struct ifnet    *ifp;
    sockcan_dev_t   *dev = sockcan_dev_byindex(bt->ifindex, &ifp);
    if (dev == NULL) {
        return ENXIO;
    }

    int err = 0;
    switch (cmd) {
        case SIOCSCANBITTIMING:
            // Prefer the driver's hook; otherwise record in the fallback field.
            if (dev->bittiming != NULL) {
                err = dev->bittiming(dev, bt, true);
            } else {
                dev->nominal_bt = *bt;
            }
            break;
        case SIOCSCANDATABITTIMING:
            if (dev->data_bittiming != NULL) {
                err = dev->data_bittiming(dev, bt, true);
            } else {
                dev->data_bt = *bt;
            }
            break;
        case SIOCGCANBITTIMING:
            if (dev->bittiming != NULL) {
                err = dev->bittiming(dev, bt, false);
            } else {
                *bt = dev->nominal_bt;
            }
            bt->ifindex = ifp->if_index;
            break;
        case SIOCGCANDATABITTIMING:
            if (dev->data_bittiming != NULL) {
                err = dev->data_bittiming(dev, bt, false);
            } else {
                *bt = dev->data_bt;
            }
            bt->ifindex = ifp->if_index;
            break;
        default:
            err = EINVAL;
            break;
    }

    if_rele(ifp);
    return err;
}

/**
 * Handle the SIOC[SG]CANDEVOPTS ioctls. GET reports the interface's enabled
 * capabilities (CANIFC_CAP_*); SET updates them, masked to the capabilities the
 * interface advertises so only supported options can be enabled.
 */
static int sockcan_devopts_ioctl(u_long cmd, caddr_t data) {
    can_devopts_t *opt = (can_devopts_t *)data;
    struct ifnet  *ifp;
    sockcan_dev_t *dev = sockcan_dev_byindex(opt->ifindex, &ifp);
    if (dev == NULL) {
        return ENXIO;
    }

    if (cmd == SIOCSCANDEVOPTS) {
        dev->enabled_caps = opt->caps & dev->caps;
    } else { /* SIOCGCANDEVOPTS */
        opt->caps    = dev->enabled_caps;
        opt->ifindex = ifp->if_index;
    }

    if_rele(ifp);
    return 0;
}

/**
 * Handle the SIOCGCANDEVINFO ioctl: fill in a snapshot (name, flags, caps) of the
 * CAN interface named by can_devinfo_t.ifindex. A non-CAN interface index yields
 * ENXIO, which is how the caller tells CAN interfaces apart while enumerating.
 */
static int sockcan_devinfo_ioctl(caddr_t data) {
    can_devinfo_t *info = (can_devinfo_t *)data;
    struct ifnet  *ifp;
    sockcan_dev_t *dev = sockcan_dev_byindex(info->ifindex, &ifp);
    if (dev == NULL) {
        return ENXIO;
    }

    info->ifindex      = ifp->if_index;
    info->caps         = dev->caps;
    info->enabled_caps = dev->enabled_caps;
    info->flags        = ifp->if_flags;
    strlcpy(info->name, ifp->if_xname, sizeof(info->name));

    if_rele(ifp);
    return 0;
}

/**
 * Reports the interface the socket is bound to for getsockname().
 */
static int sockcan_sockaddr(struct socket *so, struct sockaddr **nam) {
    CAN_TRACE();
    struct can_pcb      *pcb  = sotocanpcb(so);
    struct sockaddr_can *addr = malloc(sizeof(*addr), M_SONAME, M_WAITOK | M_ZERO);

    addr->sa_len     = sizeof(*addr);
    addr->can_family = AF_CAN;

    CAN_LOCK(pcb);
    // An unbound socket reports ifindex 0, the same as a bind-any socket.
    addr->can_ifindex = (pcb->ifc == CAN_IFC_BIND_UNBOUND) ? 0 : pcb->ifc;
    CAN_UNLOCK(pcb);

    *nam = (struct sockaddr *)addr;
    return 0;
}

static int sockcan_control(struct socket *so, u_long cmd, void *data, struct ifnet *ifp, struct thread *td) {
    CAN_TRACE();
    switch (cmd) {
        case SIOCGSTAMP: {
            SOCKBUF_LOCK(&so->so_rcv);
            uint64_t ts = sotocanpcb(so)->last_tstmp_ns;
            SOCKBUF_UNLOCK(&so->so_rcv);
            struct timeval tv = {.tv_sec = ts / 1000000000, .tv_usec = (ts % 1000000000) / 1000};
            memcpy(data, &tv, sizeof(tv));
            return 0;
        }
        case SIOCGSTAMPNS: {
            SOCKBUF_LOCK(&so->so_rcv);
            uint64_t ts = sotocanpcb(so)->last_tstmp_ns;
            SOCKBUF_UNLOCK(&so->so_rcv);
            struct timespec tp = {.tv_sec = ts / 1000000000, .tv_nsec = ts % 1000000000};
            memcpy(data, &tp, sizeof(tp));
            return 0;
        }
        case SIOCGIFNAME: {
            struct ifreq *ifr_ifru = (struct ifreq *)data;
            struct ifnet *net      = ifnet_byindex_ref(ifr_ifru->ifr_ifru.ifru_index);
            if (net == NULL) {
                return ENODEV;
            }
            strcpy(data, net->if_xname);
            return 0;
        }
        case SIOCSCANBITTIMING:
        case SIOCGCANBITTIMING:
        case SIOCSCANDATABITTIMING:
        case SIOCGCANDATABITTIMING:
            return sockcan_bittiming_ioctl(cmd, data);
        case SIOCGCANDEVOPTS:
        case SIOCSCANDEVOPTS:
            return sockcan_devopts_ioctl(cmd, data);
        case SIOCGCANDEVINFO:
            return sockcan_devinfo_ioctl(data);
        default:
            return EOPNOTSUPP;
    }
}

/**
 * Linux provides a way with ioctl(SIOCGSTAMP) to get the receive the last timestamp.
 * to mimic it on read will cache the last timestamp for the socket
 **/
static int sockcan_soreceive(struct socket *so, struct sockaddr **paddr, struct uio *uio, struct mbuf **mp0,
                             struct mbuf **controlp, int *flagsp) {
    CAN_TRACE();
    struct can_pcb *pcb = sotocanpcb(so);

    int flags = 0;
    if (flagsp != NULL) {
        flags = *flagsp;
    }

    /**
     * soreceive_generic will block until data has arrived popping it off
     * the stack before returning mean we won't get a chance to get the clients
     * last timestamp.
     *
     * This means we will mimic the behaviour of soreceive_generic
     * so we can grab the timestamp before handing it off to be read.
     */
    SOCKBUF_LOCK(&so->so_rcv);
    // Only wait if nothing is queued yet. sbwait blocks for the *next* wakeup, so if
    // a frame was already delivered (e.g. our own loopback fired during write, before
    // this read) waiting here would sleep forever.
    int err = 0;
    if (sbavail(&so->so_rcv) == 0 && !(so->so_rcv.sb_state & SBS_CANTRCVMORE)) {
        err = sbwait(so, SO_RCV);
    }
    // sbwait returns an error when interrupted by a signal. If we swallowed it and
    // fell through to soreceive_generic it would block again, leaving the read (and
    // the process) stuck; propagate it instead so the caller can be unblocked.
    if (err != 0) {
        SOCKBUF_UNLOCK(&so->so_rcv);
        return err;
    }
    struct mbuf *m = so->so_rcv.sb_mb;
    // The record starts with the source address, the frame is the mbuf after it.
    if (m != NULL && m->m_type == MT_SONAME) {
        m = m->m_next;
    }
    if (m != NULL && (m->m_flags & M_PKTHDR) && (m->m_flags & M_TSTMP)) {
        /**
         * Before a message is sent we need to send the rcv time
         * so it can be read with SIOCGSTAMP or SIOCGSTAMPNS.
         */
        pcb->last_tstmp_ns = m->m_pkthdr.rcv_tstmp;
    }
    SOCKBUF_UNLOCK(&so->so_rcv);

    return soreceive_generic(so, paddr, uio, mp0, controlp, flagsp);
}

/**
 * Get/set a boolean socket option backed by a single bit in pcb->sock_opts.
 * SET stores nonzero as the bit; GET returns 1/0. Adding a new boolean CAN_RAW_*
 * option is then a one-line case in sockcan_ctloutput.
 */
static int sockcan_boolopt(struct can_pcb *pcb, struct sockopt *sopt, uint32_t bit) {
    int optval = 0;
    if (sopt->sopt_dir == SOPT_SET) {
        int err = sooptcopyin(sopt, &optval, sizeof(optval), sizeof(optval));
        if (err != 0) {
            return err;
        }
        CAN_LOCK(pcb);
        if (optval > 0) {
            pcb->sock_opts |= bit;
        } else {
            pcb->sock_opts &= ~bit;
        }
        CAN_UNLOCK(pcb);
        return 0;
    }

    CAN_LOCK(pcb);
    optval = (pcb->sock_opts & bit) ? 1 : 0;
    CAN_UNLOCK(pcb);
    return sooptcopyout(sopt, &optval, sizeof(optval));
}

/**
 * Get/set a socket's CAN_RAW_FILTER list. SET replaces pcb->pfilter wholesale
 * (an empty list means no messages are accepted); GET copies the current list
 * out. Mirrors sockcan_boolopt's shape so CAN_RAW_FILTER is a one-line case in
 * sockcan_ctloutput.
 */
static int sockcan_filteropt(struct can_pcb *pcb, struct sockopt *sopt) {
    if (sopt->sopt_dir == SOPT_SET) {
        // Validate the filter are the right size
        if (sopt->sopt_valsize % (sizeof(struct can_filter)) != 0) {
            return EINVAL;
        }

        int nfilters = sopt->sopt_valsize / (sizeof(struct can_filter));
        if (nfilters > CAN_RAW_FILTER_MAX) {
            return EINVAL;
        }

        struct can_filter *filters = NULL;
        if (nfilters > 0) {
            long nbytes = sizeof(struct can_filter) * nfilters;
            filters     = malloc(nbytes, M_DEVBUF, M_WAITOK);
            int err     = sooptcopyin(sopt, filters, nbytes, nbytes);
            if (err != 0) {
                // Nothing has been published yet, so the socket keeps its
                // previous filters rather than an uninitialised array.
                free(filters, M_DEVBUF);
                return err;
            }
        }  // No filter supplied all messages are blocked.

        CAN_LOCK(pcb);
        struct can_filter *old = pcb->pfilter;
        pcb->nfilter           = nfilters;
        pcb->pfilter           = filters;
        CAN_UNLOCK(pcb);

        // If the old filter isn't the default filter make sure to free it.
        if (old != &g_default_can_filter) {
            free(old, M_DEVBUF);
        }
        return 0;
    } else {
        CAN_LOCK(pcb);
        int                ncopy    = pcb->nfilter;
        size_t             nbytes   = sizeof(struct can_filter) * ncopy;
        struct can_filter *snapshot = NULL;
        if (ncopy > 0) {
            snapshot = malloc(nbytes, M_DEVBUF, M_NOWAIT);
            if (snapshot != NULL) {
                memcpy(snapshot, pcb->pfilter, nbytes);
            }
        }
        CAN_UNLOCK(pcb);
        if (ncopy > 0 && snapshot == NULL) {
            return ENOMEM;
        }

        int err = sooptcopyout(sopt, snapshot, nbytes);
        free(snapshot, M_DEVBUF);
        return err;
    }
}

/**
 * Get/set a socket's CAN_RAW_ERR_FILTER mask, which selects the CAN_ERR_*
 * classes it wants to receive. Zero (the default) means no error frames at all.
 *
 * The value is masked to CAN_ERR_MASK on the way in: the EFF/RTR/ERR flag bits
 * carry no class information, so accepting them would let a caller match on
 * bits an error frame's id never varies.
 */
static int sockcan_errfilteropt(struct can_pcb *pcb, struct sockopt *sopt) {
    can_err_mask_t mask;

    if (sopt->sopt_dir == SOPT_SET) {
        int err = sooptcopyin(sopt, &mask, sizeof(mask), sizeof(mask));
        if (err != 0) {
            return err;
        }
        mask &= CAN_ERR_MASK;

        CAN_LOCK(pcb);
        pcb->err_mask = mask;
        CAN_UNLOCK(pcb);
        return 0;
    }

    CAN_LOCK(pcb);
    mask = pcb->err_mask;
    CAN_UNLOCK(pcb);
    return sooptcopyout(sopt, &mask, sizeof(mask));
}

static int sockcan_ctloutput(struct socket *so, struct sockopt *sopt) {
    CAN_TRACE();

    if (sopt->sopt_level != SOL_CAN_RAW) {
        return EINVAL;
    }

    struct can_pcb *pcb = sotocanpcb(so);

    switch (sopt->sopt_name) {
        case CAN_RAW_RECV_OWN_MSGS:
            return sockcan_boolopt(pcb, sopt, SOCK_OPTS_RECV_OWN_MSGS);
        case CAN_RAW_FD_FRAMES:
            return sockcan_boolopt(pcb, sopt, SOCK_OPTS_FD_FRAMES);
        case CAN_RAW_LOOPBACK:
            return sockcan_boolopt(pcb, sopt, SOCK_OPTS_LOOPBACK);
        case CAN_RAW_JOIN_FILTERS:
            return sockcan_boolopt(pcb, sopt, SOCK_OPTS_JOIN_FILTERS);
        case CAN_RAW_FILTER:
            return sockcan_filteropt(pcb, sopt);
        case CAN_RAW_ERR_FILTER:
            return sockcan_errfilteropt(pcb, sopt);
        default:
            return ENOPROTOOPT;
    }
}

/**
 * Setup the domain (Address family) for AF_CAN
 *
 * Currently only CAN_RAW is supported which allows for reading and writing can frames
 * to specific interfaces without CAN_BCM support.
 */
struct protosw sockcan_protosw[1] = {{.pr_type      = SOCK_RAW,
                                      .pr_domain    = &sockcandomain,
                                      .pr_protocol  = CAN_RAW,
                                      .pr_flags     = PR_ATOMIC | PR_ADDR,
                                      .pr_ctloutput = &sockcan_ctloutput,
                                      .pr_attach    = sockcan_attach,
                                      .pr_detach    = sockcan_detach,
                                      .pr_bind      = sockcan_bind,
                                      .pr_send      = sockcan_send,
                                      .pr_close     = sockcan_close,
                                      .pr_control   = sockcan_control,
                                      .pr_sockaddr  = sockcan_sockaddr,
                                      .pr_soreceive = sockcan_soreceive}};
