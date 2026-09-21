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
#ifndef SYS_SOCKETCAN_H_INCLUDED
#define SYS_SOCKETCAN_H_INCLUDED

#include <stdint.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

/**
 * @brief SocketCAN Address Family
 */
#define AF_CAN 56
#define PF_CAN AF_CAN

#define CAN_RAW 1U // RAW sockets
#define CAN_BCM 2U // broadcast manager (not supported to start)

#define SOL_CAN_BASE 100
#define SOL_CAN_RAW (SOL_CAN_BASE + CAN_RAW)

/**
 * SocketCAN specific ioctl that aren't in the BSD stack
 */
#define SIOCGSTAMP _IOR('s', 20, struct timeval)
#define SIOCGSTAMPNS _IOR('s', 28, struct timespec)
#define	SIOCGIFNAME  _IOWR('s',  29, struct ifreq) /*Get the interface name */


/**
 * Custom ioctl messages internal to this socketcan implementation.
 *
 * These configure an interface's CAN bit timing. They deliberately use the same
 * BSD ioccom encoding and command group ('s') as SIOCGSTAMP so io-sock routes
 * them to the protocol's pru_control (sockcan_control) rather than the interface
 * ioctl path. The interface is named by can_bittiming_t.ifindex, so the socket
 * does not need to be bound; issue them with ioctl(2). GET commands are _IOWR
 * because the caller passes .ifindex in and reads the timing back out.
 */
#define SIOCSCANBITTIMING     _IOW('s', 21, can_bittiming_t)  /* set nominal bit timing */
#define SIOCGCANBITTIMING     _IOWR('s', 22, can_bittiming_t) /* get nominal bit timing */
#define SIOCSCANDATABITTIMING _IOW('s', 23, can_bittiming_t)  /* set CAN FD data bit timing */
#define SIOCGCANDATABITTIMING _IOWR('s', 24, can_bittiming_t) /* get CAN FD data bit timing */
/*
 * CAN interface capability bits. caps advertises what the interface supports;
 * enabled_caps (queried/toggled via the device-options ioctls below) is the
 * currently-active subset. Shared with the kernel driver API (hw/libsockcan.h).
 */
#define CANIFC_CAP_UP            (1 << 0) /* interface supports being brought up */
#define CANIFC_CAP_CANFD         (1 << 1) /* interface supports CAN FD */
#define CANIFC_CAP_LOOPBACK      (1 << 2) /* interface supports tx frame loopback */
#define CANIFC_CAP_3_SAMPLES     (1 << 3) /* interface supports triple-sampling of bits */
#define CANIFC_CAP_BERR_REPORTING (1 << 4) /* interface supports reporting bus-error frames */
#define CANIFC_CAP_HW_TIMESTAMP  (1 << 5) /* interface supports hw timestamps */

/*
 * Device options: get/set the interface's enabled capabilities (a subset of the
 * CANIFC_CAP_* bits it advertises). Same encoding/routing as the bit-timing
 * ioctls; the interface is named by can_devopts_t.ifindex. SET is masked to the
 * capabilities the interface actually supports.
 */
#define SIOCGCANDEVOPTS _IOWR('s', 25, can_devopts_t) /* get enabled capabilities */
#define SIOCSCANDEVOPTS _IOW('s', 26, can_devopts_t)  /* set enabled capabilities */

/*
 * Query one CAN interface (named by can_devinfo_t.ifindex) for its name, flags
 * and capabilities. A tool enumerates interfaces (e.g. if_nameindex()) and issues
 * this per interface, skipping the ones that return an error (non-CAN interfaces).
 */
#define SIOCGCANDEVINFO _IOWR('s', 27, can_devinfo_t)

// SOCKET OPTIONS
enum
{
    CAN_RAW_FILTER = 1,
    CAN_RAW_ERR_FILTER,
    CAN_RAW_LOOPBACK,      // If messages sent by this interface is loopback to other interfaces (default=true)
    CAN_RAW_RECV_OWN_MSGS, // When loopback and this is enabled sockets receive their own messages (default=false)
    CAN_RAW_FD_FRAMES,     // Allows CANFD frames */
    CAN_RAW_JOIN_FILTERS,  // To be received all configured filters must match
};

// Largest number of filters CAN_RAW_FILTER will accept in one setsockopt() call.
#define CAN_RAW_FILTER_MAX 512

typedef uint32_t canid_t;
typedef uint32_t can_err_mask_t;

struct sockaddr_can
{
    /**
     * FreeBSD has a slightly different layout in sockaddr than Linux
     * which is used to validate sockaddr in the module.
     * For this reason we are deviating from the structure keeping in mind
     * all offsets will be off by one because of this.
     */
    unsigned sa_len;

    sa_family_t can_family; // Set to AF_CAN

