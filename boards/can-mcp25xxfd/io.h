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

#ifndef MCP25xxFD_IO_H_
#define MCP25xxFD_IO_H_

/**
 * @file
 *
 * APIs exposing the underlying SPI protocol the MCP25xxFD uses.
 */

#include <stdint.h>

#include <hw/io-spi.h>

#include "state.h"

#define MCP25xxFD_MAX_IO_PAYLOAD 128

/**
 * Structure defining the data sent for the MCP25xxFD's Read instruction.
 *
 * The members of this structure are filled in by the mcp25xxfd_io_read() API.
 */
struct mcp25xxfd_io_read_packet {
    /** The common SPI header required by the QNX SPI devctl interface. */
    spi_xchng_t spiHdr;
    /** The combined command/address */
    uint16_t cmdAddr;
    /** The buffer to place the bytes read from the MCP25xxFD */
    uint8_t data[MCP25xxFD_MAX_IO_PAYLOAD];
} __attribute__((packed));
typedef struct mcp25xxfd_io_read_packet mcp25xxfd_io_read_packet_t;

/**
 * Structure defining the data sent for the MCP25xxFD's Write instruction.
 *
 * The caller is responsible for filling in the 'data' portion of the struct.
 * All other members of this structure are filled in by the mcp25xxfd_io_write() API.
 */
struct mcp25xxfd_io_write_packet {
    /** The common SPI header required by the QNX SPI devctl interface. */
    spi_xchng_t spiHdr;
    /** The combined command/address */
    uint16_t cmdAddr;
    /** The buffer holding data to write to the MCP25xxFD */
    uint8_t data[MCP25xxFD_MAX_IO_PAYLOAD];
} __attribute__((packed));
typedef struct mcp25xxfd_io_write_packet mcp25xxfd_io_write_packet_t;

/**
 * Sends a Reset instruction to the MCP25xxFD.
 *
 * @note This API only sends the instruction. It does not wait for the RESET
 *       to complete.
 *
 * @param[in]   ifc   The interface context for the MCP25xxFD
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp25xxfd_io_reset(mcp25xxfd_dev_t* ifc);

/**
 * Sends a Read instruction to the MCP25xxFD
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP25xxFD
 *       actually responded to the instruction or not.
 *
 * @param[in]   ifc     The interface context for the MCP25xxFD
 * @param[in]   addr    The address of the register to start reading from
 * @param[in]   numBytes The number of bytes to read. Bytes are read from
 *                       sequential registers starting at the one specified
 *                       by @ref addr.
 * @param[out]  pkt     Caller allocated struct to use for the SPI transaction.
 *                      The bytes read from the MCP25xxFD will be in the pkt's
 *                      data member on return.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp25xxfd_io_read(mcp25xxfd_dev_t* ifc, int addr, int numBytes, mcp25xxfd_io_read_packet_t* pkt);

/**
 * Sends a Write instruction to the MCP25xxFD
 *
 * @param[in]   ifc     The interface context for the MCP25xxFD
 * @param[in]   addr    The address of the register to start writing to
 * @param[in]   numBytes The number of bytes to write. Bytes are written from
 *                       sequential registers starting at the one specified
 *                       by @ref addr.
 * @param[out]  pkt     Caller allocated struct to use for the SPI transaction.
 *                      The bytes written to the MCP25xxFD MUST be in the pkt's
 *                      data member when this API is called.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp25xxfd_io_write(mcp25xxfd_dev_t* ifc, int addr, int numBytes, mcp25xxfd_io_write_packet_t* pkt);

/**
 * A convenience function to write a single byte to the MCP25xxFD.
 *
 * Usually used when updating a single register.
 *
 * @param[in]   ifc     The interface context for the MCP25xxFD
 * @param[in]   addr    The address of the register to write to
 * @param[in]   byte    The data to write to the register.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp25xxfd_io_write_byte(mcp25xxfd_dev_t* ifc, int addr, uint8_t byte);

/**
 * A convenience function to read a single byte from the MCP25xxFD.
 *
 * Usually used when trying to read a single register.
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP25xxFD
 *       actually responded to the instruction or not.
 *
 * @param[in]   ifc     The interface context for the MCP25xxFD
 * @param[in]   addr    The address of the register to read from
 *
 * @return The (unsigned) byte of data read from the register.
 *         If there is a failure during communication, the negative of an
 *         errno is returned instead. Thus a value >=0 indicates success
 *         while a negative value indicates failure.
 */
int mcp25xxfd_io_read_byte(mcp25xxfd_dev_t* ifc, int addr);

/**
 * A convenience function to write a single word (4bytes) to the MCP25xxFD.
 *
 * Usually used when updating a single register.
 *
 * @param[in]   ifc     The interface context for the MCP25xxFD
 * @param[in]   addr    The address of the register to write to
 * @param[in]   value   The data to write to the register.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp25xxfd_io_write_word(mcp25xxfd_dev_t* ifc, int addr, uint32_t value);

/**
 * A convenience function to read a single word (4bytes) from the MCP25xxFD.
 *
 * Usually used when trying to read a single register.
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP25xxFD
 *       actually responded to the instruction or not.
 *
 * @param[in]   ifc     The interface context for the MCP25xxFD
 * @param[in]   addr    The address of the register to read from
 *
 * @return On success, the (unsigned) byte of data read from the register and
 *         errno is set to EOK.
 *         If there is a failure during communication, then 0 is returned
 *         and errno is set to a non-EOK value.
 *         Thus to check for success/failure just check errno.
 */
uint32_t mcp25xxfd_io_read_word(mcp25xxfd_dev_t* ifc, int addr);

/**
 * A convenience function to bit modify a single word (4bytes).
 *
 * This does a read/modify/write cycle for the specified address.
 *
 * @NOTE The clear bits are modified before the set bits are applied.
 *       So if the same bit is set in both set and clear, that bit
 *       will be set in the register.
 *
 * @param[in]   ifc     The interface context for the MCP25xxFD
 * @param[in]   addr    The address of the register to write to
 * @param[in]   set     The bits to set in the register
 * @param[in]   clear   The bits to clear in the register
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
uint32_t mcp25xxfd_io_bitset_word(mcp25xxfd_dev_t *ifc, int addr, uint32_t set, uint32_t clear);

#endif  // MCP25xxFD_IO_H_
