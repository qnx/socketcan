/*
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
 *
 * BlackBerry Limited and its licensors  retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from BlackBerry Limited
 * is strictly prohibited.
 */

#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <hw/socketcan.h>
#include <time.h>
#include <unistd.h>

#define MAX_FILTERS 16

// Long-only options get a value above the ASCII range.
enum {
    OPT_FD = 256,
    OPT_RTR,
    OPT_TS_NS,
    OPT_ERR_FILTER
};

static void usage(const char *prog) {
    fprintf(stderr,
            "usage: %s [options] [ifname]\n"
            "      --fd              receive CAN FD frames\n"
            "  -f, --filter <spec>   add a receive filter in format id,mask (repeatable, max %d)\n"
            "                        accepted when (frame_id & mask) == (id & mask);\n"
            "                        prefix the id with ~ to invert the match.\n"
            "                        With no -f the socket keeps its default filter.\n"
            "  -j, --join-filters    a frame must match every -f filter, not just one\n"
            "  -e, --eff             only show extended (29-bit id) frames\n"
            "      --rtr             only show remote-transmission-request frames\n"
            "  -n, --count <N>       stop after N frames (default: unlimited)\n"
            "  -s, --seconds <S>     stop after S seconds (default: run forever)\n"
            "  -t, --timestamp       print each frame's receive timestamp\n"
            "      --timestamp-ns    the same at nanosecond resolution (SIOCGSTAMPNS)\n"
            "      --err-filter <m>  receive error frames whose CAN_ERR_* class bits\n"
            "                        intersect mask m (default 0: no error frames)\n"
            "  -h, --help            show this help\n"
            "  ifname                CAN interface (default vcan0)\n",
            prog, MAX_FILTERS);
}

/**
 * Splits a raw can_id into its display id (flag bits masked off, using the
 * SFF or EFF mask as appropriate) and its EFF/RTR flags.
 */
static canid_t decode_id(canid_t raw, bool *is_eff, bool *is_rtr, bool *is_err) {
    *is_eff = (raw & CAN_EFF_FLAG) != 0;
    *is_rtr = (raw & CAN_RTR_FLAG) != 0;
    *is_err = (raw & CAN_ERR_FLAG) != 0;
    // An error frame's id is not an id at all -- it is a set of CAN_ERR_*
    // class bits, so show the whole thing rather than masking to SFF/EFF.
    if (*is_err) {
        return raw & CAN_ERR_MASK;
    }
    return raw & (*is_eff ? CAN_EFF_MASK : CAN_SFF_MASK);
}

/**
 * Parse an "id,mask" filter spec, optionally prefixed with '~' to invert it.
 * Both halves go through strtoul with base 0, so decimal, 0x-prefixed hex and
 * octal all work. Returns 0 on success, -1 if the spec is malformed.
 */
