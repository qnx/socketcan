/*
 * Copyright (c) 2025, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sockio.h>
#include <sys/utsname.h>
#include <time.h>

#include <hw/libsockcan.h>
#include <hw/socketcan.h>
#include <sockcan_debug.h>

#include <sys/buf_ring.h>
#include <sys/taskqueue.h>
#include <net/ifq.h>

#include "bit_timing.h"
#include "debug.h"
#include "event.h"
#include "io.h"
#include "platform.h"
#include "state.h"

// We must define close and usleep as they are not normally exposed on FreeBSD but work correctly on QNX
extern int close(int fd);
extern int usleep(useconds_t __useconds);

#define TX_FIFO 1
#define RX_FIFO 2
#define FIRST_UNUSED_FIFO (RX_FIFO+1)

static const can_bit_timing_consts_t NominalBtConsts = {
    .tseg1Min = 1,
    .tseg1Max = 256,
    .tseg2Min = 1,
    .tseg2Max = 128,
    .sjwMax = 128,
    .brpMin = 1,
    .brpMax = 256,
    .brpInc = 1
};
static const can_bit_timing_consts_t DataBtConsts = {
    .tseg1Min = 1,
    .tseg1Max = 32,
    .tseg2Min = 1,
    .tseg2Max = 16,
    .sjwMax = 16,
    .brpMin = 1,
    .brpMax = 256,
    .brpInc = 1
};

static const mcp25xxfd_config_t Mcp25xxfdDefaultConfig = {
    .dev = NULL,
    .gpio = -1,
    .osc = 40000000,
    .bps = 1000000,
    .dbps = 5000000,
    .txFifoSize = 6,
    .rxFifoSize = 20
};

static const uint8_t DlcToLen[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8,
    12, 16, 20, 24, 32, 48, 64
};

#define MCP_LOCK(ifc)        mtx_lock(&(ifc)->io_mtx)
#define MCP_UNLOCK(ifc)      mtx_unlock(&(ifc)->io_mtx)
#define MCP_ASSERT_LOCKED(ifc) mtx_assert(&(ifc)->io_mtx, MA_OWNED)

// Depth of the software TX queue. buf_ring_alloc requires a power of 2.
#define MCP25XXFD_TX_QUEUE_LEN 64

static inline bool mcp25xxfd_fd_enabled(const mcp25xxfd_dev_t *ifc) {
    return (ifc->dev.enabled_caps & CANIFC_CAP_CANFD) != 0;
}

static int len_to_dlc(uint8_t len) {
    uint8_t idx;

    for (idx = 0; idx < sizeof(DlcToLen)/sizeof(DlcToLen[0]); ++idx) {
        if (len <= DlcToLen[idx]) return idx;
    }
    return -1;
}

static const char* opmode_to_string(int opmode) {
    switch (opmode) {
        case MCP25xxFD_OPMODE_NORMAL:
            return "Normal";
        case MCP25xxFD_OPMODE_SLEEP:
            return "Sleep";
        case MCP25xxFD_OPMODE_INTERNAL_LOOPBACK:
            return "InternalLoopback";
        case MCP25xxFD_OPMODE_LISTEN_ONLY:
            return "ListenOnly";
        case MCP25xxFD_OPMODE_CONFIG:
            return "Config";
        case MCP25xxFD_OPMODE_EXTERNAL_LOOPBACK:
            return "ExternalLoopback";
        case MCP25xxFD_OPMODE_NORMAL_CAN_2_0:
            return "NormalCan2.0";
        case MCP25xxFD_OPMODE_RESTRICTED:
            return "Restricted";
        default:
            return "UNKNOWN";
    }
}

/* True iff hint.mcp25xxfd.<unit>.dev exists. */
static bool mcp25xxfd_unit_configured(int unit) {
    const char *val = NULL;
    return resource_string_value("mcp25xxfd", unit, "dev", &val) == 0 && val != NULL;
}

/*
 * Per-unit config, read via the hint API (hint.mcp25xxfd.<unit>.<key>):
 *   dev=<path>           required, e.g. "/dev/io-spi/spi0/dev0"
 *   gpio=<num>           required
 *   platform=<name>      optional: rpi4, rpi5, generic, or auto
 *   bps=<bps>            optional, default 1000000
 *   dbps=<bps>           optional, default 5000000
 *   osc=<hz>             optional, default 40000000
 *   fd=on|non-iso|off    optional, default off
 *   tx_fifo_size=<n>     optional, default 6
 *   rx_fifo_size=<n>     optional, default 20
 * Caller must have already confirmed mcp25xxfd_unit_configured(unit).
 */
static int mcp25xxfd_load_config(int unit, mcp25xxfd_config_t *cfg) {
    const char *val;
    int ival;

    *cfg = Mcp25xxfdDefaultConfig;

    resource_string_value("mcp25xxfd", unit, "dev", &val);
    cfg->dev = strdup(val, M_DEVBUF);
    if (cfg->dev == NULL) {
        CAN_LOG_ERR("can%d: Failed to copy dev", unit);
        return ENOMEM;
    }

    if (resource_int_value("mcp25xxfd", unit, "gpio", &cfg->gpio) != 0) {
        CAN_LOG_ERR("can%d: Missing required 'gpio'", unit);
        return EINVAL;
    }

    if (resource_string_value("mcp25xxfd", unit, "platform", &val) != 0 || val == NULL || strcmp(val, "auto") == 0) {
        cfg->platform = PLATFORM_AUTO;
    } else if (strcmp(val, "rpi4") == 0) {
        cfg->platform = PLATFORM_RPI4;
    } else if (strcmp(val, "rpi5") == 0) {
        cfg->platform = PLATFORM_RPI5;
    } else if (strcmp(val, "generic") == 0) {
        cfg->platform = PLATFORM_GENERIC;
    } else {
        CAN_LOG_ERR("can%d: Invalid 'platform' value '%s'", unit, val);
        return EINVAL;
    }

    if (resource_int_value("mcp25xxfd", unit, "bps", &ival) == 0) cfg->bps = (uint32_t)ival;
    if (resource_int_value("mcp25xxfd", unit, "dbps", &ival) == 0) cfg->dbps = (uint32_t)ival;
    if (resource_int_value("mcp25xxfd", unit, "osc", &ival) == 0) cfg->osc = (uint32_t)ival;
    if (resource_int_value("mcp25xxfd", unit, "tx_fifo_size", &ival) == 0) cfg->txFifoSize = (uint32_t)ival;
    if (resource_int_value("mcp25xxfd", unit, "rx_fifo_size", &ival) == 0) cfg->rxFifoSize = (uint32_t)ival;

    if (resource_string_value("mcp25xxfd", unit, "fd", &val) == 0 && val != NULL) {
        if (strcmp(val, "on") == 0) {
            cfg->fdMode = FD_MODE_ON;
        } else if (strcmp(val, "non-iso") == 0) {
            cfg->fdMode = FD_MODE_NON_ISO;
        } else if (strcmp(val, "off") == 0) {
            cfg->fdMode = FD_MODE_OFF;
        } else {
            CAN_LOG_ERR("can%d: Invalid 'fd' value '%s'", unit, val);
            return EINVAL;
        }
    }

    return EOK;
}

static void generate_hwts(const mcp25xxfd_dev_t *ifc, const struct timespec *newEpoch, uint32_t rawTs, struct timespec *hwts) {
    div_t divResult;

    // Generate the HW timestamp
    // If there is a new epoch I need to figure out whether I should use it
    // or the old one. The epoch currently rolls over every 71 minutes or so,
    // so if there are frames in the FIFO that span the epoch change there will
    // be a bunch with large hwts values, and some with small ones.
    if (newEpoch != NULL && rawTs < 1000*1000*60) {
        // New epoch and the frame has a hw timstamp less than
        // a minute after rollover. Assume new epoch.
        *hwts = *newEpoch;
    } else {
        // Don't have a new epoch or the hwts is greater than
        // a minute since rollover. Assume old epoch.
        *hwts = ifc->epoch;
    }
    divResult = div(rawTs, 1000*1000); // Convert to seconds & remaining us
    hwts->tv_sec += divResult.quot;
    hwts->tv_nsec += divResult.rem * 1000;
    while (hwts->tv_nsec > 1000*1000*1000) {
        hwts->tv_sec++;
        hwts->tv_nsec -= 1000*1000*1000;
    }
}

static int mcp25xxfd_configure_filter(mcp25xxfd_dev_t *ifc, uint32_t filter, uint32_t dstFifo, uint32_t mid, uint32_t mask) {
    int rc;
    uint32_t conValue;
    uint32_t regValue;

    // First disable the filter
    conValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiFLTCONm(filter));
    if (errno != EOK) {
        rc = errno;
        CAN_LOG_ERR("[%s]: Unable to get value of CiFLTCON%d to disable filter %d. Error=%s",
                ifc->dev.ifp->if_xname, filter/4, filter, strerror(rc));
        return rc;
    }
    conValue &= ~MCP25xxFD_CiFLTCONm_FLTENn_MASK(filter);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiFLTCONm(filter), conValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to write value to CiFLTCON%d to disable filter %d. Error=%s",
                ifc->dev.ifp->if_xname, filter/4, filter, strerror(rc));
        return rc;
    }

    // Set the filter's destination. Done as a two-step because the docs
    // say that this can only be changed when FLTEN is 0 and I don't know
    // if that means it has to already be 0 or it can be set to 0 at the
    // same time. Playing it safe and assuming the former
    conValue &= ~MCP25xxFD_CiFLTCONm_FnBP_MASK(filter);
    conValue |= ((dstFifo << MCP25xxFD_CiFLTCONm_FnBP_SHIFT(filter)) & MCP25xxFD_CiFLTCONm_FnBP_MASK(filter));
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiFLTCONm(filter), conValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to write value to CiFLTCON%d to set dst FIFO for filter %d. Error=%s",
                ifc->dev.ifp->if_xname, filter/4, filter, strerror(rc));
        return rc;
    }

    // Then configure the filter's mask
    regValue =   MCP25xxFD_BITFIELD(mask, CiMASKm_MID)
               | MCP25xxFD_BITFIELD(0, CiMASKm_MSID11)
               | MCP25xxFD_BITFIELD(0, CiMASKm_MIDE);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiMASKm(filter), regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to write value to CiMASKm%d. Error=%s", ifc->dev.ifp->if_xname, filter, strerror(rc));
        return rc;
    }

    // And its MID
    regValue =   MCP25xxFD_BITFIELD(mid, CiFLTOBJm_MID)
               | MCP25xxFD_BITFIELD(0, CiFLTOBJm_SID11)
               | MCP25xxFD_BITFIELD(0, CiFLTOBJm_EXIDE);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiMASKm(filter), regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to write value to CiFLTOBJ%d. Error=%s", ifc->dev.ifp->if_xname, filter, strerror(rc));
        return rc;
    }

    // If the destination was not FIFO0 (which is TXQ and isn't a valid destination),
    // re-enable the filter
    if (dstFifo != 0) {
        conValue |= MCP25xxFD_CiFLTCONm_FLTENn_MASK(filter);
        if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiFLTCONm(filter), conValue)) != EOK) {
            CAN_LOG_ERR("[%s]: Unable to write value to CiFLTCON%d to enable filter %d. Error=%s",
                    ifc->dev.ifp->if_xname, filter/4, filter, strerror(rc));
            return rc;
        }
    }

    return EOK;
}

