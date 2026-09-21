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

#include "hw/libsockcan.h"
#include "sockcan_debug.h"
#include "sockcan_usrreq.h"

/**
 * Sockets bound to ifindex 0 (CAN_IFC_BIND_ANY) listen to every CAN interface.
 */
static struct mtx sockcan_global_mtx;
static LIST_HEAD(, can_pcb) sockcan_anylist;

void sockcan_global_init(void) {
    mtx_init(&sockcan_global_mtx, "sockcan_global", NULL, MTX_DEF);
    LIST_INIT(&sockcan_anylist);
}

void sockcan_any_insert(struct can_pcb *pcb) {
    mtx_lock(&sockcan_global_mtx);
    LIST_INSERT_HEAD(&sockcan_anylist, pcb, any_socks);
    mtx_unlock(&sockcan_global_mtx);
}

void sockcan_any_remove(struct can_pcb *pcb) {
    mtx_lock(&sockcan_global_mtx);
    LIST_REMOVE(pcb, any_socks);
    mtx_unlock(&sockcan_global_mtx);
}

/**
 * Checks if a filter is accepted by a connection.
 */
static bool sockcan_filter_accept(struct can_pcb *pcb, struct mbuf *m) {
    const can_frame_t *f = mtod(m, const can_frame_t *);

    int matches = 0;
    int fidx    = 0;

    CAN_LOCK(pcb);

    /**
     * In the case that SOCK_OPTS_JOIN_FILTERS is set a can_id
     * must pass all filter in order to get accepted. Meaning
     * we must check all filters in the case join_filters=true
     */
    bool join_filters = (pcb->sock_opts & SOCK_OPTS_JOIN_FILTERS) != 0;

    for (fidx = 0; fidx < pcb->nfilter; fidx++) {
        /**
         * When checking the filter we need to first check if the invert bit is set
         * and strip it. This is needed as CAN_INV_FILTER == CAN_ERR_FLAG in f->can_id.
         *
         * If a filter is inverted it just means we invert the match i.e
         * if match = true && invert = true we reject the frame.
         **/
        bool    invert   = (pcb->pfilter[fidx].can_id & CAN_INV_FILTER) != 0;
        canid_t can_id   = pcb->pfilter[fidx].can_id & ~CAN_INV_FILTER;
        canid_t can_mask = pcb->pfilter[fidx].can_mask;

        bool match = (f->can_id & can_mask) == (can_id & can_mask);
        match      = invert != match;

        if (match) {
            matches++;
            if (!join_filters) {
                break;
            }
        } else if (join_filters) {
            break;  // Frame not valid break
        }
    }

    // A frame is accepted when it matched at least one filter, and under
    // join_filters only when it matched every one of them.
    bool accept = (matches != 0) && !(join_filters && matches != pcb->nfilter);
    CAN_UNLOCK(pcb);

    return accept;
}

/**
 * Deliver a copy of m to a single socket, applying the per-socket filtering
 * shared by the bound-device and bind-any delivery walks: the sender sees its
 * own frame only if it enabled RECV_OWN_MSGS, and an FD frame reaches a socket
 * only if it enabled CAN_RAW_FD_FRAMES.
 */
static void sockcan_deliver_one(struct can_pcb *pcb, struct mbuf *m, struct can_pcb *sender,
                                const struct sockaddr_can *from, struct ifnet *ifp) {
    // An FD frame is larger than a Classic frame. Deliver FD frames only to sockets
    // that opted in with CAN_RAW_FD_FRAMES; Classic frames go to every bound socket.
    int is_fd = m->m_len > (int)CAN_MTU;

    if (pcb == sender && !(pcb->sock_opts & SOCK_OPTS_RECV_OWN_MSGS)) {
        return;
    } else if (is_fd && !(pcb->sock_opts & SOCK_OPTS_FD_FRAMES)) {
        return;
    }

    const can_frame_t *f = mtod(m, const can_frame_t *);
    if ((f->can_id & CAN_ERR_FLAG) != 0) {
        // If it is an error frame use CAN_RAW_ERR_FILTER instead.
        CAN_LOCK(pcb);
        can_err_mask_t err_mask = pcb->err_mask;
        CAN_UNLOCK(pcb);

        if ((f->can_id & CAN_ERR_MASK & err_mask) == 0) {
            return;
        }
    } else if (!sockcan_filter_accept(pcb, m)) {
        return;
    }

    /**
     * Now that filtering is done add the frame to the client's queue
     *
     * The message is copied to avoid passing ownership as other clients may still
     * need this messages
     */
    struct mbuf *copy = m_copym(m, 0, M_COPYALL, M_NOWAIT);
    if (copy == NULL) {
        if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
        return;
    }

    /**
     * Queued with the source address so recvfrom() reports which interface the
     * frame arrived on. A bind-any socket has no interface of its own, so this is
     * the only way it can tell its devices apart.
     *
     * sbappendaddr takes ownership only when it succeeds; a 0 return means the
     * receive buffer is full and the frame is ours to free.
     */
    if (sbappendaddr(&pcb->so->so_rcv, (const struct sockaddr *)from, copy, NULL) == 0) {
        m_freem(copy);
        if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
        return;
    }
    sorwakeup(pcb->so);
}

