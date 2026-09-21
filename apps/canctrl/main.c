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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/iomsg.h>
#include <sys/neutrino.h>
#include <devctl.h>
#include <errno.h>

#include <hw/socketcan.h>

void
usage() {
    printf("Usage: canctrl [ifname [subcommand ...]]\n"
           "  (no args)              list every CAN interface and its capabilities\n"
           "  ifname timing[:opt=val,...]    get/set nominal bit timing\n"
           "  ifname dtiming[:opt=val,...]   get/set CAN FD data bit timing\n"
           "                         opts: bitrate,clkrate,sample_point,tq,\n"
           "                               prop_seg,phase_seg1,phase_seg2,sjw,brp\n"
           "  ifname devopts[:opt=0|1,...]   get/set device capabilities\n"
           "                         opts: fd,loopback,3samples,berr,hwtimestamp\n"
           "  ifname sockopt         toggle and read back CAN_RAW_LOOPBACK/RECV_OWN_MSGS\n"
           "  ifname up | down       bring the interface up or down\n"
           "  ifname devinfo         show one interface's capabilities\n"
           "  ifname filter          self-test CAN_RAW_FILTER (overwrites current filters)\n"
           "Multiple subcommands may be given for the same ifname.\n");
    exit(EXIT_FAILURE);
}

static int canctrl_bittiming(int sock, int ifindex, int get_dcmd, int set_dcmd, char *args);
static int canctrl_sockopt(int sock);
static int canctrl_devopts(int sock, int ifindex, char *args);
static int canctrl_up(int sock, const char *name);
static int canctrl_down(int sock, const char *name);
static int canctrl_devinfo(int sock);
static int canctrl_filter(int sock);

int main(int argc, char *argv[])
{
    int sock = -1;
    struct ifreq ifr;

    sock = socket(AF_CAN, SOCK_RAW, CAN_RAW);
    if (sock== -1) {
        fprintf(stderr, "socket failed\n");
        return (EXIT_FAILURE);
    }

    if (argc == 1) {
        return canctrl_devinfo(sock);
    }

    // Translate name to index
    memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    strncpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name));
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "Unable to get if_index for %s\n", argv[1]);
        return (EXIT_FAILURE);
    }

    if (argc > 2) {
        int     idx = 2;
        int     rc  = EOK;
        while (idx < argc) {
            if (strncmp(argv[idx], "timing", strlen("timing")) == 0) {
                rc = canctrl_bittiming(sock, ifr.ifr_ifru.ifru_index, SIOCGCANBITTIMING, SIOCSCANBITTIMING, argv[idx]);
            } else if (strncmp(argv[idx], "dtiming", strlen("dtiming")) == 0) {
                rc = canctrl_bittiming(sock, ifr.ifr_ifru.ifru_index, SIOCGCANDATABITTIMING, SIOCSCANDATABITTIMING, argv[idx]);
            } else if (strncmp(argv[idx], "devopts", strlen("devopts")) == 0) {
                rc = canctrl_devopts(sock, ifr.ifr_ifru.ifru_index, argv[idx]);
            } else if (strncmp(argv[idx], "sockopt", strlen("sockopt")) == 0) {
                rc = canctrl_sockopt(sock);
            } else if (strncmp(argv[idx], "up", strlen("up")) == 0) {
                rc = canctrl_up(sock, argv[1]);
            } else if (strncmp(argv[idx], "down", strlen("down")) == 0) {
                rc = canctrl_down(sock, argv[1]);
            } else if (strncmp(argv[idx], "devinfo", strlen("devinfo")) == 0) {
                rc = canctrl_devinfo(sock);
            } else if (strncmp(argv[idx], "filter", strlen("filter")) == 0) {
                rc = canctrl_filter(sock);
            } else {
                fprintf(stderr, "Unknown subcommand '%s'\n", argv[idx]);
                rc = EINVAL;
            }
            if (rc != EOK) {
                return (EXIT_FAILURE);
            }
            idx++;
        }
        return (EXIT_SUCCESS);
    }

    fprintf(stderr, "No subcommand given for '%s'.\n", argv[1]);
    usage();
    return EXIT_FAILURE;
}