static int mcp25xxfd_write_bit_timings(mcp25xxfd_dev_t *ifc, can_bit_timings_t *bt, bool data) {
    int rc;
    uint32_t value;
    int addr;

    CAN_LOG_NTC("[%s]: %s(brp=%u prop=%u phase1=%u phase2=%u sjw=%u spPct=%u.%u%%)",
            ifc->dev.ifp->if_xname, data ? "DataBt" : "NominalBt",
            bt->brp, bt->prop, bt->phase1, bt->phase2, bt->sjw,
            bt->spPct / 10, bt->spPct % 10);

    // TSEG1 bitfield includes phase1 and propegation segment
    if (data) {
        // Data
        addr = MCP25xxFD_CiDBTCFG;
        value =   MCP25xxFD_BITFIELD(bt->sjw - 1, CiDBTCFG_SJW)
                | MCP25xxFD_BITFIELD(bt->phase2 - 1, CiDBTCFG_TSEG2)
                | MCP25xxFD_BITFIELD(bt->prop + bt->phase1 - 1, CiDBTCFG_TSEG1)
                | MCP25xxFD_BITFIELD(bt->brp - 1, CiDBTCFG_BRP);
    } else {
        // Nominal
        addr = MCP25xxFD_CiNBTCFG;
        value =   MCP25xxFD_BITFIELD(bt->sjw - 1, CiNBTCFG_SJW)
                | MCP25xxFD_BITFIELD(bt->phase2 - 1, CiNBTCFG_TSEG2)
                | MCP25xxFD_BITFIELD(bt->prop + bt->phase1 - 1, CiNBTCFG_TSEG1)
                | MCP25xxFD_BITFIELD(bt->brp - 1, CiNBTCFG_BRP);
    }
    if ((rc = mcp25xxfd_io_write_word(ifc, addr, value)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to write %s bit timings to Ci%cBTCFG. Error=%s",
                ifc->dev.ifp->if_xname,
                data ? "data" : "nominal",
                data ? 'D' : 'N',
                strerror(rc));
        return rc;
    }

    if (data) {
        // I also need to program the Transmitter Delay Compensation's TDCO
        // value. This is used to help set a secondary sample point to
        // verify written bits when operating at data bit rates.
        // TDCO should be set so that it ends up at the same sample point
        // used to calculate the data bit timings. We already know how many Tqs
        // that takes, it is dbt.prop + dbt.phase1. Just need to also factor
        // in the BRP (as TDCO uses SYSCLK directly).
        //
        // NOTE: For some reason the sync Tq (1) doesn't seem to be factored
        //       into things though it probably should be. I'm leaving it out
        //       for now to match what the datasheet talks about.
        int tdco = (ifc->dbt.prop + ifc->dbt.phase1) * ifc->dbt.brp;
        if (tdco < 0 || tdco > MCP25xxFD_MAX_TDCO) {
            CAN_LOG_ERR("[%s]: Calculated TDCO %d is out of range, 0<=x<=%d", ifc->dev.ifp->if_xname, tdco, MCP25xxFD_MAX_TDCO);
            return EINVAL;
        }

        rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiTDC,
                MCP25xxFD_BITFIELD(2, CiTDC_TDCMOD) | MCP25xxFD_BITFIELD(tdco, CiTDC_TDCO),
                MCP25xxFD_CiTDC_TDCMOD_MASK | MCP25xxFD_CiTDC_TDCO_MASK);
    }

    return rc;
}

static int mcp25xxfd_configure_bit_timings(mcp25xxfd_dev_t *ifc) {
    can_bit_timing_consts_t btConsts;
    int rc;

    // Documentation recommends that the BRP value used for NBRP and DBRP
    // be the same to avoid quantization errors. To try and achieve this
    // I'm going to calculate the data bit-timings first. Then I will try
    // to calculate the nominal bit timings using JUST that BRP. If it works,
    // great. If it doesn't, I'll calculate the nominal bit timings independently
    // of the data ones.

    CAN_LOG_DEBUG2("[%s]: Calculating data bit-timings for %ubps with osc=%uHz", ifc->dev.ifp->if_xname, ifc->cfg.dbps, ifc->cfg.osc);
    if ((rc = can_calculate_bit_timings(ifc->cfg.dbps, ifc->cfg.osc, -1, &DataBtConsts, &ifc->dbt)) != EOK) {
        return rc;
    }

    btConsts = NominalBtConsts;
    btConsts.brpMin = ifc->dbt.brp;
    btConsts.brpMax = ifc->dbt.brp;
    CAN_LOG_DEBUG2("[%s]: Calculating nominal bit-timings for %ubps with osc=%uHz and BRP locked to %u",
            ifc->dev.ifp->if_xname, ifc->cfg.bps, ifc->cfg.osc, ifc->dbt.brp);
    if ((rc = can_calculate_bit_timings(ifc->cfg.bps, ifc->cfg.osc, -1, &btConsts, &ifc->nbt)) != EOK) {
        CAN_LOG_WRN("[%s]: Failed to calculate NBT locking BRP to DBT. Trying free BRP", ifc->dev.ifp->if_xname);
        if ((rc = can_calculate_bit_timings(ifc->cfg.bps, ifc->cfg.osc, -1, &NominalBtConsts, &ifc->nbt)) != EOK) {
            return rc;
        }
    }

    // I was able to come up with timings. Yay! Program them in.
    rc = mcp25xxfd_write_bit_timings(ifc, &ifc->nbt, false);
    if (rc == EOK && ifc->cfg.fdMode != FD_MODE_OFF) {
        rc = mcp25xxfd_write_bit_timings(ifc, &ifc->dbt, true);
    }

    return rc;
}

// Set bittiming configuration, overrides command line
static int mcp25xxfd_do_bittiming(mcp25xxfd_dev_t *ifc, can_bittiming_t *bt, bool get, bool data) {
    uint32_t regValue;
    int count;
    int rc;
    const can_bit_timing_consts_t *btConsts;
    can_bit_timings_t newBt;
    can_bit_timings_t *curBt;

    if (!get) {
        // Can only change the bit-timing when I'm down (ie: in config mode)
        regValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiCON);
        if (errno != EOK) {
            rc = errno;
            CAN_LOG_ERR("[%s]: Unable to verify in CONFIGURATION mode to set bit-timing. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        } else if (MCP25xxFD_GET_BITFIELD(regValue, CiCON_OPMOD) != MCP25xxFD_OPMODE_CONFIG) {
            CAN_LOG_ERR("[%s]: Not in CONFIG mode. Mode=%d", ifc->dev.ifp->if_xname, MCP25xxFD_GET_BITFIELD(regValue, CiCON_OPMOD));
            return ENOSYS;
        }

        // Make sure the inputs are good for what I can do
        if (bt->clkrate != CAN_PARAM_INVALID && bt->clkrate != ifc->cfg.osc) {
            CAN_LOG_ERR("[%s]: Unable to change clockrate. Must be %uHz or unset.", ifc->dev.ifp->if_xname, ifc->cfg.osc);
            return EINVAL;
        } else if (bt->tq != CAN_PARAM_INVALID) {
            CAN_LOG_ERR("[%s]: Unable to change tq directly. Must be unset.", ifc->dev.ifp->if_xname);
            return EINVAL;
        } else if (bt->bitrate == CAN_PARAM_INVALID) {
            CAN_LOG_ERR("[%s]: A bitrate MUST be specified", ifc->dev.ifp->if_xname);
            return EINVAL;
        }

        // Either all of the HW values must be set or none of them.
        count = 0;
        if (bt->sample_point != CAN_PARAM_INVALID) ++count;
        if (bt->prop_seg != CAN_PARAM_INVALID) ++count;
        if (bt->phase_seg1 != CAN_PARAM_INVALID) ++count;
        if (bt->phase_seg2 != CAN_PARAM_INVALID) ++count;
        if (bt->sjw != CAN_PARAM_INVALID) ++count;
        if (bt->brp != CAN_PARAM_INVALID) ++count;
        if (count != 0 && count != 6) {
            CAN_LOG_ERR("[%s]: Either all of sample_point, prop, phase_seg1, phase_seg2, sjw, and brp"
                   " MUST be specified OR none of them",
                    ifc->dev.ifp->if_xname);
            return EINVAL;
        }

        // If a sample point is specified it must be valid
        if (bt->sample_point != CAN_PARAM_INVALID && bt->sample_point > 1000) {
            CAN_LOG_ERR("[%s]: Invalid sample point. Must be between 0-1000", ifc->dev.ifp->if_xname);
            return EINVAL;
        }

        btConsts = data ? &DataBtConsts : &NominalBtConsts;
        if (bt->brp == CAN_PARAM_INVALID) {
            // If HW values were not specified, calculate them.
            rc = can_calculate_bit_timings(bt->bitrate, ifc->cfg.osc,
                    bt->sample_point != CAN_PARAM_INVALID ? bt->sample_point : -1,
                    btConsts, &newBt);
            if (rc != EOK) {
                CAN_LOG_ERR("[%s]: Unable to calculate %s bit timings for bitrate of %ubps",
                        ifc->dev.ifp->if_xname, data ? "data" : "nominal", bt->bitrate);
            }
        } else {
            // Take the values we were given, if they are good.
            rc = EOK;
            if (bt->prop_seg + bt->phase_seg1 >= btConsts->tseg1Min &&
                bt->prop_seg + bt->phase_seg1 <= btConsts->tseg1Max)
            {
                newBt.prop = bt->prop_seg;
                newBt.phase1 = bt->phase_seg1;
            } else {
                CAN_LOG_ERR("[%s]: prop+phase_seg1 must be in range [%u,%u]",
                       ifc->dev.ifp->if_xname, btConsts->tseg1Min, btConsts->tseg1Max);
                rc = EINVAL;
            }
            if (bt->phase_seg2 >= btConsts->tseg2Min &&
                bt->phase_seg2 <= btConsts->tseg2Max)
            {
                newBt.phase2 = bt->phase_seg2;
            } else {
                CAN_LOG_ERR("[%s]: phase_seg2 must be in range [%u,%u]",
                       ifc->dev.ifp->if_xname, btConsts->tseg2Min, btConsts->tseg2Max);
                rc = EINVAL;
            }
            if (bt->sjw >= 1 &&
                bt->sjw <= btConsts->sjwMax)
            {
                newBt.sjw = bt->sjw;
            } else {
                CAN_LOG_ERR("[%s]: sjw must be in range [%u,%u]",
                       ifc->dev.ifp->if_xname, 1, btConsts->sjwMax);
                rc = EINVAL;
            }
            if (bt->brp >= btConsts->brpMin &&
                bt->brp <= btConsts->brpMax &&
                (bt->brp - btConsts->brpMin) % btConsts->brpInc == 0)
            {
                newBt.brp = bt->brp;
            } else {
                CAN_LOG_ERR("[%s]: brp must be in range [%u,%u] with an increment of %u",
                       ifc->dev.ifp->if_xname, btConsts->brpMin, btConsts->brpMax, btConsts->brpInc);
                rc = EINVAL;
            }
            // Sample point was already checked
            newBt.spPct = bt->sample_point;
        }
        if (rc != EOK) return rc;

        // Actually write new values to HW
        rc = mcp25xxfd_write_bit_timings(ifc, &newBt, data);
        if (rc != EOK) return rc;

        // Success! Update internal state
        if (data) {
            ifc->dbt = newBt;
            ifc->cfg.dbps = bt->bitrate;
        } else {
            ifc->nbt = newBt;
            ifc->cfg.bps = bt->bitrate;
        }

        return EOK;
    } else {
        // Just return the required values
        curBt = data ? &ifc->dbt : &ifc->nbt;
        bt->bitrate = data ? ifc->cfg.dbps : ifc->cfg.bps;
        bt->clkrate = ifc->cfg.osc;
        bt->sample_point = curBt->spPct;
        bt->tq = CAN_PARAM_INVALID;
        bt->phase_seg1 = curBt->phase1;
        bt->prop_seg = curBt->prop;
        bt->phase_seg2 = curBt->phase2;
        bt->sjw = curBt->sjw;
        bt->brp = curBt->brp;

        return 0;
    }
}