static int parse_filter(const char *spec, struct can_filter *f) {
    char *end    = NULL;
    bool  invert = false;

    if (*spec == '~') {
        invert = true;
        spec++;
    }

    errno            = 0;
    unsigned long id = strtoul(spec, &end, 0);
    if (end == spec || *end != ',' || errno != 0) {
        return -1;
    }

    const char   *mask_spec = end + 1;
    errno                   = 0;
    unsigned long mask      = strtoul(mask_spec, &end, 0);
    if (end == mask_spec || *end != '\0' || errno != 0) {
        return -1;
    }

    f->can_id   = (canid_t)id;
    f->can_mask = (canid_t)mask;
    if (invert) {
        f->can_id |= CAN_INV_FILTER;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    bool fd_mode      = false;
    bool want_ts      = false;
    bool want_ts_ns   = false;
    bool set_err      = false;
    can_err_mask_t err_mask = 0;
    bool join_filters = false;
    bool eff_only     = false;
    bool rtr_only     = false;
    int  nfilters     = 0;
    long count        = 0;  // 0 => unlimited
    long seconds      = 0;  // 0 => no time limit

    struct can_filter filters[MAX_FILTERS];

    static struct option long_opts[] = {{"fd", no_argument, NULL, OPT_FD},
                                        {"filter", required_argument, NULL, 'f'},
                                        {"join-filters", no_argument, NULL, 'j'},
                                        {"eff", no_argument, NULL, 'e'},
                                        {"rtr", no_argument, NULL, OPT_RTR},
                                        {"count", required_argument, NULL, 'n'},
                                        {"seconds", required_argument, NULL, 's'},
                                        {"timestamp", no_argument, NULL, 't'},
                                        {"timestamp-ns", no_argument, NULL, OPT_TS_NS},
                                        {"err-filter", required_argument, NULL, OPT_ERR_FILTER},
                                        {"help", no_argument, NULL, 'h'},
                                        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "ef:jn:s:th", long_opts, NULL)) != -1) {
        switch (opt) {
            case OPT_FD:
                fd_mode = true;
                break;
            case 'e':
                eff_only = true;
                break;
            case OPT_RTR:
                rtr_only = true;
                break;
            case 'f':
                if (nfilters >= MAX_FILTERS) {
                    fprintf(stderr, "too many filters, max %d\n", MAX_FILTERS);
                    return 1;
                }
                if (parse_filter(optarg, &filters[nfilters]) != 0) {
                    fprintf(stderr, "bad filter '%s', expected id,mask or ~id,mask\n", optarg);
                    return 1;
                }
                nfilters++;
                break;
            case 'j':
                join_filters = true;
                break;
            case 'n':
                count = strtol(optarg, NULL, 0);
                break;
            case 's':
                seconds = strtol(optarg, NULL, 0);
                break;
            case 't':
                want_ts = true;
                break;
            case OPT_TS_NS:
                want_ts_ns = true;
                break;
            case OPT_ERR_FILTER:
                err_mask = (can_err_mask_t)strtoul(optarg, NULL, 0);
                set_err  = true;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }
    const char *ifname = (optind < argc) ? argv[optind] : "vcan0";

    int s = socket(AF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket");
        return 1;
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        close(s);
        return 1;
    }

    struct sockaddr_can addr = {0};
    addr.can_family          = AF_CAN;
    addr.can_ifindex         = ifr.ifr_ifru.ifru_index;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(s);
        return 1;
    }

    const int on = 1;
    if (join_filters && setsockopt(s, SOL_CAN_RAW, CAN_RAW_JOIN_FILTERS, &on, sizeof(on)) < 0) {
        perror("setsockopt(CAN_RAW_JOIN_FILTERS)");
        close(s);
        return 1;
    }

    // Only apply filters if at least one -f was given
    if (nfilters > 0 && setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, filters, nfilters * sizeof(filters[0])) < 0) {
        perror("setsockopt(CAN_RAW_FILTER)");
        close(s);
        return 1;
    }

    // Error frames are a separate subscription from CAN_RAW_FILTER: without
    // this the socket sees none, whatever its normal filters say.
    if (set_err && setsockopt(s, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0) {
        perror("setsockopt(CAN_RAW_ERR_FILTER)");
        close(s);
        return 1;
    }

    // A Classic socket is not delivered CAN FD frames, so enable FD to see them.
    if (fd_mode && setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &on, sizeof(on)) < 0) {
        perror("setsockopt(CAN_RAW_FD_FRAMES)");
        close(s);
        return 1;
    }

    long   received = 0;
    time_t start    = time(NULL);
    for (;;) {
        if (count > 0 && received >= count) {
            break;
        }

        // With a time limit, gate the (blocking) read on select() so we can stop
        // when the deadline passes even if no frame arrives.
        if (seconds > 0) {
            long remain = seconds - (long)(time(NULL) - start);
            if (remain <= 0) {
                break;
            }
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(s, &rfds);
            struct timeval tv = {.tv_sec = remain, .tv_usec = 0};
            int            rc = select(s + 1, &rfds, NULL, NULL, &tv);
            if (rc == 0) {
                break;  // time limit reached
            }
            if (rc < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("select");
                break;
            }
        }

        // A canfd_frame buffer holds either frame type (they share the leading
        // layout); the returned size tells us which was received.
        struct canfd_frame frame = {0};
        ssize_t            size  = read(s, &frame, fd_mode ? CANFD_MTU : CAN_MTU);
        if (size < 0) {
            perror("read() failure");
            close(s);
            return 1;
        }

        bool is_eff;
        bool is_rtr;
        bool    is_err  = false;
        canid_t disp_id = decode_id(frame.can_id, &is_eff, &is_rtr, &is_err);

        if ((eff_only && !is_eff) || (rtr_only && !is_rtr)) {
            continue;
        }

        uint64_t value = 0;
        unsigned n     = frame.len < sizeof(value) ? frame.len : sizeof(value);
        (void)memcpy(&value, frame.data, n);

        if (want_ts_ns) {
            struct timespec tp;
            if (ioctl(s, SIOCGSTAMPNS, &tp) < 0) {
                perror("ioctl(SIOCGSTAMPNS)");
                close(s);
                return 1;
            }
            printf("Read a CAN%s frame (sz=%ld) (ts=%ld.%09ld) {id:0x%X%s%s%s, len:%u, data:0x%016lX}\n",
                   (size == (ssize_t)CANFD_MTU) ? " FD" : "", size, (long)tp.tv_sec, (long)tp.tv_nsec, disp_id,
                   is_eff ? " [EFF]" : "", is_rtr ? " [RTR]" : "", is_err ? " [ERR]" : "", frame.len, value);
        } else if (want_ts) {
            struct timeval tv;
            if (ioctl(s, SIOCGSTAMP, &tv) < 0) {
                perror("ioctl(SIOCGSTAMP)");
                close(s);
                return 1;
            }
            printf("Read a CAN%s frame (sz=%ld) (ts=%ld.%06ld) {id:0x%X%s%s%s, len:%u, data:0x%016lX}\n",
                   (size == (ssize_t)CANFD_MTU) ? " FD" : "", size, (long)tv.tv_sec, (long)tv.tv_usec, disp_id,
                   is_eff ? " [EFF]" : "", is_rtr ? " [RTR]" : "", is_err ? " [ERR]" : "", frame.len, value);
        } else {
            printf("Read a CAN%s frame (sz=%ld) {id:0x%X%s%s%s, len:%u, data:0x%016lX}\n",
                   (size == (ssize_t)CANFD_MTU) ? " FD" : "", size, disp_id, is_eff ? " [EFF]" : "",
                   is_rtr ? " [RTR]" : "", is_err ? " [ERR]" : "", frame.len, value);
        }

        received++;
    }

    (void)close(s);
    return 0;
}