/**
 * Deliver a copy of m to every socket bound to dev. If sender is non-NULL it is
 * the socket that originated the frame (a local transmit); it gets its own frame
 * back only if it enabled SOCK_OPTS_RECV_OWN_MSGS. For frames arriving from
 * hardware, pass sender == NULL so every bound socket receives a copy.
 */
static void sockcan_deliver(sockcan_dev_t *dev, struct mbuf *m, struct can_pcb *sender) {
    struct can_pcb *pcb;

    // Debug: log the frame being delivered on this device. Both the hardware RX
    // path (sockcan_output) and the local loopback path funnel through here, so a
    // single log line covers every frame seen on any CAN interface.
    sockcan_log_frame("deliver", dev->ifp, m);

    // Source address reported to recvfrom() for every copy of this frame.
    struct sockaddr_can from = {0};
    from.sa_len              = sizeof(from);
    from.can_family          = AF_CAN;
    from.can_ifindex         = dev->ifp->if_index;

    mtx_lock(&dev->mtx);
    LIST_FOREACH (pcb, &dev->connected_sockets, dev_socks) {
        sockcan_deliver_one(pcb, m, sender, &from, dev->ifp);
    }
    mtx_unlock(&dev->mtx);

    // Bind-any (ifindex 0) sockets receive from every interface, so every frame
    // delivered on any device is also offered to them. Kept in a separate locked
    // section so dev->mtx and the global mtx never nest. A socket is in exactly
    // one of the two lists, so there is no double delivery.
    mtx_lock(&sockcan_global_mtx);
    LIST_FOREACH (pcb, &sockcan_anylist, any_socks) {
        sockcan_deliver_one(pcb, m, sender, &from, dev->ifp);
    }
    mtx_unlock(&sockcan_global_mtx);
}

/**
 * @brief attach can_tag_t to a message.
 *
 * @note if a message already has a tag this is a no-op.
 *
 * @return 0 if the tag was attached.
 */
static int sockcan_tag_attach(struct mbuf *m, can_tag_data_t data) {
    if (!(m->m_flags & M_PKTHDR)) {
        return EINVAL;
    }

    // If the frame already has a tag just ignore attach
    struct m_tag *mt = m_tag_locate(m, MTAG_CAN, MTAG_CAN_INFO, NULL);
    if (mt != NULL) {
        return 0;
    }

    // struct m_tag is the first member of can_tag_t.
    can_tag_t *ct = (can_tag_t *)m_tag_alloc(MTAG_CAN, MTAG_CAN_INFO, MTAG_CAN_SIZE, M_WAITOK);
    memcpy(&ct->can, &data, sizeof(can_tag_data_t));
    m_tag_prepend(m, &ct->tag);
    return 0;
}

int sockcan_output(sockcan_dev_t *dev, struct mbuf *m, can_tag_data_t *tag) {
    if (!(m->m_flags & M_TSTMP)) {
        m->m_flags |= M_TSTMP;
        struct timespec ts;
        nanotime(&ts);
        m->m_pkthdr.rcv_tstmp = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
    }

    // A NULL tag means the frame already carries its own tag.
    struct can_pcb *sender = NULL;
    if (tag != NULL) {
        sockcan_tag_attach(m, *tag);
        sender = tag->sender;
    } else {
        struct m_tag *mt = m_tag_locate(m, MTAG_CAN, MTAG_CAN_INFO, NULL);
        if (mt != NULL) {
            sender = ((can_tag_t *)mt)->can.sender;
        }
    }

    if_inc_counter(dev->ifp, IFCOUNTER_IPACKETS, 1);
    sockcan_deliver(dev, m, sender);
    m_free(m);
    return 0;
}