static int mcp25xxfd_drain_rx_fifo(mcp25xxfd_dev_t *ifc, struct timespec *newEpoch) {
    mcp25xxfd_io_read_packet_t pkt;
    int rc;
    uint32_t status;
    uint32_t userAddr;
    uint32_t *hdr;
    canfd_frame_t frame;
    struct timespec hwts;

    while (true) {
        // Read status and UA
        if ((rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiFIFOSTAm(RX_FIFO), 2*sizeof(uint32_t), &pkt)) != EOK) {
            CAN_LOG_ERR("[%s]: Unable to read RX_FIFO Status & UA to drain. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }
        status = ((uint32_t*)pkt.data)[0];
        userAddr = ((uint32_t*)pkt.data)[1];

        // If there is nothing in the FIFO, I'm done.
        if ((status & MCP25xxFD_BITFIELD(1, CiFIFOSTAm_TFNRFNIF)) == 0) {
            break;
        }

        // Read the message
        rc = mcp25xxfd_io_read(ifc, MCP25xxFD_MAKE_MSGOBJ_ADDR(userAddr), MCP25xxFD_RX_FIFO_SLOT_SIZE, &pkt);
        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to read message from RX_FIFO. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }
        hdr = (uint32_t*)pkt.data;

        // Populate the frame
        memset(&frame, 0, sizeof(frame));
        frame.can_id = hdr[0];
        frame.len = DlcToLen[MCP25xxFD_GET_BITFIELD(hdr[1], MSGOBJ_DLC)];
        if ((hdr[1] & MCP25xxFD_BITFIELD(1, MSGOBJ_IDE)) != 0) {
            frame.can_id &= CAN_EFF_MASK;

            /* In the CAN2.0B standard an identifier comes in either standard
             * or extended format.
             * - The standard format is 11bits in length and consists of just the SID
             * - The extended format is 29bits in length and consists of the SID
             *   and the EID where the SID occupies bits [28:18] and the EID occupies
             *   bits [17:0].
             *
             * Contrast this to the MID exposed by SocketCAN. Here we have either a
             * 11 bit Standard-frame identifier (SFF) or 20 bit Extended-frame identifier
             * (EFF). To make things easier at the API level, the lowest order bit
             * of both of these is placed at bit 0 of the MID.
             *
             * This means that for an extended-frame, I need to shift things around
             * to make the MID come out correct.
             */
            uint32_t eid = MCP25xxFD_GET_BITFIELD(frame.can_id, MSGOBJ_EID);
            uint32_t sid = MCP25xxFD_GET_BITFIELD(frame.can_id, MSGOBJ_SID);
            frame.can_id = eid | (sid << 18);

            frame.can_id |= CAN_EFF_FLAG;
        } else {
            frame.can_id &= CAN_SFF_MASK;
        }
        if ((hdr[1] & MCP25xxFD_BITFIELD(1, MSGOBJ_RTR)) != 0) {
            frame.can_id |= CAN_RTR_FLAG;
        }
        if ((hdr[1] & MCP25xxFD_BITFIELD(1, MSGOBJ_BRS)) != 0) {
            frame.flags |= CANFD_FLAGS_BRS;
        }
        if ((hdr[1] & MCP25xxFD_BITFIELD(1, MSGOBJ_FDF)) != 0) {
            frame.flags |= CANFD_FLAGS_FDF;
        }
        if ((hdr[1] & MCP25xxFD_BITFIELD(1, MSGOBJ_ESI)) != 0) {
            frame.flags |= CANFD_FLAGS_ESI;
        }

        generate_hwts(ifc, newEpoch, hdr[2], &hwts);
        memcpy(frame.data, &hdr[3], frame.len);

        CAN_LOG_DEBUG2("[%s]: Received frame: mid=0x%08x len=%d", ifc->dev.ifp->if_xname, frame.can_id, frame.len);

        bool isFd = (frame.flags & CANFD_FLAGS_FDF) != 0;
        size_t len = isFd ? sizeof(canfd_frame_t) : sizeof(can_frame_t);
        struct mbuf *m = m_gethdr(M_NOWAIT, MT_DATA);
        if (m == NULL) {
            CAN_LOG_ERR("[%s]: Unable to allocate mbuf for received frame. Frame dropped.", ifc->dev.ifp->if_xname);
            if_inc_counter(ifc->dev.ifp, IFCOUNTER_IQDROPS, 1);
        } else {
            memcpy(mtod(m, void *), &frame, len);
            m->m_len = m->m_pkthdr.len = len;
            m->m_flags |= M_TSTMP | M_TSTMP_HPREC;
            m->m_pkthdr.rcv_tstmp = (uint64_t)hwts.tv_sec * 1000000000 + hwts.tv_nsec;
            can_tag_data_t tag = {.flags = isFd ? CANMSG_FLAG_FD_FRAME : 0, .sender = NULL};
            sockcan_output(&ifc->dev, m, &tag);
        }

        // Advance the queue
        rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiFIFOCONm(RX_FIFO),
                                      MCP25xxFD_BITFIELD(1, CiFIFOCONm_UINC), 0);
        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to advance RX_FIFO. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }
    }

    return EOK;
}

static int mcp25xxfd_handle_te_interrupt(mcp25xxfd_dev_t *ifc, struct timespec *newEpoch) {
    mcp25xxfd_io_read_packet_t pkt;
    int rc;
    uint32_t status;
    uint32_t userAddr;
    uint32_t *hdr;
    struct timespec hwts;
    int txSeq;

    while (true) {
        // Read status and UA
        if ((rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiTEFSTA, 2*sizeof(uint32_t), &pkt)) != EOK) {
            CAN_LOG_ERR("[%s]: Unable to read TEF Status & UA to drain. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }
        status = ((uint32_t*)pkt.data)[0];
        userAddr = ((uint32_t*)pkt.data)[1];

        // Report an overflow, this shouldn't happen as TEF slots are
        // essentially assigned before tramission begins.
        if ((status & MCP25xxFD_BITFIELD(1, CiTEFSTA_TEFOVIF)) != 0) {
            CAN_LOG_ERR("[%s]: TEF overflow. This shouldn't happen! Loopback frame may be lost.", ifc->dev.ifp->if_xname);
            // Need to clear this bit.
            mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiTEFSTA, 0, MCP25xxFD_BITFIELD(1, CiTEFSTA_TEFOVIF));
        }

        // If there is nothing in the FIFO, I'm done.
        if ((status & MCP25xxFD_BITFIELD(1, CiTEFSTA_TEFNEIF)) == 0) {
            break;
        }

        // Read the event
        rc = mcp25xxfd_io_read(ifc, MCP25xxFD_MAKE_MSGOBJ_ADDR(userAddr), MCP25xxFD_TE_FIFO_SLOT_SIZE, &pkt);
        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to read event from TE_FIFO. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }
        hdr = (uint32_t*)pkt.data;

        if_inc_counter(ifc->dev.ifp, IFCOUNTER_OPACKETS, 1);

        // Check the sequence number. If it is non-zero then I have a loopback frame I can report
        txSeq = MCP25xxFD_GET_BITFIELD(hdr[1], MSGOBJ_TX_SEQ);
        if (txSeq != 0 && ifc->txLoopback[txSeq-1] != NULL) {
            generate_hwts(ifc, newEpoch, hdr[2], &hwts);

            // Get the message from the txLoopback queue and free the stop back up
            struct mbuf *m = ifc->txLoopback[txSeq-1];
            ifc->txLoopback[txSeq-1] = NULL;
            ifc->txSeq &= ~(1 << (txSeq-1));

#if CAN_LOG_LEVEL >= 3
            const canfd_frame_t *loopFrame = mtod(m, const canfd_frame_t *);
            CAN_LOG_DEBUG2("[%s]: TxEvent frame: mid=0x%08x len=%d", ifc->dev.ifp->if_xname, loopFrame->can_id, loopFrame->len);
#endif

            // Fill in the hardware timestamp now that we have recieved the frame
            m->m_flags |= M_TSTMP | M_TSTMP_HPREC;
            m->m_pkthdr.rcv_tstmp = (uint64_t)hwts.tv_sec * 1000000000 + hwts.tv_nsec;

            // Loopback frames already have a tag so leave it as is.
            sockcan_output(&ifc->dev, m, NULL);
        }

        // Advance the TEF
        rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiTEFCON, MCP25xxFD_BITFIELD(1, CiTEFCON_UINC), 0);
        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to advance TE_FIFO. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }
    }

    return EOK;
}