    /**
     * ifindex obtained using:
     *
     * strcpy(ifr.ifr_name, "can0" );
     * ioctl(s, SIOCGIFINDEX, &ifr);
     *
     * If set to 0 all can devices are connected for reading. writing must use sendto
     */
    int can_ifindex;

    union
    {
        struct
        {
            canid_t rx_id, tx_id;
        } tp;
        // This won't be used but left for compatibility
        struct
        {
            /* 8 byte name when using dynamic addressing */
            uint64_t name;
            uint32_t pgn;
            uint8_t addr;
        } j1939;
    } can_addr;
};


// Message ID Flags which are set in can[fd]_frame.can_id
#define CAN_EFF_FLAG 0x80000000U /* Extended frame */
#define CAN_RTR_FLAG 0x40000000U /* RTR frame */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

/* valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */
#define CAN_ERR_MASK 0x1FFFFFFFU /* omit EFF, RTR, ERR flags */

#define CAN_MAX_DLC 8
#define CAN_MAX_RAW_DLC 15
#define CAN_MAX_DLEN 8

typedef struct can_frame
{
    canid_t can_id; /* 32 bit CAN_ID + EFF/RTR/ERR flags */
    // Left as a union for backward compatibility
    union
    {
        uint8_t len;
        uint8_t can_dlc;
    };
    uint8_t __pad;
    uint8_t __res0;
    uint8_t len8_dlc; /* Optional DLC */
    uint8_t data[CAN_MAX_DLEN] __attribute__((aligned(8)));
} can_frame_t;

#define CANFD_MAX_DLC 15
#define CANFD_MAX_DLEN 64

#define CANFD_FLAGS_BRS     ((uint8_t)1 << 0)   /* bit rate switch */
#define CANFD_FLAGS_ESI     ((uint8_t)1 << 1)   /* Transmiting node Error-State-Indicator */
#define CANFD_FLAGS_FDF     ((uint8_t)1 << 2)   /* Frame is a CANFD frame */

typedef struct canfd_frame
{
    canid_t can_id; /* 32 bit CAN_ID + EFF/RTR/ERR flags */
    uint8_t len;
    uint8_t flags;
    uint8_t __res0;
    uint8_t __res1;
    uint8_t data[CANFD_MAX_DLEN] __attribute__((aligned(8)));
} canfd_frame_t;

// Size of tranmision in the API (how large the read/write should be on the socket)
#define CAN_MTU (sizeof(struct can_frame))
#define CANFD_MTU (sizeof(struct canfd_frame))

/**
  * Inverts the filter if set in can_filter.id.
  */
#define CAN_INV_FILTER 0x20000000U
struct can_filter {
  canid_t can_id;
  canid_t can_mask;
};

typedef struct can_bitting_s {
  int ifindex;
  uint32_t bitrate;
  uint32_t clkrate;
  uint32_t sample_point;
  uint32_t tq;
  uint32_t prop_seg;
  uint32_t phase_seg1;
  uint32_t phase_seg2;
  uint32_t sjw;
  uint32_t brp;
} can_bittiming_t;

/*
 * Argument for the SIOC[SG]CANDEVOPTS ioctls. ifindex names the interface; caps
 * is a bitmask of CANIFC_CAP_* -- on GET the interface's enabled capabilities, on
 * SET the desired set (masked to what the interface supports).
 */
typedef struct can_devopts_s {
  int ifindex;
  uint32_t caps;
} can_devopts_t;

/*
 * Result of the SIOCGCANDEVINFO ioctl: a snapshot of one CAN interface. ifindex
 * is the interface to query on input and is echoed on output. name is 16 bytes to
 * match IFNAMSIZ; flags carries the IFF_* bits (IFF_UP for admin up/down); caps
 * and enabled_caps are CANIFC_CAP_* bitmasks.
 */
typedef struct can_devinfo_s {
  int ifindex;
  uint32_t caps;
  uint32_t enabled_caps;
  uint32_t flags;
  char name[16];
} can_devinfo_t;


// Error classes

#define CAN_ERR_DLC 8 /* dlc for error frames */

/* error class (mask) in can_id */
#define CAN_ERR_TX_TIMEOUT   0x00000001U /* TX timeout (by netdevice driver) */
#define CAN_ERR_LOSTARB      0x00000002U /* lost arbitration    / data[0]    */
#define CAN_ERR_CRTL         0x00000004U /* controller problems / data[1]    */
#define CAN_ERR_PROT         0x00000008U /* protocol violations / data[2..3] */
#define CAN_ERR_TRX          0x00000010U /* transceiver status  / data[4]    */
#define CAN_ERR_ACK          0x00000020U /* received no ACK on transmission */
#define CAN_ERR_BUSOFF       0x00000040U /* bus off */
#define CAN_ERR_BUSERROR     0x00000080U /* bus error (may flood!) */
#define CAN_ERR_RESTARTED    0x00000100U /* controller restarted */

