#ifndef _LINUX_CAN_H
#define _LINUX_CAN_H

#include <linux/socket.h>

typedef uint32_t canid_t;

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
#endif


/**
 * To get around can-utils using the wrong structure we are going to replace theirs with ours.
 * The easiest way to do this is by forcing it to be included at the start of every file but
 * renaming theirs so ours gets picked up instead.
 *
 * Sadly this also gets include during the CMakes test build which only our linux/can.h will be found so include_next will fail
 * to fix this we just need to verify one of their other headers are there in this can can/raw.h
 */
#define sockaddr_can sockaddr_can_linux
#if __has_include (<linux/can/raw.h>)
#include_next <linux/can.h>
#endif
#undef sockaddr_can