/*
 * Push one frame into the chip's TX_FIFO. Caller holds io_mtx.
 *
 * Returns ENOBUFS with m untouched when the FIFO has no room, so the caller can
 * leave it queued and wait for the "room again" interrupt. On every other
 * return m has been consumed -- handed to an echo slot, or freed.
 */
static int mcp25xxfd_write_frame(mcp25xxfd_dev_t *ifc, struct mbuf *m) {
    KASSERT(ifc != NULL, "Interface is NULL");
    KASSERT(m != NULL, "Message is null");

    sockcan_dev_t *dev = &ifc->dev;
    mcp25xxfd_io_write_packet_t pkt;
    int rc;
    uint32_t status;
    uint32_t userAddr;
    uint32_t *hdr;
    int txSeq = 0;

    const canfd_frame_t *frame = mtod(m, const canfd_frame_t *);

    struct m_tag *mt = m_tag_locate(m, MTAG_CAN, MTAG_CAN_INFO, NULL);
    can_tag_t *ct = (can_tag_t *)mt;

    CAN_LOG_DEBUG2("[%s]: %s(mid=0x%08x, len=%d)", ifc->dev.ifp->if_xname, __func__, frame->can_id, frame->len);

    // Read status and UA
    rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiFIFOSTAm(TX_FIFO), 2*sizeof(uint32_t), (mcp25xxfd_io_read_packet_t*)&pkt);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to read TX_FIFO Status & UA. Tx frame is lost. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        if_inc_counter(ifc->dev.ifp, IFCOUNTER_OERRORS, 1);
        rc = EOK;
        goto exit;
    }
    status = ((uint32_t*)pkt.data)[0];
    userAddr = ((uint32_t*)pkt.data)[1];

    // Check to see if there is room in the FIFO
    if ((status & MCP25xxFD_BITFIELD(1, CiFIFOSTAm_TFNRFNIF)) == 0) {
        // There is no room. Enable the interrupt to tell us when the
        // TX_FIFO is less than half-full. This gives the HW a chance to drain
        // the FIFO.
        CAN_LOG_DEBUG2("[%s]: %s(mid=0x%08x, len=%d) -> TX_FIFO is FULL. Deferring.",
                   ifc->dev.ifp->if_xname, __func__, frame->can_id, frame->len);
        rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiFIFOCONm(TX_FIFO), MCP25xxFD_BITFIELD(1, CiFIFOCONm_TFHRFHIE), 0);
        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to enable TX IRQ. Transmission will stall. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        }
        return ENOBUFS;
    }

    // Populate the message
    hdr = (uint32_t*)pkt.data;
    hdr[0] = frame->can_id;
    hdr[1] = 0;
    if ((frame->can_id & CAN_EFF_FLAG) != 0) {
        hdr[0] &= CAN_EFF_MASK;

        /* In the CAN2.0B standard an identifier comes in either standard
         * or extended format.
         * - The standard format is 11bits in length and consists of just the SID
         * - The extended format is 29bits in length and consists of the SID
         *   and the EID where the SID occupies bits [28:18] and the EID occupies
         *   bits [17:0].
         *
         * Contrast this to the MID exposed by SocketCAN. Here we have either a
         * 11 bit Standard-frame identifier (SFF) or 20 bit Extended-frame identifier
         * (EFF). To make things easier at the API level, the lowest order bit
         * of both of these is placed at bit 0 of the MID.
         *
         * This means that for an extended-frame, I need to shift things around
         * to make the EID/SID come out correct.
         */
        uint32_t sid = (hdr[0] >> 18) & 0x7FF;
        uint32_t eid = hdr[0] & 0x3FFFF;
        hdr[0] = MCP25xxFD_BITFIELD(sid, MSGOBJ_SID) | MCP25xxFD_BITFIELD(eid, MSGOBJ_EID);

        hdr[1] |= MCP25xxFD_BITFIELD(1, MSGOBJ_IDE);
    } else {
        hdr[0] &= CAN_SFF_MASK;
    }
    if ((frame->can_id & CAN_RTR_FLAG) != 0) {
        hdr[1] |= MCP25xxFD_BITFIELD(1, MSGOBJ_RTR);
    }

    if ((ct->can.flags & CANMSG_FLAG_FD_FRAME) != 0) {
        if ((frame->flags & CANFD_FLAGS_BRS) != 0) {
            hdr[1] |= MCP25xxFD_BITFIELD(1, MSGOBJ_BRS);
        }
        if ((frame->flags & CANFD_FLAGS_FDF) != 0) {
            hdr[1] |= MCP25xxFD_BITFIELD(1, MSGOBJ_FDF);
        }
    }

    rc = len_to_dlc(frame->len);
    if (rc == -1) {
        CAN_LOG_ERR("[%s]: Bad len=%u specified for frame. Tx frame is lost.", ifc->dev.ifp->if_xname, frame->len);
        if_inc_counter(ifc->dev.ifp, IFCOUNTER_OERRORS, 1);
        rc = EOK;
        goto exit;
    }
    hdr[1] |= MCP25xxFD_BITFIELD((uint8_t)rc, MSGOBJ_DLC);
    memcpy(&hdr[2], frame->data, frame->len);

    // If hardware loopback is enabled and this frame has a real sender, pick a
    // slot for it and set that as the sequence # so the TEF interrupt can echo
    // it back with the right sender identity once the HW confirms the TX.
    if ((dev->enabled_caps & CANIFC_CAP_LOOPBACK) != 0 && ct->can.sender != NULL) {
        txSeq = __builtin_ffs(~ifc->txSeq);
        if (txSeq == 0 || ifc->txLoopback[txSeq-1] != NULL) {
            // This shouldn't happen. I've already checked that a slot in the TX_FIFO
            // is available which means a slot in txSeq should be available
            CAN_LOG_ERR("[%s]: No free txSeq slot! txSeq=%d", ifc->dev.ifp->if_xname, txSeq);
            txSeq = 0;
        } else {
            // We don't copy the frame output of the loopback meaning in this case we can't free
            // the frame. the RX path will free it once the frame comes back.
            ifc->txSeq |= (1 << (txSeq-1));
            ifc->txLoopback[txSeq-1] = m;
            // Note M can no longer be used after this point.
            m = NULL;
        }
    }
    hdr[1] |= MCP25xxFD_BITFIELD(txSeq, MSGOBJ_TX_SEQ);

    // Write the frame to HW
    rc = mcp25xxfd_io_write(ifc, MCP25xxFD_MAKE_MSGOBJ_ADDR(userAddr), MCP25xxFD_TX_FIFO_SLOT_SIZE, &pkt);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to write message to TX_FIFO. Tx frame is lost. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        if_inc_counter(ifc->dev.ifp, IFCOUNTER_OERRORS, 1);
        rc = EOK;
        goto exit;
    }

    // Advance the queue and start transmission
    rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiFIFOCONm(TX_FIFO),
            MCP25xxFD_BITFIELD(1, CiFIFOCONm_UINC) | MCP25xxFD_BITFIELD(1, CiFIFOCONm_TXREQ),
            0);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to request transmission and/or advance RX_FIFO. Tx frame may be lost. Error=%s",
                ifc->dev.ifp->if_xname, strerror(rc));
        if_inc_counter(ifc->dev.ifp, IFCOUNTER_OERRORS, 1);
    }
    rc = EOK;

exit:
    if (m != NULL && txSeq != 0) {
        // Bailed out after taking a slot, so no TEF event will ever arrive for
        // it -- give the slot back and fall through to freeing the mbuf here.
        ifc->txLoopback[txSeq-1] = NULL;
        ifc->txSeq &= ~(1 << (txSeq-1));
    }
    m_freem(m);
    return rc;
}

/*
 * Drain the software TX queue into the chip. Caller holds io_mtx.
 */
static void mcp25xxfd_transmit_locked(mcp25xxfd_dev_t *ifc) {
    struct ifnet *ifp = ifc->dev.ifp;
    struct mbuf *m;

    MCP_ASSERT_LOCKED(ifc);

    if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
        return;
    }

    while ((m = drbr_peek(ifp, ifc->tx_br)) != NULL) {
        if (mcp25xxfd_write_frame(ifc, m) == ENOBUFS) {
            drbr_putback(ifp, ifc->tx_br, m);
            ifp->if_drv_flags |= IFF_DRV_OACTIVE;
            break;
        }
        // write_frame owns m from here on, whatever the outcome.
        drbr_advance(ifp, ifc->tx_br);
    }
}

// Task handler: drains whatever if_transmit queued while the chip was busy.
static void mcp25xxfd_transmit_deferred(void *arg, int pending) {
    mcp25xxfd_dev_t *ifc = arg;

    MCP_LOCK(ifc);
    mcp25xxfd_transmit_locked(ifc);
    MCP_UNLOCK(ifc);
}

/*
 * if_transmit: queue only. The chip is driven from mcp25xxfd_transmit_locked,
 * either inline here when nothing else holds the bus, or on the task thread --
 * a socket write must never block behind another thread's SPI traffic.
 */
static int mcp25xxfd_transmit(struct ifnet *ifp, struct mbuf *m) {
    mcp25xxfd_dev_t *ifc = ifp->if_softc;
    int rc;

    if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
        m_freem(m);
        return ENETDOWN;
    }

    // Queue up a message and poke the tx queue
    MCP_LOCK(ifc);
    rc = drbr_enqueue(ifp, ifc->tx_br, m);
    taskqueue_enqueue(ifc->tx_taskq, &ifc->tx_task);
    MCP_UNLOCK(ifc);
    return rc;
}

static int mcp25xxfd_bittiming(sockcan_dev_t *dev, can_bittiming_t *bt, bool set) {
    mcp25xxfd_dev_t *ifc = (mcp25xxfd_dev_t *)dev;

    CAN_LOG_NTC("[%s]: %s(bitrate=%u, clkrate=%u, sample_pt=%u.%u%%, tq=%uns, "
           "prop_seg=%u, phase_seg1=%u, phase_seg2=%u, sjw=%u, brp=%u, set=%s)\n",
           ifc->dev.ifp->if_xname, __func__, bt->bitrate, bt->clkrate, bt->sample_point / 10, bt->sample_point % 10,
           bt->tq, bt->prop_seg, bt->phase_seg1, bt->phase_seg2, bt->sjw, bt->brp,
           set ? "true" : "false");

    MCP_LOCK(ifc);
    int rc = mcp25xxfd_do_bittiming(ifc, bt, !set, false);
    MCP_UNLOCK(ifc);
    return rc;
}