/* arbitration lost in bit ... / data[0] */
#define CAN_ERR_LOSTARB_UNSPEC   0x00 /* unspecified */
				      /* else bit number in bitstream */

/* error status of CAN-controller / data[1] */
#define CAN_ERR_CRTL_UNSPEC      0x00 /* unspecified */
#define CAN_ERR_CRTL_RX_OVERFLOW 0x01 /* RX buffer overflow */
#define CAN_ERR_CRTL_TX_OVERFLOW 0x02 /* TX buffer overflow */
#define CAN_ERR_CRTL_RX_WARNING  0x04 /* reached warning level for RX errors */
#define CAN_ERR_CRTL_TX_WARNING  0x08 /* reached warning level for TX errors */
#define CAN_ERR_CRTL_RX_PASSIVE  0x10 /* reached error passive status RX */
#define CAN_ERR_CRTL_TX_PASSIVE  0x20 /* reached error passive status TX */
				      /* (at least one error counter exceeds */
				      /* the protocol-defined level of 127)  */

/* error in CAN protocol (type) / data[2] */
#define CAN_ERR_PROT_UNSPEC      0x00 /* unspecified */
#define CAN_ERR_PROT_BIT         0x01 /* single bit error */
#define CAN_ERR_PROT_FORM        0x02 /* frame format error */
#define CAN_ERR_PROT_STUFF       0x04 /* bit stuffing error */
#define CAN_ERR_PROT_BIT0        0x08 /* unable to send dominant bit */
#define CAN_ERR_PROT_BIT1        0x10 /* unable to send recessive bit */
#define CAN_ERR_PROT_OVERLOAD    0x20 /* bus overload */
#define CAN_ERR_PROT_ACTIVE      0x40 /* active error announcement */
#define CAN_ERR_PROT_TX          0x80 /* error occured on transmission */

/* error in CAN protocol (location) / data[3] */
#define CAN_ERR_PROT_LOC_UNSPEC  0x00 /* unspecified */
#define CAN_ERR_PROT_LOC_SOF     0x03 /* start of frame */
#define CAN_ERR_PROT_LOC_ID28_21 0x02 /* ID bits 28 - 21 (SFF: 10 - 3) */
#define CAN_ERR_PROT_LOC_ID20_18 0x06 /* ID bits 20 - 18 (SFF: 2 - 0 )*/
#define CAN_ERR_PROT_LOC_SRTR    0x04 /* substitute RTR (SFF: RTR) */
#define CAN_ERR_PROT_LOC_IDE     0x05 /* identifier extension */
#define CAN_ERR_PROT_LOC_ID17_13 0x07 /* ID bits 17-13 */
#define CAN_ERR_PROT_LOC_ID12_05 0x0F /* ID bits 12-5 */
#define CAN_ERR_PROT_LOC_ID04_00 0x0E /* ID bits 4-0 */
#define CAN_ERR_PROT_LOC_RTR     0x0C /* RTR */
#define CAN_ERR_PROT_LOC_RES1    0x0D /* reserved bit 1 */
#define CAN_ERR_PROT_LOC_RES0    0x09 /* reserved bit 0 */
#define CAN_ERR_PROT_LOC_DLC     0x0B /* data length code */
#define CAN_ERR_PROT_LOC_DATA    0x0A /* data section */
#define CAN_ERR_PROT_LOC_CRC_SEQ 0x08 /* CRC sequence */
#define CAN_ERR_PROT_LOC_CRC_DEL 0x18 /* CRC delimiter */
#define CAN_ERR_PROT_LOC_ACK     0x19 /* ACK slot */
#define CAN_ERR_PROT_LOC_ACK_DEL 0x1B /* ACK delimiter */
#define CAN_ERR_PROT_LOC_EOF     0x1A /* end of frame */
#define CAN_ERR_PROT_LOC_INTERM  0x12 /* intermission */

/* error status of CAN-transceiver / data[4] */
/*                                             CANH CANL */
#define CAN_ERR_TRX_UNSPEC             0x00 /* 0000 0000 */
#define CAN_ERR_TRX_CANH_NO_WIRE       0x04 /* 0000 0100 */
#define CAN_ERR_TRX_CANH_SHORT_TO_BAT  0x05 /* 0000 0101 */
#define CAN_ERR_TRX_CANH_SHORT_TO_VCC  0x06 /* 0000 0110 */
#define CAN_ERR_TRX_CANH_SHORT_TO_GND  0x07 /* 0000 0111 */
#define CAN_ERR_TRX_CANL_NO_WIRE       0x40 /* 0100 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_BAT  0x50 /* 0101 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_VCC  0x60 /* 0110 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_GND  0x70 /* 0111 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_CANH 0x80 /* 1000 0000 */

#endif // SOCKETCAN_H_INCLUDED
