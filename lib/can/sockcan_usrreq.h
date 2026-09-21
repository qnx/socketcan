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

#ifndef _SOCKCAN_USRREQ_H_
#define _SOCKCAN_USERREQ_H_

#include <hw/libsockcan.h>
#include <sys/kernel.h>

#define SOCK_OPTS_LOOPBACK      (1 << 0)
#define SOCK_OPTS_RECV_OWN_MSGS (1 << 1)
#define SOCK_OPTS_FD_FRAMES     (1 << 2)
#define SOCK_OPTS_JOIN_FILTERS  (1 << 3)

#define CAN_IFC_BIND_ANY     0
#define CAN_IFC_BIND_UNBOUND -1

struct can_pcb {
    struct socket *so;
    sockcan_dev_t *device;

    /**
     * Linux supports getting the timestamp of the last read message
     * which io-sock as no native support for. in order to get around this
     * we will save the timestamp of a message being read before it is passed
     * to the client.
     */
    uint64_t last_tstmp_ns;

    /**
     * CAN filter for connection
     */
    int                nfilter;
    struct can_filter *pfilter;

    /**
     * CAN_RAW_ERR_FILTER. Which CAN_ERR_* classes this socket wants to see.
     * Defaults to 0, so error frames are not delivered until asked for.
     * pfilter above never applies to error frames -- the two are separate
     * subscriptions, as on Linux.
     */
    can_err_mask_t err_mask;

    /**
     * Socket
     */
    uint32_t sock_opts;

    /**
     * Interface idx which socket is attached too.
     * If ifc=0(CAN_IFC_BIND_ANY) the socket will receive
     * messages from all can drivers which it supports.
     *
     * Note sendto must be used when ifc=CAN_IFC_BIND_ANY
     * in order to send to a specific CAN device
     */
    int ifc;

    /**
     * Links the socket to a specific device's list
     */
    LIST_ENTRY(can_pcb) dev_socks;

    /**
     * Links a socket to any interface.
     *
     * This is only used if ifc == 0
     */
    LIST_ENTRY(can_pcb) any_socks;
};

// Gets can_pcb from socket pointer
#define sotocanpcb(so) ((struct can_pcb *)(so)->so_pcb)

// Protects the can_pcb
#define CAN_LOCK(pcb)   SOCK_LOCK((pcb)->so)
#define CAN_UNLOCK(pcb) SOCK_UNLOCK((pcb)->so)

/**
 * Default filter which all connections start with
 *
 * By default it will accept all non-error frames
 */
extern struct can_filter g_default_can_filter;


/**
 * One-time init of the module-global bind-any socket list and its mutex. Called
 * from the domain init, before any socket binds.
 */
void sockcan_global_init(void);

/**
 * Add/remove a bind-any (ifindex 0) socket to the global promiscuous list, which
 * sockcan_deliver walks so these sockets receive frames from every interface.
 */
void sockcan_any_insert(struct can_pcb *pcb);
void sockcan_any_remove(struct can_pcb *pcb);

extern struct protosw sockcan_protosw[1];

#endif  // _SOCKCAN_USRREQ_H_