static int mcp25xxfd_data_bittiming(sockcan_dev_t *dev, can_bittiming_t *bt, bool set) {
    mcp25xxfd_dev_t *ifc = (mcp25xxfd_dev_t *)dev;

    CAN_LOG_NTC("[%s]: %s(bitrate=%u, clkrate=%u, sample_pt=%u.%u%%, tq=%uns, "
           "prop_seg=%u, phase_seg1=%u, phase_seg2=%u, sjw=%u, brp=%u, "
           "set=%s)\n",
           ifc->dev.ifp->if_xname, __func__, bt->bitrate, bt->clkrate, bt->sample_point / 10, bt->sample_point % 10,
           bt->tq, bt->prop_seg, bt->phase_seg1, bt->phase_seg2, bt->sjw, bt->brp,
           set ? "true" : "false");

    MCP_LOCK(ifc);
    int rc = mcp25xxfd_do_bittiming(ifc, bt, !set, true);
    MCP_UNLOCK(ifc);
    return rc;
}

static void handle_can_error_event(mcp25xxfd_dev_t *ifc) {
    int rc;
    mcp25xxfd_io_read_packet_t pkt;
    DBUFFER_ALLOCA(db, 2048);
    uint32_t trec;
    uint32_t diag1;

    mcp25xxfd_dbuffer_append(&db, "[%s]: CAN Bus Error - ", ifc->dev.ifp->if_xname);

    // To figure out the specific event that happened. I need to read more registers.
    rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiTREC, 3*4, &pkt);
    if (rc != EOK) {
        mcp25xxfd_dbuffer_append(&db, "Unable to read CiTREC, CiBDIAG0, and CiBDIAG1. Error=%s", strerror(rc));
        CAN_LOG_ERR("%s", db.orig);
        goto exit;
    }

    trec = ((uint32_t*)pkt.data)[0];
    diag1 = ((uint32_t*)pkt.data)[2];
    mcp25xxfd_dbuffer_append(&db, "CiTREC=0x%08x, CiBDIAG1=0x%08x", trec, diag1);

    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_TXBOERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, "\n  HW auto-recovered from bus-off error state");
    }
    if ((trec & MCP25xxFD_BITFIELD(1, CiTREC_TXBO)) != 0) {
        mcp25xxfd_dbuffer_append(&db, "\n  Tx in Bus-Off state");
    }
    if ((trec & MCP25xxFD_BITFIELD(1, CiTREC_TXBP)) != 0) {
        mcp25xxfd_dbuffer_append(&db, "\n  Tx in Error-Passive state. TEC=%d", MCP25xxFD_GET_BITFIELD(trec, CiTREC_TEC));
    }
    if ((trec & MCP25xxFD_BITFIELD(1, CiTREC_RXBP)) != 0) {
        mcp25xxfd_dbuffer_append(&db, "\n  Rx in Error-Passive state. REC=%d", MCP25xxFD_GET_BITFIELD(trec, CiTREC_REC));
    }
    if ((trec & MCP25xxFD_BITFIELD(1, CiTREC_TXWARN)) != 0) {
        mcp25xxfd_dbuffer_append(&db, "\n  Tx in error warning state. TREC=%d", MCP25xxFD_GET_BITFIELD(trec, CiTREC_TEC));
    }
    if ((trec & MCP25xxFD_BITFIELD(1, CiTREC_RXWARN)) != 0) {
        mcp25xxfd_dbuffer_append(&db, "\n  Rx in error warning state. REC=%d", MCP25xxFD_GET_BITFIELD(trec, CiTREC_REC));
    }

    CAN_LOG_WRN("%s", db.orig);

exit:
    // Clear the TXBOERR flag in diag1
    rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiBDIAG1, 0, MCP25xxFD_BITFIELD(1, CiBDIAG1_TXBOERR));
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to clear TXBOERR. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
    }
}

static void handle_invalid_message_event(mcp25xxfd_dev_t *ifc) {
    int rc;
    union {
        mcp25xxfd_io_read_packet_t r;
        mcp25xxfd_io_write_packet_t w;
    } pkt;
    uint32_t diag0;
    uint32_t diag1;
    DBUFFER_ALLOCA(db, 2048);

    mcp25xxfd_dbuffer_append(&db, "[%s]: Invalid message Error - ", ifc->dev.ifp->if_xname);

    // To figure out the specific event that happened. I need to read more registers.
    rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiBDIAG0, 2*4, &pkt.r);
    if (rc != EOK) {
        mcp25xxfd_dbuffer_append(&db, "Unable to read CiBDIAG0 and CiBDIAG1. Error=%s", strerror(rc));
        CAN_LOG_ERR("%s", db.orig);
        goto exit;
    }

    diag0 = ((uint32_t*)pkt.r.data)[0];
    diag1 = ((uint32_t*)pkt.r.data)[2];
    mcp25xxfd_dbuffer_append(&db, "CiBDIAG0=0x%08x, CiBDIAG1=0x%08x", diag0, diag1);

    mcp25xxfd_dbuffer_append(&db, "\n  Error Free Message Count = %d", MCP25xxFD_GET_BITFIELD(diag1, CiBDIAG1_EFMSGCNT));
    mcp25xxfd_dbuffer_append(&db, "\n  Nominal Bit Rate Error Counts: Tx=%d Rx=%d",
            MCP25xxFD_GET_BITFIELD(diag0, CiBDIAG0_NTERRCNT),  MCP25xxFD_GET_BITFIELD(diag0, CiBDIAG0_NRERRCNT));
    if (ifc->cfg.fdMode != FD_MODE_OFF) {
        mcp25xxfd_dbuffer_append(&db, "\n  Data Bit Rate Error Counts: Tx=%d Rx=%d",
                MCP25xxFD_GET_BITFIELD(diag0, CiBDIAG0_DTERRCNT),  MCP25xxFD_GET_BITFIELD(diag0, CiBDIAG0_DRERRCNT));
    }

    mcp25xxfd_dbuffer_append(&db, "\n  Error Flags:");
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_NBIT0ERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " NBIT0ERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_NBIT1ERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " NBIT1ERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_NACKERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " NACKERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_NFORMERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " NFORMERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_NSTUFERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " NSTUFERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_NCRCERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " NCRCERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_DBIT0ERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " DBIT0ERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_DBIT1ERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " DBIT1ERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_DFORMERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " DFORMERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_DSTUFERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " DSTUFERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_DCRCERR)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " DCRCERR");
    }
    if ((diag1 & MCP25xxFD_BITFIELD(1, CiBDIAG1_DLCMM)) != 0) {
        mcp25xxfd_dbuffer_append(&db, " DLCMM");
    }

    CAN_LOG_WRN("%s", db.orig);

exit:
    // Clear the counters and states in the diag registers
    memset(pkt.w.data, 0, sizeof(uint32_t)*2);
    if ((rc = mcp25xxfd_io_write(ifc, MCP25xxFD_CiBDIAG0, sizeof(uint32_t)*2, &pkt.w)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to clear can bus error state/counters. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
    }
}