static int
canctrl_bittiming(int sock, int ifindex, int get_dcmd, int set_dcmd, char *args)
{
    can_bittiming_t         btm;
    static char *opts[] = {
        "bitrate",          // 0
        "clkrate",          // 1
        "sample_point",     // 2
        "tq",               // 3
        "prop_seg",         // 4
        "phase_seg1",       // 5
        "phase_seg2",       // 6
        "sjw",              // 7
        "brp",              // 8
        NULL
    };

    if (strcmp(args, "timing") == 0 || strcmp(args, "dtiming") == 0) {
        btm.ifindex = ifindex;
        int ret = ioctl(sock, get_dcmd, &btm);
        if (ret == EOK) {
            fprintf(stdout, "Timing:\n");
            fprintf(stdout, "\tbitrate\t\t= %d\n", btm.bitrate);
            fprintf(stdout, "\tclkrate\t\t= %d\n", btm.clkrate);
            fprintf(stdout, "\tsample_point\t= %d\n", btm.sample_point);
            fprintf(stdout, "\ttq\t\t= %d\n", btm.tq);
            fprintf(stdout, "\tprop_seg\t= %d\n", btm.prop_seg);
            fprintf(stdout, "\tphase_seg1\t= %d\n", btm.phase_seg1);
            fprintf(stdout, "\tphase_seg2\t= %d\n", btm.phase_seg2);
            fprintf(stdout, "\tsjw\t\t= %d\n", btm.sjw);
            fprintf(stdout, "\tbrp\t\t= %d\n", btm.brp);
        } else {
            fprintf(stderr, "CAN get-bittiming ioctl failed\n");
        }
    } else {
        int  opt = 0;
        char *cp = NULL;
        char *value= NULL;

        memset(&btm, 0xFF, sizeof(btm));
        btm.ifindex = ifindex;
        cp = strchr(args, ':');
        if (cp == NULL) {
            fprintf(stderr, "Invalid Timing option\n");
            return (EINVAL);
        }
        ++cp;
        if ((cp == NULL) || (*cp == '\0')) {
            fprintf(stderr, "Timing option not specified\n");
            return (EINVAL);
        }
        while ((cp != NULL) && (*cp != '\0')) {
            opt = getsubopt(&cp, opts, &value);
            if (opt == -1) {
                return (EINVAL);
            }
            if ((value == NULL) || (*value == '\0')) {
                fprintf(stderr, "missing argument for option %s\n", opts[opt]);
                return (EINVAL);
            }
            switch (opt) {
                case 0:
                    btm.bitrate = strtol(value, NULL, 0);
                    break;
                case 1:
                    btm.clkrate = strtol(value, NULL, 0);
                    break;
                case 2:
                    btm.sample_point = strtol(value, NULL, 0);
                    break;
                case 3:
                    btm.tq = strtol(value, NULL, 0);
                    break;
                case 4:
                    btm.prop_seg = strtol(value, NULL, 0);
                    break;
                case 5:
                    btm.phase_seg1 = strtol(value, NULL, 0);
                    break;
                case 6:
                    btm.phase_seg2 = strtol(value, NULL, 0);
                    break;
                case 7:
                    btm.sjw = strtol(value, NULL, 0);
                    break;
                case 8:
                    btm.brp = strtol(value, NULL, 0);
                    break;
                default:
                    break;
            }
        }
        int ret = ioctl(sock, set_dcmd, &btm);
        if (ret != EOK) {
            fprintf(stderr, "CAN set-bittiming ioctl failed\n");
            return (ret);
        }
    }

    return (EOK);
}

/*
 * Self-test the CAN_RAW_FILTER mechanism: print whatever filters are currently
 * set, then overwrite them with a synthetic set exercising every filter slot
 * (filter i matches only id i) and read them back to verify the round trip.
 * This replaces the socket's real filters with the synthetic set as a side effect.
 */
