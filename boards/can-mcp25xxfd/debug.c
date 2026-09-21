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
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "state.h"
#include "mcp25xxfd.h"
#include "io.h"

#include "debug.h"

#include <sockcan_debug.h>

#define append(...) mcp25xxfd_dbuffer_append(__VA_ARGS__)

int mcp25xxfd_dbuffer_append(dbuffer_t *db, const char *fmt, ...) {
    va_list ap;
    size_t size;
    va_start(ap, fmt);
    if (db->size >= (db->cur - db->orig)) {
        size = db->size - (db->cur - db->orig);
    } else {
        size = 0;
    }
    int count = vsnprintf(db->cur, size, fmt, ap);
    va_end(ap);
    db->cur += count;
    return count;
}

int mcp25xxfd_debug_dump_specific_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db) {
    mcp25xxfd_io_read_packet_t pkt;

    int rc = mcp25xxfd_io_read(ifc, MCP25xxFD_OSC, 6*4, &pkt);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to read specific registers. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        return rc;
    }
    uint32_t *ptr = (uint32_t*)(pkt.data);

    append(db, "MCP2518FD Specific Registers\n");
    append(db, "============================\n");
    append(db, "OSC =     0x%08x\n", ptr[0]);
    append(db, "IOCON =   0x%08x\n", ptr[1]);
    append(db, "CRC =     0x%08x\n", ptr[2]);
    append(db, "ECCCON =  0x%08x\n", ptr[3]);
    append(db, "ECCSTAT = 0x%08x\n", ptr[4]);
    append(db, "DEVID =   0x%08x\n", ptr[5]);

    return EOK;
}

int mcp25xxfd_debug_dump_controller_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db) {
    mcp25xxfd_io_read_packet_t pkt;

    int rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiCON, 16*4, &pkt);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to read controller registers. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        return rc;
    }
    uint32_t *ptr = (uint32_t*)(pkt.data);

    append(db, "Controller Registers\n");
    append(db, "====================\n");
    append(db, "CiCON =    0x%08x\n", ptr[0]);
    append(db, "CiNBTCFG = 0x%08x\n", ptr[1]);
    append(db, "CiDBTCFG = 0x%08x\n", ptr[2]);
    append(db, "CiTDC =    0x%08x\n", ptr[3]);
    append(db, "CiTBC =    0x%08x\n", ptr[4]);
    append(db, "CiTSCON =  0x%08x\n", ptr[5]);
    append(db, "CiVEC =    0x%08x\n", ptr[6]);
    append(db, "CiINT =    0x%08x\n", ptr[7]);
    append(db, "CiRXIF =   0x%08x\n", ptr[8]);
    append(db, "CiTXIF =   0x%08x\n", ptr[9]);
    append(db, "CiRXOVIF = 0x%08x\n", ptr[10]);
    append(db, "CiTXATIF = 0x%08x\n", ptr[11]);
    append(db, "CiTXREQ =  0x%08x\n", ptr[12]);
    append(db, "CiTREC =   0x%08x\n", ptr[13]);
    append(db, "CiBDIAG0 = 0x%08x\n", ptr[14]);
    append(db, "CiBDIAG1 = 0x%08x\n", ptr[15]);

    return EOK;
}

int mcp25xxfd_debug_dump_controller_tef_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db) {
    mcp25xxfd_io_read_packet_t pkt;

    int rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiTEFCON, 3*4, &pkt);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to read controller TEF registers. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        return rc;
    }
    uint32_t *ptr = (uint32_t*)(pkt.data);

    append(db, "Controller TEF Registers\n");
    append(db, "========================\n");
    append(db, "CiTEFCON = 0x%08x\n", ptr[0]);
    append(db, "CiTEFSTA = 0x%08x\n", ptr[1]);
    append(db, "CiTEFUA =  0x%08x\n", ptr[2]);

    return EOK;
}

int mcp25xxfd_debug_dump_controller_txq_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db) {
    mcp25xxfd_io_read_packet_t pkt;

    int rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiTXQCON, 3*4, &pkt);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: Unable to read controller TXQ registers. Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        return rc;
    }
    uint32_t *ptr = (uint32_t*)(pkt.data);

    append(db, "Controller TXQ Registers\n");
    append(db, "========================\n");
    append(db, "CiTXQCON = 0x%08x\n", ptr[0]);
    append(db, "CiTXQSTA = 0x%08x\n", ptr[1]);
    append(db, "CiTXQUA =  0x%08x\n", ptr[2]);

    return EOK;
}

int mcp25xxfd_debug_dump_controller_fifo_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db) {
    mcp25xxfd_io_read_packet_t pkt;
    int rc;

    append(db, "Controller FIFO Registers\n");
    append(db, "=========================\n");

    for (int i = 1; i <= 31; ++i) {
        rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiFIFOCONm(i), 3*4, &pkt);
        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to read controller FIFO%d registers. Error=%s", ifc->dev.ifp->if_xname, i, strerror(rc));
            return rc;
        }
        uint32_t *ptr = (uint32_t*)(pkt.data);
        append(db, "CiFIFOCON%d=0x%08x  CiFIFOSTA%d=0x%08x  CiFIFOUA%d=0x%08x\n",
                i, ptr[0], i, ptr[1], i, ptr[2]);
    }

    return EOK;
}

int mcp25xxfd_debug_dump_controller_filter_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db) {
    mcp25xxfd_io_read_packet_t pkt;
    int rc;

    append(db, "Controller Filter Registers\n");
    append(db, "===========================\n");

    for (int i = 0; i <= 31; ++i) {
        rc = mcp25xxfd_io_read(ifc, MCP25xxFD_CiFLTCONm(i), 3*4, &pkt);
        if (rc != EOK) {
            CAN_LOG_ERR("[%s]: Unable to read controller Filter%d registers. Error=%s", ifc->dev.ifp->if_xname, i, strerror(rc));
            return rc;
        }
        uint32_t *ptr = (uint32_t*)(pkt.data);
        append(db, "CiFLTCON%d=0x%08x  CiFLTOBJ%d=0x%08x  CiMASK%d=0x%08x\n",
                i, ptr[0], i, ptr[1], i, ptr[2]);
    }

    return EOK;
}