// An event occured - IRQ has fired
int mcp25xxfd_event_cb(void *ctx, int event, int irq) {
    mcp25xxfd_dev_t *ifc = ctx;
    uint32_t cintValue;
    uint32_t regValue;
    int rc;
    bool workDone;
    uint32_t cintClear;
    struct timespec newEpoch;
    bool newEpochValid;

    CAN_LOG_DEBUG2("[%s]: %s(event=%d, irq=%d)", ifc->dev.ifp->if_xname, __func__, event, irq);

    MCP_LOCK(ifc);

    while (true) {
        workDone = false;
        cintClear = 0;
        newEpochValid = false;

        // Figure out what interrupts have fired
        cintValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiINT);
        if (errno != EOK) {
            rc = errno;
            CAN_LOG_ERR("[%s]: Unable to read interrupt status. Disabling interrupts. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiINT, 0);
            break;
        }
        CAN_LOG_DEBUG2("[%s]: CiINT=0x%08x", ifc->dev.ifp->if_xname, cintValue);

        // Did our time counter overflow? We want to get the new value
        // as quickly as possible to try and correlate this time
        // with CiTBC==0
        //
        // I have to be a bit careful with changing the epoch because
        // any frames currently in the RX_FIFO might be associated with
        // either the old or the new epoch.
        //
        // So just get the new epoch, don't switch to it until
        // after I've drained the FIFO
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_TBCIF) != 0) {
            // Get timestamp, which will hopefully be correlated to CiTBC==0.
            // It probably won't be but it should be close.
            workDone = true;
            if (clock_gettime(CLOCK_MONOTONIC, &newEpoch) == 0) {
                CAN_LOG_INFO("[%s]: Time Base Counter roll-over. New epoch=(secs=%lu, nsecs=%lu)",
                        ifc->dev.ifp->if_xname, newEpoch.tv_sec, newEpoch.tv_nsec);
                newEpochValid = true;
            } else {
                rc = errno;
                CAN_LOG_WRN("[%s]: Unable to get timestamp epoch. Timestamps will likely be bunk. Error=%s",
                           ifc->dev.ifp->if_xname, strerror(rc));
            }
            cintClear |= MCP25xxFD_BITFIELD(1, CiINT_TBCIF);
        }

        // Did our RX_FIFO overflow?
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_RXOVIF) != 0) {
            workDone = true;
            CAN_LOG_WRN("[%s]: RX queue overflow!", ifc->dev.ifp->if_xname);
            // To clear have to write directly to the FIFO control reg
            rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiFIFOSTAm(RX_FIFO), 0,
                                         MCP25xxFD_BITFIELD(1, CiFIFOSTAm_RXOVIF));
            if (rc != EOK) {
                CAN_LOG_WRN("[%s]: Unable to clear RX queue overflow. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            }
        }

        // Was there a system error?
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_SERRIF) != 0) {
            workDone = true;
            CAN_LOG_WRN("[%s]: System bus error! Interface may need to be brought down then up to recover.", ifc->dev.ifp->if_xname);
            cintClear |= MCP25xxFD_BITFIELD(1, CiINT_SERRIF);
        }

        // CAN Bus error
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_CERRIF) != 0) {
            workDone = true;
            handle_can_error_event(ifc);
            cintClear |= MCP25xxFD_BITFIELD(1, CiINT_CERRIF);
        }

        // Invalid message
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_IVMIF) != 0) {
            workDone = true;
            handle_invalid_message_event(ifc);
            cintClear |= MCP25xxFD_BITFIELD(1, CiINT_IVMIF);
        }

        // If there was a mode switch make sure it was to what we expected.
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_MODIF) != 0) {
            workDone = true;
            cintClear |= MCP25xxFD_BITFIELD(1, CiINT_MODIF);

            regValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiCON);
            if (errno == EOK) {
                if (MCP25xxFD_GET_BITFIELD(regValue, CiCON_OPMOD) == ifc->opMode) {
                    CAN_LOG_NTC("[%s]: Changed to opmode %s (%d)",
                              ifc->dev.ifp->if_xname, opmode_to_string(ifc->opMode), ifc->opMode);
                } else {
                    CAN_LOG_WRN("[%s]: Unexpected opmode change. Want=%s(%d), have=%s(%d), req=%s(%d)",
                               ifc->dev.ifp->if_xname,
                               opmode_to_string(ifc->opMode), ifc->opMode,
                               opmode_to_string(MCP25xxFD_GET_BITFIELD(regValue, CiCON_OPMOD)), MCP25xxFD_GET_BITFIELD(regValue, CiCON_OPMOD),
                               opmode_to_string(MCP25xxFD_GET_BITFIELD(regValue, CiCON_REQOP)), MCP25xxFD_GET_BITFIELD(regValue, CiCON_REQOP));
                }
            } else {
                CAN_LOG_ERR("[%s]: Unable to read CiCON to get current opmode. Error=%s", ifc->dev.ifp->if_xname, strerror(errno));
            }
        }

        // Drain the RX_FIFO if it isn't empty
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_RXIF) != 0) {
            workDone = true;
            if ((rc = mcp25xxfd_drain_rx_fifo(ifc, newEpochValid ? &newEpoch : NULL)) != EOK) {
                // Unable to drain RX_FIFO. It's interrupt will keep firing so disable it.
                CAN_LOG_ERR("[%s]: Unable to drain RX FIFO. Disabling its interrupts. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
                mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiFIFOCONm(RX_FIFO), 0, MCP25xxFD_BITFIELD(1, CiFIFOCONm_TFNRFNIE));
            }
        }

        // Deal with the TEF interrupt
        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_TEFIF) != 0) {
            workDone = true;
            if ((rc = mcp25xxfd_handle_te_interrupt(ifc, newEpochValid ? &newEpoch : NULL)) != EOK) {
                // Unable to drain TEF. It's interrupt will keep firing so disable it.
                CAN_LOG_ERR("[%s]: Unable to handle TE interrupt. Disabling its interrupts. Error=%s",
                    ifc->dev.ifp->if_xname, strerror(rc));
                mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiTEFCON, 0, MCP25xxFD_BITFIELD(1, CiTEFCON_TEFNEIE));
            }
        }

        // RX_FIFO and TEF are drained, can store the new epoch now.
        if (newEpochValid) ifc->epoch = newEpoch;

        if (MCP25xxFD_GET_BITFIELD(cintValue, CiINT_TXIF) != 0) {
            workDone = true;
            CAN_LOG_DEBUG2("[%s]: TX_FIFO less than half-full. Disabling interrupt", ifc->dev.ifp->if_xname);
            mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiFIFOCONm(TX_FIFO), 0, MCP25xxFD_BITFIELD(1, CiFIFOCONm_TFHRFHIE));

            // Room again: clear the stall and let the task push whatever is
            // still queued. Deferred rather than drained here so the interrupt
            // thread isn't held for a queue's worth of SPI writes.
            ifc->dev.ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
            taskqueue_enqueue(ifc->tx_taskq, &ifc->tx_task);
        }

        if (cintClear != 0) {
            // I need to manually clear some bits in CiINT
            if ((rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiINT, 0, cintClear)) != EOK) {
                CAN_LOG_ERR("[%s]: Unable to clear system interrupts. They will likely keep firing. Error=%s",
                        ifc->dev.ifp->if_xname, strerror(rc));
                break;
            }
        }

        if (!workDone) {
            rc = EOK;
            break;
        }
    }

    MCP_UNLOCK(ifc);

    ifc->platform->gpio_irq_clear(ifc->cfg.gpio);

    return rc;
}


/*
 * Free any mbufs still sitting in TX echo slots. Called when the interface is
 * stopped or torn down, because the TEF events that would otherwise have echoed
 * and freed them are never going to arrive.
 */
static void mcp25xxfd_purge_tx_echoes(mcp25xxfd_dev_t *ifc) {
    for (int i = 0; i < MCP25xxFD_MAX_FIFO_SLOTS; ++i) {
        if (ifc->txLoopback[i] != NULL) {
            m_freem(ifc->txLoopback[i]);
            ifc->txLoopback[i] = NULL;
        }
    }
    ifc->txSeq = 0;
}

/*
 * Stop the TX task and release the queue. Safe on a partially-attached device:
 * every field it touches is either allocated or still NULL. The caller must
 * have already stopped the interrupt thread, so nothing can be running here.
 */
static void mcp25xxfd_tx_teardown(mcp25xxfd_dev_t *ifc) {
    if (ifc->tx_taskq != NULL) {
        taskqueue_drain(ifc->tx_taskq, &ifc->tx_task);
        taskqueue_free(ifc->tx_taskq);
        ifc->tx_taskq = NULL;
    }
    if (ifc->tx_br != NULL) {
        drbr_free(ifc->tx_br, M_DEVBUF);
        ifc->tx_br = NULL;
    }
    mcp25xxfd_purge_tx_echoes(ifc);
    mtx_destroy(&ifc->io_mtx);
}

// Start the HW
static int mcp25xxfd_start(mcp25xxfd_dev_t *ifc, bool start) {
    int rc;
    uint32_t regValue;
    int poll;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 300*1000};

    CAN_LOG_NTC("[%s]: %s(start=%s)", ifc->dev.ifp->if_xname, __func__, start ? "true" : "false");

    if (start) {
        // If CANFD is desired, I need a data bitrate
        if (mcp25xxfd_fd_enabled(ifc) && ifc->cfg.dbps < ifc->cfg.bps) {
            CAN_LOG_ERR("[%s]: Unable to start CANFD without data bitrate.", ifc->dev.ifp->if_xname);
            return ENOTSUP;
        }

        // Reset CiTBC to 0 so that we have the most time available before overflow
        if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiTBC, 0)) != EOK) {
            CAN_LOG_ERR("[%s]: Unable to set TBC to 0. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }

        // Enable interrupts
        regValue = MCP25xxFD_BITFIELD(1, CiINT_IVMIE) |
                   MCP25xxFD_BITFIELD(0, CiINT_WAKIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_CERRIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_SERRIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_RXOVIE) |
                   MCP25xxFD_BITFIELD(0, CiINT_TXATIE) |
                   MCP25xxFD_BITFIELD(0, CiINT_SPICRCIE) |
                   MCP25xxFD_BITFIELD(0, CiINT_ECCIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_TEFIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_MODIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_TBCIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_RXIE) |
                   MCP25xxFD_BITFIELD(1, CiINT_TXIE);
        // All the flag fields will be set to 0, clearing any
        // that require the application to clear them.
        if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiINT, regValue)) != EOK) {
            CAN_LOG_ERR("[%s]: Unable to enable interrupts. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }

    } else {
        // Disable interrupts (except for MODIE) to stop them from firing while I'm down.
        // This doesn't actually clear any of the interrupts. This is intentional
        // so that they can be inspected while the interface is down.
        regValue = MCP25xxFD_BITFIELD(1, CiINT_MODIE);
        if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiINT, regValue)) != EOK) {
            CAN_LOG_ERR("[%s]: Unable to disable interrupts. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }

        // Disable the TBC
        rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiTSCON, 0, MCP25xxFD_BITFIELD(1, CiTSCON_TBCEN));
        if (rc != EOK) {
            CAN_LOG_WRN("[%s]: Unable to disable timestamp clock. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        }

        // Nothing queued survives a stop, and a restart must not begin stalled.
        drbr_flush(ifc->dev.ifp, ifc->tx_br);
        ifc->dev.ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
        mcp25xxfd_purge_tx_echoes(ifc);
    }

    // Change operating mode
    regValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiCON);
    if (errno != EOK) {
        rc = errno;
        CAN_LOG_ERR("[%s]: Unable to read CiCON to get current opmode. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        return rc;
    }
    if (start) {
        if (mcp25xxfd_fd_enabled(ifc)) {
            ifc->opMode = MCP25xxFD_OPMODE_NORMAL;
        } else {
            ifc->opMode = MCP25xxFD_OPMODE_NORMAL_CAN_2_0;
        }
    } else {
        ifc->opMode = MCP25xxFD_OPMODE_CONFIG;
    }
    CAN_LOG_DEBUG2("[%s]: Changing to operating mode: %s (%d)", ifc->dev.ifp->if_xname, opmode_to_string(ifc->opMode), ifc->opMode);
    MCP25xxFD_SET_BITFIELD(regValue, (mcp25xxfd_fd_enabled(ifc) && ifc->cfg.fdMode == FD_MODE_ON) ? 1 : 0, CiCON_ISOCRCEN);
    MCP25xxFD_SET_BITFIELD(regValue, ifc->opMode, CiCON_REQOP);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiCON, regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to write CiCON to set opmode. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        return rc;
    }

    if (start) {
        // Start TBC
        rc = mcp25xxfd_io_bitset_word(ifc, MCP25xxFD_CiTSCON, MCP25xxFD_BITFIELD(1, CiTSCON_TBCEN), 0);
        if (rc != EOK) {
            CAN_LOG_WRN("[%s]: Error enabling timestamp clock. HW timestamps will be bad. Error=%s",
                    ifc->dev.ifp->if_xname, strerror(rc));
        }
        // Get timestamp, which will hopefully be correlated to CiTBC==0
        // In theory, CiTBC isn't supposed to start incrementing until we are out
        // of the CONFIG opmode
        if (clock_gettime(CLOCK_MONOTONIC, &ifc->epoch) == -1) {
            rc = errno;
            CAN_LOG_WRN("[%s]: Unable to get timestamp epoch. HW timestamps will be bad. Error=%s",
                       ifc->dev.ifp->if_xname, strerror(rc));
        }
    }

    // Poll for the opmode to change. In theory there is an interrupt I could wait
    // for as well but libsockcan isn't set up for that sort of scenario. The mode
    // MUST be changed by the time this API returns and the interrupt event handler
    // will be blocked on this ifc's mutex until this API returns.
    //
    // The mode change will only happen when the bus is not busy. So it isn't
    // actively receiving or transmitting. How long this will take really depends
    // on how many frames we have queued in HW to transmit.
    //
    // It doesn't _really_ matter/ if I wait longer than I need to. Other than
    // CAN communication will be delayed (and thus CANFrames might get missed).
    // So ideally we want to wait as little as possible.
    //
    // That said, the range for how long it takes is rather large. Try to deal
    // with this by setting up an initial poll tailored for the two most common
    // bitrates.
    // At 1Mbs a frame takes ~47-125us.
    // At 500Kbps a frame takes ~94-250us
    // By default the TX_FIFO is 7 frames deep, but in theory could be as high as 32.
    // That means it could take up to 250*32=8000us to clear, but more likely it will
    // be 250*7=1750us. Assume that the queue won't really be full and try to poll
    // every 300us. I'm doing it 10 times for a max delay of 3ms.
    rc = ETIMEDOUT;
    for (poll = 0; poll < 10; ++poll) {
        if (poll != 0) nanospin(&ts);
        regValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiCON);
        if (errno == EOK && MCP25xxFD_GET_BITFIELD(regValue, CiCON_OPMOD) == ifc->opMode) {
            rc = EOK;
            break;
        }
    }
    if (rc != EOK) {
        CAN_LOG_WRN("[%s]: %s has exceeded initial poll....", ifc->dev.ifp->if_xname, start ? "Starting" : "Stopping");

        // I still haven't transitioned. This could be because the bitrate is really slow
        // or the bus is really contended or whatever. I'm going to keep polling but this
        // time I'll do a poll every ms (or so).
        for (poll = 0; poll < 10; ++poll) {
            if (poll != 0) usleep(1000);
            regValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiCON);
            if (errno == EOK && MCP25xxFD_GET_BITFIELD(regValue, CiCON_OPMOD) == ifc->opMode) {
                rc = EOK;
                break;
            }
        }

        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to %s as HW hasn't changed to correct mode.",
                 ifc->dev.ifp->if_xname, start ? "start" : "stop");
            return EIO;
        }
    }

    return 0;
}

