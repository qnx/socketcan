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

#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <hw/socketcan.h>
#include <unistd.h>

#define MAX_FILTERS 16

// Long-only options get a value above the ASCII range.
enum {
    OPT_FD = 256,
    OPT_RTR,
    OPT_DATA
};

static const uint64_t VALUE = 0x0123456789ABCDEF;  // payload we send

// -d overrides VALUE with caller-supplied bytes.
static uint8_t g_data[64];
static int     g_datalen = -1;  // -1: not given, use VALUE

/* Parse a hex byte string like "44010000" into g_data. Returns bytes, or -1. */
static int parse_data(const char *hex) {
    size_t n = strlen(hex);
    if (n == 0 || (n % 2) != 0 || n / 2 > sizeof(g_data)) {
        return -1;
    }
    for (size_t i = 0; i < n; i += 2) {
        char  byte[3] = {hex[i], hex[i + 1], 0};
        char *end     = NULL;
        long  v       = strtol(byte, &end, 16);
        if (end != byte + 2) {
            return -1;
        }
        g_data[i / 2] = (uint8_t)v;
    }
    return (int)(n / 2);
}
static const canid_t  ID    = 0x27;                // CAN message ID

static void usage(const char *prog) {
    fprintf(stderr,
            "usage: %s [options] [ifname]\n"
            "      --fd              send CAN FD frames\n"
            "  -r, --recv-own        receive our own frames and verify the loopback\n"
            "  -f, --filter <spec>   add a receive filter in format id,mask (repeatable, max %d)\n"
            "                        accepted when (frame_id & mask) == (id & mask);\n"
            "                        prefix the id with ~ to invert the match.\n"
            "                        With no -f the socket keeps its default filter.\n"
            "  -j, --join-filters    a frame must match every -f filter, not just one\n"
            "  -e, --eff             send an extended (29-bit id) frame\n"
            "      --rtr             send a remote-transmission-request frame (no CAN FD)\n"
            "  -d, --data <hex>      payload as a hex string, e.g. 4401 (default: a fixed pattern)\n"
            "  -n, --count <N>       number of frames to send (default 1)\n"
            "  -t, --timestamp       print the receive timestamp of echoed frames (with -r)\n"
            "  -i. --id              id of the can frame (default 0x27)"
            "  -h, --help            show this help\n"
            "  ifname                CAN interface (default vcan0)\n",
            prog, MAX_FILTERS);
}

/**
 * Splits a raw can_id into its display id (flag bits masked off, using the
 * SFF or EFF mask as appropriate) and its EFF/RTR flags.
 */