sockcan_dev_t *sockcan_alloc(const char *name, struct ifc_data *ifd, size_t softc_size) {
    // softc_size must be at least the size of sockcan_dev_t, if not we know they
    // don't have it as their first member so use it as a sanity check.
    if (softc_size < sizeof(sockcan_dev_t)) {
        errno = EINVAL;
        return NULL;
    }

    sockcan_dev_t *dev = malloc(softc_size, M_DEVBUF, M_WAITOK | M_ZERO);
    if (dev == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    /**
     * There is currently no IFT_CAN and adding it risk coliding with an existing
     * IFT using the wrong allocator.
     */
    dev->ifp = if_alloc(IFT_OTHER);
    if (dev->ifp == NULL) {
        free(dev, M_DEVBUF);
        errno = ENOMEM;
        return NULL;
    }
    if_initname(dev->ifp, name, ifd->unit);
    dev->ifp->if_mtu          = CAN_MTU;
    dev->ifp->if_flags        = 0;
    dev->ifp->if_capabilities = 0;
    dev->ifp->if_capenable    = 0;
    dev->ifp->if_hwassist     = 0;
    dev->ifp->if_softc        = dev;

    mtx_init(&dev->mtx, "sockcan_dev", NULL, MTX_DEF);
    LIST_INIT(&dev->connected_sockets);

    return dev;
}

int sockcan_register(sockcan_dev_t *dev) {
    if_attach(dev->ifp);
    return 0;
}

int sockcan_fini(sockcan_dev_t *dev) {
    mtx_destroy(&dev->mtx);
    free(dev, M_DEVBUF);
    return 0;
}

int sockcan_input(struct can_pcb *pcb, sockcan_dev_t *dev, struct mbuf *m) {
    // Add a software timestamp to the message as soon as we receive it
    if (m->m_flags & M_PKTHDR) {
        m->m_flags |= M_TSTMP;
        struct timespec ts;
        nanotime(&ts);
        m->m_pkthdr.rcv_tstmp = (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
    }

    if (dev == NULL || dev->ifp->if_transmit == NULL) {
        m_freem(m);
        return ENETDOWN;
    }

    // Software loopback echoes the frame to local listeners, only when the
    // hardware doesn't loop back for us itself.
    int hw_loopback = dev->enabled_caps & CANIFC_CAP_LOOPBACK;
    int do_loopback = !hw_loopback && (pcb->sock_opts & SOCK_OPTS_LOOPBACK);

    can_tag_data_t tag = {.flags = 0, .sender = pcb};
    if (m->m_len > (int)CAN_MTU) {
        tag.flags |= CANMSG_FLAG_FD_FRAME;
    }

    int tag_err = sockcan_tag_attach(m, tag);
    if (tag_err != 0) {
        CAN_LOG_ERR("Unable to tag frame for %s. Frame dropped.", dev->ifp->if_xname);
        m_freem(m);
        if_inc_counter(dev->ifp, IFCOUNTER_OERRORS, 1);
        return tag_err;
    }

    // Must be copied before if_transmit (if_transmit takes ownership)
    struct mbuf *loopback_frame = NULL;
    if (do_loopback) {
        loopback_frame = m_copym(m, 0, M_COPYALL, M_NOWAIT);

        // loopback is best effort if we drop it just add it to the counter and continue.
        if (loopback_frame != NULL) {
            CAN_LOG_ERR("Unable to copy loopback frame frame for %s. Loopback frame dropped.", dev->ifp->if_xname);
            if_inc_counter(dev->ifp, IFCOUNTER_OERRORS, 1);
        }
    }

    // Consider non-zero values to be dropped frames.
    int err = dev->ifp->if_transmit(dev->ifp, m);
    if (err != 0) {
        if_inc_counter(dev->ifp, IFCOUNTER_OERRORS, 1);
    }

    if (loopback_frame != NULL) {
        if (err == 0) {
            sockcan_deliver(dev, loopback_frame, pcb);
        } else {
            // The driver rejected the frame, so don't echo it as if it reached the bus.
            m_freem(loopback_frame);
        }
    }

    return err;
}
