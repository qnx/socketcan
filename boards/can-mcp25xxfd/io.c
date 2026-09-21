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
#include <stddef.h>
#include <string.h>

#include "state.h"
#include "mcp25xxfd.h"
#include "io.h"

#include <sockcan_debug.h>

// Additional structs used internally
struct mcp25xxfd_io_reset_packet {
    spi_xchng_t spiHdr;
    uint16_t cmdAddr;
} __attribute__((packed));
typedef struct mcp25xxfd_io_reset_packet mcp25xxfd_io_reset_packet_t;

int mcp25xxfd_io_reset(mcp25xxfd_dev_t* ifc) {
    int rc;
    mcp25xxfd_io_reset_packet_t pkt;

    assert(ifc && "ifc is NULL");

    pkt.spiHdr.nbytes = 2;
    pkt.cmdAddr = MCP25xxFD_MAKE_CMD(MCP25xxFD_SPI_CMD_RESET, 0);

    CAN_LOG_NTC("[%s]: Reset()", ifc->dev.ifp->if_xname);
    rc = devctl(ifc->fd, DCMD_SPI_DATA_XCHNG, &pkt.spiHdr, sizeof(pkt), NULL);
    if (rc) {
        CAN_LOG_ERR("[%s]: Reset() FAILED! Error=%s", ifc->dev.ifp->if_xname, strerror(rc));
        return rc;
    }

    return EOK;
}

int mcp25xxfd_io_read(mcp25xxfd_dev_t* ifc, int addr, int numBytes, mcp25xxfd_io_read_packet_t* pkt) {
    int rc;

    assert(ifc && "ifc is NULL");
    assert(pkt && "pkt is NULL");
    assert(((addr & ~0xFFF) == 0) && "Address is out of range");
    assert(numBytes >= 0 && numBytes < MCP25xxFD_MAX_IO_PAYLOAD && "numBytes out of range");

    pkt->spiHdr.nbytes = 2 + numBytes;
    pkt->cmdAddr = MCP25xxFD_MAKE_CMD(MCP25xxFD_SPI_CMD_READ, addr);

    CAN_LOG_DEBUG2("[%s]: Read(addr=0x%02x, numBytes=%d)", ifc->dev.ifp->if_xname, addr, numBytes);
    rc = devctl(ifc->fd, DCMD_SPI_DATA_XCHNG, &pkt->spiHdr, sizeof(pkt->spiHdr) + pkt->spiHdr.nbytes, NULL);
    if (rc) {
        CAN_LOG_ERR("[%s]: Read(addr=0x%02x, numBytes=%d) FAILED! Error=%s",
            ifc->dev.ifp->if_xname, addr, numBytes, strerror(rc));
        return rc;
    }

    return EOK;
}

int mcp25xxfd_io_write(mcp25xxfd_dev_t* ifc, int addr, int numBytes, mcp25xxfd_io_write_packet_t* pkt) {
    int rc;

    assert(ifc && "ifc is NULL");
    assert(pkt && "pkt is NULL");
    assert(((addr & ~0xFFF) == 0) && "Address is out of range");
    assert(numBytes >= 0 && numBytes < MCP25xxFD_MAX_IO_PAYLOAD && "numBytes out of range");

    pkt->spiHdr.nbytes = 2 + numBytes;
    pkt->cmdAddr = MCP25xxFD_MAKE_CMD(MCP25xxFD_SPI_CMD_WRITE, addr);

    CAN_LOG_DEBUG2("[%s]: Write(addr=0x%02x, numBytes=%d)", ifc->dev.ifp->if_xname, addr, numBytes);
    rc = devctl(ifc->fd, DCMD_SPI_DATA_XCHNG, &pkt->spiHdr, sizeof(pkt->spiHdr) + pkt->spiHdr.nbytes, NULL);
    if (rc) {
        CAN_LOG_ERR("[%s]: Write(addr=0x%02x, numBytes=%d) FAILED! Error=%s",
             ifc->dev.ifp->if_xname, addr, numBytes, strerror(rc));
        return rc;
    }

    return EOK;
}

int mcp25xxfd_io_write_byte(mcp25xxfd_dev_t* ifc, int addr, uint8_t byte) {
    mcp25xxfd_io_write_packet_t pkt;
    pkt.data[0] = byte;
    CAN_LOG_DEBUG2("[%s]: WriteByte(addr=0x%02x, byte=0x%02x)", ifc->dev.ifp->if_xname, addr, byte);
    return mcp25xxfd_io_write(ifc, addr, 1, &pkt);
}

int mcp25xxfd_io_read_byte(mcp25xxfd_dev_t* ifc, int addr) {
    mcp25xxfd_io_read_packet_t pkt;
    int rc = mcp25xxfd_io_read(ifc, addr, 1, &pkt);
    if (!rc) {
        CAN_LOG_DEBUG2("[%s]: ReadByte(addr=0x%02x) -> 0x%02x", ifc->dev.ifp->if_xname, addr, pkt.data[0]);
        return pkt.data[0];
    } else {
        CAN_LOG_ERR("[%s]: ReadByte(addr=0x%02x) -> FAILED", ifc->dev.ifp->if_xname, addr);
        return -rc;
    }
}

int mcp25xxfd_io_write_word(mcp25xxfd_dev_t* ifc, int addr, uint32_t value) {
    mcp25xxfd_io_write_packet_t pkt;
    *(uint32_t*)pkt.data = value;
    CAN_LOG_DEBUG2("[%s]: WriteWord(addr=0x%02x, word=0x%08x)", ifc->dev.ifp->if_xname, addr, value);
    return mcp25xxfd_io_write(ifc, addr, sizeof(value), &pkt);
}

uint32_t mcp25xxfd_io_read_word(mcp25xxfd_dev_t* ifc, int addr) {
    mcp25xxfd_io_read_packet_t pkt;
    int rc = mcp25xxfd_io_read(ifc, addr, sizeof(uint32_t), &pkt);
    if (!rc) {
        CAN_LOG_DEBUG2("[%s]: ReadWord(addr=0x%02x) -> 0x%08x", ifc->dev.ifp->if_xname, addr, *(uint32_t*)pkt.data);
        errno = EOK;
        return *(uint32_t*)pkt.data;
    } else {
        errno = rc;
        CAN_LOG_ERR("[%s]: ReadWord(addr=0x%02x) -> FAILED", ifc->dev.ifp->if_xname, addr);
        return 0;
    }
}

uint32_t mcp25xxfd_io_bitset_word(mcp25xxfd_dev_t *ifc, int addr, uint32_t set, uint32_t clear) {
    union {
        mcp25xxfd_io_read_packet_t r;
        mcp25xxfd_io_write_packet_t w;
    } pkt;
    int rc;
    uint32_t value;

    CAN_LOG_DEBUG2("[%s]: BitSet(addr=0x%02x, set=0x%08x, clear=0x%08x)", ifc->dev.ifp->if_xname, addr, set, clear);

    rc = mcp25xxfd_io_read(ifc, addr, sizeof(uint32_t), &pkt.r);
    if (rc != EOK) {
        CAN_LOG_ERR("[%s]: BitSet(addr=0x%02x) -> FAILED READING", ifc->dev.ifp->if_xname, addr);
        return rc;
    }

    value = *(uint32_t*)pkt.r.data;
    value &= ~clear;
    value |= set;

    *(uint32_t*)pkt.w.data = value;
    return mcp25xxfd_io_write(ifc, addr, sizeof(uint32_t), &pkt.w);
}