// Drop everything still waiting in the software TX queue.
static void mcp25xxfd_qflush(struct ifnet *ifp) {
    mcp25xxfd_dev_t *ifc = ifp->if_softc;

    MCP_LOCK(ifc);
    drbr_flush(ifp, ifc->tx_br);
    MCP_UNLOCK(ifc);

    if_qflush(ifp);
}

static int mcp25xxfd_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data) {
    struct ifreq    *ifr = (struct ifreq *)data;
    mcp25xxfd_dev_t *ifc = ifp->if_softc;
    int rc;

    switch (cmd) {
        case SIOCSIFFLAGS: {
            bool up = (ifp->if_flags & IFF_UP) != 0;
            MCP_LOCK(ifc);
            rc = mcp25xxfd_start(ifc, up);
            MCP_UNLOCK(ifc);
            if (rc != EOK) {
                return rc;
            }
            if (up) {
                ifp->if_drv_flags |= IFF_DRV_RUNNING;
            } else {
                ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
            }
            break;
        }
        case SIOCSIFMTU:
            // MTU is set when the device starts so just enable it only when not running.
            if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
                CAN_LOG_ERR("[%s]: Bring the interface down before changing the MTU.", ifc->dev.ifp->if_xname);
                return EBUSY;
            }
            if (ifr->ifr_mtu == CAN_MTU) {
                ifp->if_mtu = CAN_MTU;
                ifc->dev.enabled_caps &= ~CANIFC_CAP_CANFD;
            } else if (ifr->ifr_mtu == CANFD_MTU && (ifc->dev.caps & CANIFC_CAP_CANFD)) {
                // Caught here rather than at start so the failure lands on the
                // ioctl that asked for FD.
                if (ifc->cfg.dbps < ifc->cfg.bps) {
                    CAN_LOG_ERR("[%s]: Unable to enable CANFD without a data bitrate.", ifc->dev.ifp->if_xname);
                    return EINVAL;
                }
                ifp->if_mtu = CANFD_MTU;
                ifc->dev.enabled_caps |= CANIFC_CAP_CANFD;
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

static int platform_init(mcp25xxfd_dev_t *ifc) {
    struct utsname utsname;
    int rc;

    // Try to auto-detect the platform if directed
    if (ifc->cfg.platform == PLATFORM_AUTO) {
        if (uname(&utsname) == -1) {
            rc = errno;
            CAN_LOG_ERR("[%s]: Unable to query uname. Err=%s", ifc->dev.ifp->if_xname, strerror(rc));
            return rc;
        }

        if (strcmp(utsname.machine, "RaspberryPi4B") == 0 ||
            strcmp(utsname.machine, "RaspberryPi400") == 0 ||
            strcmp(utsname.machine, "RaspberryPiCM4") == 0 ||
            strcmp(utsname.machine, "RaspberryPiCM4S") == 0)
        {
            CAN_LOG_NTC("[%s]: Auto detected RPi4 or one of its variants.", ifc->dev.ifp->if_xname);
            ifc->cfg.platform = PLATFORM_RPI4;
        } else if (strcmp(utsname.machine, "RaspberryPi5") == 0 ||
                   strcmp(utsname.machine, "RaspberryPiCM5") == 0 ||
                   strcmp(utsname.machine, "RaspberryPiCM5Lite") == 0 ||
                   strcmp(utsname.machine, "RaspberryPi500") == 0)
        {
            CAN_LOG_NTC("[%s]: Auto detected RPi5 or one of its variants.", ifc->dev.ifp->if_xname);
            ifc->cfg.platform = PLATFORM_RPI5;
        } else {
            CAN_LOG_ERR("[%s]: Unrecognized or unsupported platform: %s", ifc->dev.ifp->if_xname, utsname.machine);
            return ENOTSUP;
        }
    }

    switch (ifc->cfg.platform) {
        case PLATFORM_RPI4:
            ifc->platform = &Mcp25xxfdPlatformRpi4;
            break;
        case PLATFORM_RPI5:
            ifc->platform = &Mcp25xxfdPlatformRpi5;
            break;
        case PLATFORM_GENERIC:
            ifc->platform = &Mcp25xxfdPlatformGeneric;
            break;
        default:
            CAN_LOG_ERR("[%s]: Unrecognized or unsupported platform type: %d", ifc->dev.ifp->if_xname, ifc->cfg.platform);
            return ENOTSUP;
    }

    return ifc->platform->initialize();
}

#define MCP25XXFD_MAX_UNITS 4

// Used to walk the devices on shutdown
static LIST_HEAD(, mcp25xxfd_dev) s_devs = LIST_HEAD_INITIALIZER(s_devs);

static mcp25xxfd_dev_t *mcp25xxfd_attach_one(int unit) {
    int rc;
    uint32_t regValue;
    int irq;

    struct ifc_data ifd = {.unit = unit};
    mcp25xxfd_dev_t *ifc = (mcp25xxfd_dev_t *)sockcan_alloc("can", &ifd, sizeof(*ifc));
    if (ifc == NULL) {
        CAN_LOG_ERR("sockcan_alloc failed");
        return NULL;
    }
    ifc->fd = -1;

    // Set up the TX queue and its task before anything can reach if_transmit or
    // the interrupt thread.
    mtx_init(&ifc->io_mtx, "mcp25xxfd", NULL, MTX_DEF);

    ifc->tx_br = buf_ring_alloc(MCP25XXFD_TX_QUEUE_LEN, M_DEVBUF, M_NOWAIT, &ifc->io_mtx);
    if (ifc->tx_br == NULL) {
        CAN_LOG_ERR("[%s]: buf_ring_alloc failed", ifc->dev.ifp->if_xname);
        rc = ENOMEM;
        goto exit;
    }

    TASK_INIT(&ifc->tx_task, 0, mcp25xxfd_transmit_deferred, ifc);
    ifc->tx_taskq = taskqueue_create(ifc->dev.ifp->if_xname, M_NOWAIT, taskqueue_thread_enqueue, &ifc->tx_taskq);
    if (ifc->tx_taskq == NULL) {
        CAN_LOG_ERR("[%s]: taskqueue_create failed", ifc->dev.ifp->if_xname);
        rc = ENOMEM;
        goto exit;
    }
    taskqueue_start_threads(&ifc->tx_taskq, 1, PI_NET, "%s_tx", ifc->dev.ifp->if_xname);

    if ((rc = mcp25xxfd_load_config(unit, &ifc->cfg)) != EOK) {
        CAN_LOG_ERR("[%s]: Invalid configuration", ifc->dev.ifp->if_xname);
        goto exit;
    }

    CAN_LOG_NTC("[%s]: Attaching (dev=%s gpio=%d)", ifc->dev.ifp->if_xname, ifc->cfg.dev, ifc->cfg.gpio);

    // Initialize the platform for this interface.
    if ((rc = platform_init(ifc)) != EOK) {
        goto exit;
    }

    // Connect to the HW over SPI
    ifc->fd = open(ifc->cfg.dev, O_RDWR);
    if (ifc->fd == -1) {
        rc = errno;
        CAN_LOG_ERR("[%s]: Unable to open '%s'. Error=%s", ifc->dev.ifp->if_xname, ifc->cfg.dev, strerror(rc));
        goto exit;
    }
    // Push a reset, this should put it into the CONFIG opmode.
    ifc->opMode = MCP25xxFD_OPMODE_CONFIG;
    if ((rc = mcp25xxfd_io_reset(ifc)) != EOK) {
        goto exit;
    }

    // Configure the MCP25xxFD part.
    // The MCP2518FD Specific Registers have ok defaults after reset.
    // For the CAN FD Controller bits
    // - Enable FIFO1 as the TXFIFO, 32 entires w/ up to 64 byte payloads
    // - Enable FIFO2 as the RXFIFO, 32 entries w/ up to 64 byte payloads
    // - Disable FIFO3-FIFO31
    // - Filter0 is disabled (would be used for TXQ)
    // - Filter1 is enabled to receive everything (into TXFIFO)
    // - Filter2-Filter31 are disabled
    // CiCON
    regValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiCON);
    if (errno != EOK) {
        rc = errno;
        CAN_LOG_ERR("[%s]: Unable to get initial value of %s. Error=%s", ifc->dev.ifp->if_xname, "CiCON", strerror(rc));
        goto exit;
    }
    // - Disable filtering on CAN data
    MCP25xxFD_SET_BITFIELD(regValue, 0, CiCON_DNCNT);
    // - Allow BRS (Bit-Rate-Switching)
    MCP25xxFD_SET_BITFIELD(regValue, 0, CiCON_BRSDIS);
    // - Disable TXQ (in favour of using an explicit FIFO to keep transmission order same as submission order)
    MCP25xxFD_SET_BITFIELD(regValue, 0, CiCON_TXQEN);
    // - No delay in transmissions
    MCP25xxFD_SET_BITFIELD(regValue, 0, CiCON_TXBWS);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiCON, regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to set initial value of %s. Error=%s", ifc->dev.ifp->if_xname, "CiCON", strerror(rc));
        goto exit;
    }

    // Timestamping based off SYSCLK. If I aim for microsecond accuracy that gives a rollover of around
    // 71 minutes. Configure this via CiTSCON, but don't enable yet. Will do then when the interface
    // is brought up.
    regValue = mcp25xxfd_io_read_word(ifc, MCP25xxFD_CiTSCON);
    if (errno != EOK) {
        rc = errno;
        CAN_LOG_ERR("[%s]: Unable to get initial value of %s. Error=%s", ifc->dev.ifp->if_xname, "CiTSCON", strerror(rc));
        goto exit;
    }
    // Configure SYSCLOCK divider to give a timestamp clock running at 1MHz (ie: 1us accuracy)
    // Minimum oscillator is 2MHz.
    MCP25xxFD_SET_BITFIELD(regValue, (ifc->cfg.osc / 1000000) - 1, CiTSCON_TBCPRE);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiTSCON, regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to set initial value of %s. Error=%s", ifc->dev.ifp->if_xname, "CiTSCON", strerror(rc));
        goto exit;
    }

    // Enable the opMode switch interrupt.
    // All other interrupts will be enabled when the interface is started.
    regValue = MCP25xxFD_BITFIELD(1, CiINT_MODIE);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiINT, regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to disable interrupts. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        goto exit;
    }

    // Program TEF (Transmit Event FIFO)
    regValue =   MCP25xxFD_BITFIELD(1, CiTEFCON_TEFNEIE)
               | MCP25xxFD_BITFIELD(0, CiTEFCON_TEFHIE)
               | MCP25xxFD_BITFIELD(0, CiTEFCON_TEFFIE)
               | MCP25xxFD_BITFIELD(1, CiTEFCON_TEFOVIE)
               | MCP25xxFD_BITFIELD(1, CiTEFCON_TEFTSEN)
               | MCP25xxFD_BITFIELD(1, CiTEFCON_FRESET)
               | MCP25xxFD_BITFIELD(ifc->cfg.txFifoSize-1, CiTEFCON_FSIZE);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiTEFCON, regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to set initial value of TEF. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        goto exit;
    }
    // Program TX_FIFO
    regValue =   MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFNRFNIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFHRFHIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFERFFIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RXOVIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXATIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RXTSEN)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RTREN)
               | MCP25xxFD_BITFIELD(1, CiFIFOCONm_TXEN)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_UINC)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXREQ)
               | MCP25xxFD_BITFIELD(1, CiFIFOCONm_FRESET)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXPRI)
               | MCP25xxFD_BITFIELD(3, CiFIFOCONm_TXAT)
               | MCP25xxFD_BITFIELD(0x1F, CiFIFOCONm_TXPRI)
               | MCP25xxFD_BITFIELD(ifc->cfg.txFifoSize-1, CiFIFOCONm_FSIZE)
               | MCP25xxFD_BITFIELD(CiFIFOCONm_PLSIZE_64, CiFIFOCONm_PLSIZE);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiFIFOCONm(TX_FIFO), regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to set initial value of %s. Error=%s", ifc->dev.ifp->if_xname, "FIFO1(Tx)", strerror(rc));
        goto exit;
    }
    // Program RX_FIFO
    regValue =   MCP25xxFD_BITFIELD(1, CiFIFOCONm_TFNRFNIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFHRFHIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFERFFIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RXOVIE)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXATIE)
               | MCP25xxFD_BITFIELD(1, CiFIFOCONm_RXTSEN)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RTREN)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXEN)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_UINC)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXREQ)
               | MCP25xxFD_BITFIELD(1, CiFIFOCONm_FRESET)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXPRI)
               | MCP25xxFD_BITFIELD(3, CiFIFOCONm_TXAT)
               | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXPRI)
               | MCP25xxFD_BITFIELD(ifc->cfg.rxFifoSize-1, CiFIFOCONm_FSIZE)
               | MCP25xxFD_BITFIELD(CiFIFOCONm_PLSIZE_64, CiFIFOCONm_PLSIZE);
    if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiFIFOCONm(RX_FIFO), regValue)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to set initial value of %s. Error=%s", ifc->dev.ifp->if_xname, "FIFO1(Rx)", strerror(rc));
        goto exit;
    }
    // Program all the other FIFOs to be Rx FIFOs with no interrupts enabled.
    for (int i = FIRST_UNUSED_FIFO; i <= MCP25xxFD_MAX_FIFO; ++i) {
        regValue =   MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFNRFNIE)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFHRFHIE)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TFERFFIE)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RXOVIE)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXATIE)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RXTSEN)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_RTREN)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXEN)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_UINC)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXREQ)
                   | MCP25xxFD_BITFIELD(1, CiFIFOCONm_FRESET)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXPRI)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXAT)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_TXPRI)
                   | MCP25xxFD_BITFIELD(0, CiFIFOCONm_FSIZE)
                   | MCP25xxFD_BITFIELD(CiFIFOCONm_PLSIZE_8, CiFIFOCONm_PLSIZE);
        if ((rc = mcp25xxfd_io_write_word(ifc, MCP25xxFD_CiFIFOCONm(i), regValue)) != EOK) {
            CAN_LOG_ERR("[%s]: Unable to set initial value of FIFO%d. Error=%s", ifc->dev.ifp->if_xname, i, strerror(rc));
            goto exit;
        }
    }

    // Filter everything into RX_FIFO, all other filters disabled.
    if ((rc = mcp25xxfd_configure_filter(ifc, 0, RX_FIFO, 0, 0)) != EOK) {
        goto exit;
    }
    for (int i = 1; i <= MCP25xxFD_MAX_FILTER; ++i) {
        if ((rc = mcp25xxfd_configure_filter(ifc, i, 0, 0, 0)) != EOK) {
            goto exit;
        }
    }

    if ((rc = mcp25xxfd_configure_bit_timings(ifc)) != EOK) {
        goto exit;
    }

    // Leave it in configure mode. It will be brought out with I'm
    // actually told to start via mcp25xxfd_start

    // Set up the GPIO/IRQ and register to receive an event when it fires
    if ((rc = ifc->platform->setup_gpio(ifc->cfg.gpio)) != EOK) {
        goto exit;
    }

    if ((irq = ifc->platform->gpio_irq_get(ifc->cfg.gpio)) == -1) {
        rc = EINVAL;
        goto exit;
    }

    ifc->irq = irq;
    if ((rc = can_register_event(ifc)) != EOK) {
        CAN_LOG_ERR("[%s]: Unable to enable/register IRQ %d for GPIO %u. Error=%s", ifc->dev.ifp->if_xname, irq, ifc->cfg.gpio, strerror(rc));
        goto exit;
    }

    // Set up interface callbacks
    ifc->dev.ifp->if_transmit = mcp25xxfd_transmit;
    ifc->dev.ifp->if_qflush = mcp25xxfd_qflush;
    ifc->dev.ifp->if_ioctl = mcp25xxfd_ioctl;
    ifc->dev.bittiming = mcp25xxfd_bittiming;
    ifc->dev.data_bittiming = mcp25xxfd_data_bittiming;

    // Set interface capabilities
    ifc->dev.caps = CANIFC_CAP_CANFD | CANIFC_CAP_HW_TIMESTAMP | CANIFC_CAP_LOOPBACK;
    ifc->dev.enabled_caps = CANIFC_CAP_HW_TIMESTAMP | CANIFC_CAP_LOOPBACK;
    if (ifc->cfg.fdMode != FD_MODE_OFF) {
        ifc->dev.enabled_caps |= CANIFC_CAP_CANFD;
        ifc->dev.ifp->if_mtu = CANFD_MTU;
    }

    // Register the interface with socketcan lib
    LIST_INSERT_HEAD(&s_devs, ifc, link);
    sockcan_register(&ifc->dev);

    return ifc;