static int
canctrl_filter(int sock)
{
    // set up filters
    struct can_filter   rfilter[CAN_RAW_FILTER_MAX];
    socklen_t           optlen = sizeof(rfilter);

    if (getsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, &optlen) == 0) {
        for (int i = 0; optlen >= sizeof(struct can_filter); i++, optlen -= sizeof(struct can_filter)) {
            fprintf(stdout, "Filter %d, mask %x, id %x\n", i, rfilter[i].can_mask, rfilter[i].can_id);
        }

        for (int i = 0; i < sizeof(rfilter)/sizeof(rfilter[0]); ++i) {
            rfilter[i].can_mask = CAN_SFF_MASK;
            rfilter[i].can_id   = i;
        }
        if (setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) == -1) {
            fprintf(stderr, "setsockopt failed(%d)\n", errno);
            return (errno);
        }
        struct can_filter   vfilter[CAN_RAW_FILTER_MAX];
        optlen = sizeof(vfilter);
        if (getsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &vfilter, &optlen) == 0) {
            size_t nm = 0;
            for (int i = 0; optlen >= sizeof(struct can_filter); i++, optlen -= sizeof(struct can_filter)) {
                if ((rfilter[i].can_id == vfilter[i].can_id) && (rfilter[i].can_mask == vfilter[i].can_mask)) {
                    nm++;
                } else {
                    fprintf(stderr, "filter %d mismatch: %x/%x %x/%x\n", i,
                            rfilter[i].can_id, rfilter[i].can_mask, vfilter[i].can_id, vfilter[i].can_mask);
                }
            }
            if (nm == sizeof(rfilter)/sizeof(rfilter[0]) ) {
                fprintf(stderr, "Filter verified successfully\n");
            } else {
                fprintf(stderr, "Only %lu/%lu filters verified successfully\n", nm, sizeof(rfilter)/sizeof(rfilter[0]));
            }
        } else {
            fprintf(stderr, "getsockopt failed(%d)\n", errno);
        }
    } else {
        fprintf(stderr, "getsockopt failed(%d)\n", errno);
    }

    return (EOK);
}

static int
canctrl_sockopt(int sock)
{
    uint32_t    val;
    socklen_t   optlen = sizeof(val);

    if (getsockopt(sock, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &val, &optlen) == 0) {
        fprintf(stdout, "Loopback %s\n", (val == 0) ? "disabled" : "enabled");

        val = !val;
        if (setsockopt(sock, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &val, sizeof(val)) == -1) {
            fprintf(stderr, "setsockopt(LOOPBACK) failed\n");
            return (errno);
        }
        optlen = sizeof(val);
        if (getsockopt(sock, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &val, &optlen) != 0) {
            fprintf(stderr, "getsockopt(LOOPBACK) failed\n");
            return (errno);
        }

        fprintf(stdout, "Loopback %s\n", (val == 0) ? "disabled" : "enabled");
    } else {
        fprintf(stderr, "getsockopt(LOOPBACK) failed\n");
        return (errno);
    }

    optlen = sizeof(val);
    if (getsockopt(sock, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &val, &optlen) == 0) {
        fprintf(stdout, "RecvOwnMSG%s\n", (val == 0) ? "disabled" : "enabled");

        val = !val;
        if (setsockopt(sock, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &val, sizeof(val)) == -1) {
            fprintf(stderr, "setsockopt(CAN_RAW_RECV_OWN_MSGS) failed\n");
            return (errno);
        }
        optlen = sizeof(val);
        if (getsockopt(sock, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &val, &optlen) != 0) {
            fprintf(stderr, "getsockopt(CAN_RAW_RECV_OWN_MSGS) failed\n");
            return (errno);
        }

        fprintf(stdout, "RecvOwnMSG %s\n", (val == 0) ? "disabled" : "enabled");
    } else {
        fprintf(stderr, "getsockopt(LOOPBACK) failed\n");
        return (errno);
    }

    return (EOK);
}

/*
 * Device options: get/set the interface's enabled capabilities via the
 * SIOC[SG]CANDEVOPTS ioctls. "devopts" alone prints them; "devopts:fd=1,..."
 * applies changes on top of the current set (SET is masked by the driver to the
 * capabilities it supports, so read-back may differ from what was requested).
 */
static int
canctrl_devopts(int sock, int ifindex, char *args)
{
    static char *opts[] = {
        "fd",          // CANIFC_CAP_CANFD
        "loopback",    // CANIFC_CAP_LOOPBACK
        "3samples",    // CANIFC_CAP_3_SAMPLES
        "berr",        // CANIFC_CAP_BERR_REPORTING
        "hwtimestamp", // CANIFC_CAP_HW_TIMESTAMP
        NULL
    };
    static const uint32_t opt_caps[] = {
        CANIFC_CAP_CANFD,
        CANIFC_CAP_LOOPBACK,
        CANIFC_CAP_3_SAMPLES,
        CANIFC_CAP_BERR_REPORTING,
        CANIFC_CAP_HW_TIMESTAMP,
    };

    can_devopts_t dev = { .ifindex = ifindex };
    char *cp = strchr(args, ':');

    // Read the current options first (also the base for a modify).
    if (ioctl(sock, SIOCGCANDEVOPTS, &dev) != 0) {
        fprintf(stderr, "SIOCGCANDEVOPTS ioctl failed\n");
        return (errno);
    }

    if (cp != NULL) {
        ++cp;
        while ((cp != NULL) && (*cp != '\0')) {
            char *value;
            int   opt = getsubopt(&cp, opts, &value);
            if (opt == -1 || value == NULL || *value == '\0') {
                fprintf(stderr, "invalid device option\n");
                return (EINVAL);
            }
            if (strtol(value, NULL, 0) != 0) {
                dev.caps |= opt_caps[opt];
            } else {
                dev.caps &= ~opt_caps[opt];
            }
        }
        dev.ifindex = ifindex;
        if (ioctl(sock, SIOCSCANDEVOPTS, &dev) != 0) {
            fprintf(stderr, "SIOCSCANDEVOPTS ioctl failed\n");
            return (errno);
        }
        // Read back the capability-masked result.
        dev.ifindex = ifindex;
        if (ioctl(sock, SIOCGCANDEVOPTS, &dev) != 0) {
            fprintf(stderr, "SIOCGCANDEVOPTS ioctl failed\n");
            return (errno);
        }
    }

    fprintf(stdout, "Device options (caps=0x%08x):\n", dev.caps);
    for (int i = 0; opts[i] != NULL; i++) {
        fprintf(stdout, "\t%-11s = %d\n", opts[i], (dev.caps & opt_caps[i]) ? 1 : 0);
    }
    return (EOK);
}