static canid_t decode_id(canid_t raw, bool *is_eff, bool *is_rtr) {
    *is_eff = (raw & CAN_EFF_FLAG) != 0;
    *is_rtr = (raw & CAN_RTR_FLAG) != 0;
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

// Fetch and print the last frame's receive timestamp via SIOCGSTAMP.
static void print_timestamp(int s) {
    struct timeval tv;
    if (ioctl(s, SIOCGSTAMP, &tv) < 0) {
        perror("ioctl(SIOCGSTAMP)");
        return;
    }
    printf("  timestamp: %ld.%06ld\n", (long)tv.tv_sec, (long)tv.tv_usec);
}

// Send one frame; if recv_own, read it back and verify. Returns 0 on success.
static int write_one(int s, canid_t id, bool fd_mode, bool recv_own, bool want_ts) {
    bool is_eff;
    bool is_rtr;
    canid_t disp_id = decode_id(id, &is_eff, &is_rtr);

    if (fd_mode) {
        struct canfd_frame frame = {0};
        frame.can_id             = id;
        frame.len                = 16;  // > 8 bytes: only valid for CAN FD
        memcpy(&frame.data[0], &VALUE, sizeof(VALUE));
        memcpy(&frame.data[8], &VALUE, sizeof(VALUE));

        if (write(s, &frame, CANFD_MTU) != (ssize_t)CANFD_MTU) {
            perror("write() failure");
            return 1;
        }
        printf("Wrote a CAN FD frame {id:0x%X%s, len:%u}\n", disp_id, is_eff ? " [EFF]" : "", frame.len);

        if (recv_own) {
            struct canfd_frame echo = {0};
            if (read(s, &echo, CANFD_MTU) != (ssize_t)CANFD_MTU) {
                perror("read() failure");
                return 1;
            }
            if (want_ts) {
                print_timestamp(s);
            }
            printf("Read back a CAN FD frame {id:0x%X%s, len:%u}\n", disp_id, is_eff ? " [EFF]" : "", echo.len);
            if (echo.can_id != id || echo.len != frame.len || memcmp(echo.data, frame.data, frame.len) != 0) {
                fprintf(stderr, "Loopback mismatch\n");
                return 1;
            }
        }
    } else {
        struct can_frame frame = {0};
        frame.can_id           = id;
        frame.can_dlc          = sizeof(VALUE);
        // An RTR frame requests a reply of the given length; it carries no payload
        // of its own, so leave frame.data zeroed rather than filling in VALUE.
        if (g_datalen >= 0) {
            int n = (g_datalen > CAN_MAX_DLEN) ? CAN_MAX_DLEN : g_datalen;
            memcpy(frame.data, g_data, n);
            frame.can_dlc = n;
        } else if (!is_rtr) {
            memcpy(frame.data, &VALUE, sizeof(VALUE));
        }

        if (write(s, &frame, CAN_MTU) != (ssize_t)CAN_MTU) {
            perror("write() failure");
            return 1;
        }
        printf("Wrote a CAN frame {id:0x%X%s%s, data:0x%016lX}\n", disp_id, is_eff ? " [EFF]" : "",
               is_rtr ? " [RTR]" : "", is_rtr ? 0 : VALUE);

        if (recv_own) {
            struct can_frame echo = {0};
            if (read(s, &echo, CAN_MTU) != (ssize_t)CAN_MTU) {
                perror("read() failure");
                return 1;
            }
            if (want_ts) {
                print_timestamp(s);
            }
            uint64_t echo_value = 0;
            (void)memcpy(&echo_value, echo.data, echo.can_dlc);
            printf("Read back a CAN frame {id:0x%X%s%s, data:0x%016lX}\n", disp_id, is_eff ? " [EFF]" : "",
                   is_rtr ? " [RTR]" : "", echo_value);
            // An RTR frame's data has no meaning, so only its id and requested length round-trip.
            if (echo.can_id != id || echo.can_dlc != sizeof(VALUE)
                || (!is_rtr && echo_value != VALUE)) {
                fprintf(stderr, "Loopback mismatch\n");
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    bool    fd_mode      = false;
    bool    recv_own     = false;
    bool    want_ts      = false;
    bool    join_filters = false;
    bool    eff          = false;
    bool    rtr          = false;
    int     nfilters     = 0;
    long    count        = 1;
    canid_t id           = ID;

    struct can_filter filters[MAX_FILTERS];

    static struct option long_opts[] = {{"fd", no_argument, NULL, OPT_FD},
                                        {"recv-own", no_argument, NULL, 'r'},
                                        {"filter", required_argument, NULL, 'f'},
                                        {"join-filters", no_argument, NULL, 'j'},
                                        {"eff", no_argument, NULL, 'e'},
                                        {"rtr", no_argument, NULL, OPT_RTR},
                                        {"data", required_argument, NULL, 'd'},
                                        {"count", required_argument, NULL, 'n'},
                                        {"id", required_argument, NULL, 'i'},
                                        {"timestamp", no_argument, NULL, 't'},
                                        {"help", no_argument, NULL, 'h'},
                                        {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "erf:jn:i:thd:", long_opts, NULL)) != -1) {
        switch (opt) {
            case OPT_FD:
                fd_mode = true;
                break;
            case 'r':
                recv_own = true;
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
            case 'e':
                eff = true;
                break;
            case 'd':
                g_datalen = parse_data(optarg);
                if (g_datalen < 0) {
                    fprintf(stderr, "bad -d '%s', expected an even-length hex string\n", optarg);
                    return 1;
                }
                break;
            case OPT_RTR:
                rtr = true;
                break;
            case 'i':
                id = strtoul(optarg, NULL, 0);
                break;
            case 'n':
                count = strtol(optarg, NULL, 0);
                break;
            case 't':
                want_ts = true;
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

    if (count < 1) {
        count = 1;
    }

    if (rtr && fd_mode) {
        fprintf(stderr, "--rtr is not valid with --fd: CAN FD has no RTR frames\n");
        return 1;
    }
    if (!eff && id > CAN_SFF_MASK) {
        fprintf(stderr, "id 0x%X exceeds the 11-bit standard range; pass --eff for an extended id\n", id);
        return 1;
    }
    if (eff && id > CAN_EFF_MASK) {
        fprintf(stderr, "id 0x%X exceeds the 29-bit extended range\n", id);
        return 1;
    }
    id |= (eff ? CAN_EFF_FLAG : 0) | (rtr ? CAN_RTR_FLAG : 0);

    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
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
    if (recv_own && setsockopt(s, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &on, sizeof(on)) < 0) {
        perror("setsockopt(CAN_RAW_RECV_OWN_MSGS)");
        close(s);
        return 1;
    }
    if (fd_mode && setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &on, sizeof(on)) < 0) {
        perror("setsockopt(CAN_RAW_FD_FRAMES)");
        close(s);
        return 1;
    }
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

    for (long i = 0; i < count; i++) {
        if (write_one(s, id, fd_mode, recv_own, want_ts) != 0) {
            close(s);
            return 1;
        }
    }

    if (recv_own) {
        printf("Loopback matched\n");
    }
    close(s);
    return 0;
}