exit:
    if (ifc->fd != -1) {
        close(ifc->fd);
        ifc->fd = -1;
    }
    mcp25xxfd_tx_teardown(ifc);
    if_free(ifc->dev.ifp);
    sockcan_fini(&ifc->dev);
    return NULL;
}

/*
 * Tear down every attached instance: stop its interrupt thread, detach the
 * interface, and close its SPI fd. A bound socket keeps a pointer to its
 * device, so an instance that's still in use is left in place rather than
 * torn down out from under it (mirrors candummy_detach's EBUSY guard).
 */
void mcp25xxfd_detach(void) {
    mcp25xxfd_dev_t *ifc;
    mcp25xxfd_dev_t *tmp;

    LIST_FOREACH_SAFE (ifc, &s_devs, link, tmp) {
        mtx_lock(&ifc->dev.mtx);
        bool busy = !LIST_EMPTY(&ifc->dev.connected_sockets);
        mtx_unlock(&ifc->dev.mtx);
        if (busy) {
            CAN_LOG_WRN("%s still has bound sockets, leaving it in place", ifc->dev.ifp->if_xname);
            continue;
        }

        can_unregister_event(ifc);
        mcp25xxfd_tx_teardown(ifc);
        if (ifc->fd != -1) {
            close(ifc->fd);
            ifc->fd = -1;
        }

        if (ifc->cfg.dev != NULL) {
            free(ifc->cfg.dev, M_DEVBUF);
        }

        LIST_REMOVE(ifc, link);
        if_detach(ifc->dev.ifp);
        if_free(ifc->dev.ifp);
        sockcan_fini(&ifc->dev);
    }
}

int mcp25xxfd_attach(void) {
    int nattached = 0;

    for (int unit = 0; unit < MCP25XXFD_MAX_UNITS; ++unit) {
        if (!mcp25xxfd_unit_configured(unit)) {
            continue;
        }

        if (mcp25xxfd_attach_one(unit) == NULL) {
            CAN_LOG_ERR("can%d: attach failed", unit);
            mcp25xxfd_detach();
            return EIO;
        }

        nattached++;
    }

    return (nattached > 0) ? EOK : ENXIO;
}