/*
 * Bring an interface up/down the way ifconfig does: read the current interface
 * flags with SIOCGIFFLAGS, flip IFF_UP, and write them back with SIOCSIFFLAGS.
 */
static int
canctrl_setflags(int sock, const char *name, int up)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        fprintf(stderr, "SIOCGIFFLAGS(%s) failed: %s\n", name, strerror(errno));
        return (errno);
    }
    if (up) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= ~IFF_UP;
    }
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        fprintf(stderr, "SIOCSIFFLAGS(%s) failed: %s\n", name, strerror(errno));
        return (errno);
    }
    fprintf(stdout, "%s is now %s\n", name, up ? "UP" : "DOWN");
    return (EOK);
}

static int
canctrl_up(int sock, const char *name)
{
    return canctrl_setflags(sock, name, 1);
}

static int
canctrl_down(int sock, const char *name)
{
    return canctrl_setflags(sock, name, 0);
}

static void
dump_capabilities(const char *indent, const char *name, uint32_t caps) {
    fprintf(stdout, "%s%s (0x%08x):\n", indent, name, caps);
    if (caps & CANIFC_CAP_UP) fprintf(stdout, "%s  UP\n", indent);
    if (caps & CANIFC_CAP_CANFD) fprintf(stdout, "%s  CANFD\n", indent);
    if (caps & CANIFC_CAP_LOOPBACK) fprintf(stdout, "%s  Loopback\n", indent);
    if (caps & CANIFC_CAP_3_SAMPLES) fprintf(stdout, "%s  3/Triple Sampling\n", indent);
    if (caps & CANIFC_CAP_BERR_REPORTING) fprintf(stdout, "%s  BERR Reporting\n", indent);
    if (caps & CANIFC_CAP_HW_TIMESTAMP) fprintf(stdout, "%s  HW Timestamp\n", indent);
}

/*
 * List every CAN interface and its capabilities. There is no bulk "all devices"
 * call: we enumerate the system's interfaces with if_nameindex() and query each
 * one with SIOCGCANDEVINFO, skipping the interfaces that reject it (non-CAN).
 */
static int
canctrl_devinfo(int sock)
{
    struct if_nameindex *ifs = if_nameindex();
    if (ifs == NULL) {
        fprintf(stderr, "if_nameindex failed: %s\n", strerror(errno));
        return (errno);
    }

    int found = 0;
    for (int i = 0; ifs[i].if_index != 0; i++) {
        can_devinfo_t info = { .ifindex = (int)ifs[i].if_index };
        if (ioctl(sock, SIOCGCANDEVINFO, &info) != 0) {
            continue; /* not a CAN interface */
        }
        fprintf(stdout, "CAN interface %s:\n", info.name);
        fprintf(stdout, "\tifindex:\t%d\n", info.ifindex);
        fprintf(stdout, "\tstate:\t\t%s\n", (info.flags & IFF_UP) ? "UP" : "DOWN");
        dump_capabilities("\t", "Capabilities", info.caps);
        dump_capabilities("\t", "Enabled Capabilities", info.enabled_caps);
        found++;
    }
    if_freenameindex(ifs);

    if (!found) {
        fprintf(stdout, "No CAN interfaces found\n");
    }
    return (EOK);
}

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL$ $Rev$")
#endif
