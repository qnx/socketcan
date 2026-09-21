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
#ifndef HW_SOCKETCAN_H_INCLUDED
#define HW_SOCKETCAN_H_INCLUDED

#include <sys/cdefs.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/sockbuf.h>
#include <sys/socket.h>
#include <hw/socketcan.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <machine/bus.h>
#include <qnx/qnx_modload.h>
#include <qnx/qnx_smmu.h>
#include <sys/sysctl.h>
#include <net/if_clone.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>

// Socket Includes
#include <hw/socketcan.h>

struct can_pcb;

// The CANIFC_CAP_* interface capability bits are part of the device-options ioctl
// ABI, so they live in the public <hw/socketcan.h> (included above) and are
// shared with userspace rather than defined here.

/**
 * Interface for socketcan devices.
 *
 * If a custom softc is used for a driver sockcan_dev_t must be the first member.
 */
typedef struct sockcan_dev_s {
  struct ifnet *ifp;

  // Called to set bittiming to the hardware. If bittiming isn't used then
  // they can be left as NULL and the system will emulate bittiming being set.
  int (*bittiming)(struct sockcan_dev_s *dev, can_bittiming_t *bt, bool set);
  int (*data_bittiming)(struct sockcan_dev_s *dev, can_bittiming_t *bt, bool set);

  uint32_t caps;
  uint32_t enabled_caps;

  // CAN timings
  can_bittiming_t nominal_bt;
  can_bittiming_t data_bt;

  // Protects connected_sockets.
  struct mtx mtx;
  // List of connected sockets
  LIST_HEAD(, can_pcb) connected_sockets;
} sockcan_dev_t;

#define CANMSG_FLAG_LOOPBACK                                                   \
  ((uint8_t)1 << 0) /* frame is a locally-echoed own message */
#define CANMSG_FLAG_FD_FRAME ((uint8_t)1 << 1) /* frame is a CAN FD frame */

/*
 * Every CAN-frame mbuf that flows through the stack carries a can_tag_t as an
 * mbuf packet tag describing the frame (loopback echo, FD frame). The frame's
 * timestamp is NOT carried here -- it lives in m_pkthdr.rcv_tstmp (with
 * M_TSTMP, and M_TSTMP_HPREC for a hardware timestamp). MTAG_CAN is this
 * module's ABI cookie and MTAG_CAN_INFO the (only) tag type; retrieve the tag
 * with:
 *
 *   struct m_tag   *mt = m_tag_locate(m, MTAG_CAN, MTAG_CAN_INFO, NULL);
 *   can_tag_t      *ct = (can_tag_t *)mt;
 *   can_tag_data_t *data = &ct->can;
 */
#define MTAG_CAN 1128481584 /* 'C''A''N''0' ABI cookie */
#define MTAG_CAN_INFO 0     /* tag type */

// What a driver builds and passes to sockcan_output.
typedef struct {
  uint8_t flags; /* CANMSG_FLAG_* */
  struct can_pcb *sender; /* NULL, or the socket this is a loopback echo of */
} can_tag_data_t;

// The stored form of a tag once attached to an mbuf. struct m_tag is embedded
// as the first member (per the alignment guidance in <sys/mbuf.h>'s m_tag_alloc
// comment) so a pointer from m_tag_alloc/m_tag_locate can be cast directly to
// can_tag_t* -- no separate trailing-data offset needed.
typedef struct {
  struct m_tag tag;
  can_tag_data_t can;
} can_tag_t;

#define MTAG_CAN_SIZE (sizeof(can_tag_t) - sizeof(struct m_tag))



/**
 * @brief Allocates a new socketcan device
 *
 * @note sockcan_dev_t must be the first member of a custom softc struct
 *
 * @param name Interface base name (e.g. "vcan", "candummy")
 * @param ifd  Clone data carrying the unit number
 * @param softc_size Size of the softc to allocate, at least sizeof(sockcan_dev_t)
 */
sockcan_dev_t *sockcan_alloc(const char *name, struct ifc_data *ifd, size_t softc_size);

/**
 * Register a new sockcan device.
 */
int sockcan_register(sockcan_dev_t *dev);

/**
 * Cleanup sockcan device
 */
int sockcan_fini(sockcan_dev_t *dev);

/**
 * @brief Sends frame from a client to hardware.
 *
 * @note sockcan_input takes ownership of m on call.
 *
 * @param pcb Client socket.
 * @param dev Device to transmit on / loop back through.
 * @param m Message sent from the client (ownership transferred).
 *
 * @return 0 on success, otherwise an error code.
 */
int sockcan_input(struct can_pcb *pcb, sockcan_dev_t *dev, struct mbuf *m);

/**
 * @brief Deliver a CAN frame received from a hardware device to all sockets bound to
 * it. A software timestamp is filled in unless M_TSTMP is already set. Set
 * CANMSG_FLAG_FD_FRAME in tag->flags if m holds a canfd_frame_t.
 *
 * @note If a CAN message already has tag attached to it, the tag param is ignored.
 * @note This function takes ownership of m
 *
 * @param dev Device message is sent from
 * @param m CAN message.
 * @param tag Infomation about the message. If NULL content or message already contains tag this is ignored.
 *
 * @return 0 on success, otherwise an error code.
 */
int sockcan_output(sockcan_dev_t *dev, struct mbuf *m, can_tag_data_t *tag);

#endif // HW_SOCKETCAN_H_INCLUDED
