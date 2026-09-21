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

#ifndef MCP25xxFD_H_
#define MCP25xxFD_H_

/**
 * @file
 *
 * Hardware level information about the MCP25xxFD family.
 *
 * See the MCP25xxFD datasheet(s) for more details about the various registers
 * and the bitfields within them.
 */

#include <stdint.h>

/**
 * Helper that can be used to easily get a bitfield from the value read
 * from a MCP25xxFD register.
 *
 * @param[in]   value   The value read from the register
 * @param[in]   bf      The NAME of the bitfield to extract. This is used
 *                      to construct references to MCP25xxFD_<bf>_MASK and
 *                      MCP25xxFD_<bf>_SHIFT constants.
 *
 * @return The desired bitfield. Note that the return value is shifted so that
 *         it starts at bit 0. Regardless of where it actually is in the register.
 */
#define MCP25xxFD_GET_BITFIELD(value, bf) \
    (((value) & (MCP25xxFD_##bf##_MASK)) >> (MCP25xxFD_##bf##_SHIFT))

/**
 * Helper that can be used to create a bitfield.
 * All non-bitfield bits are set to 0.
 * Useful when you want to OR a bunch of bitfields together into a single
 * value.
 *
 * @param[in]   value   The value for the bitfield. Note that it
 *                      should start at bit 0. This API is responsible
 *                      for shifting it to the correct location.
 * @param[in]   bf      The NAME of the bitfield to create. This is used
 *                      to construct references to MCP25xxFD_<bf>_MASK and
 *                      MCP25xxFD_<bf>_SHIFT constants.
 */
#define MCP25xxFD_BITFIELD(value, bf) (((value) << (MCP25xxFD_##bf##_SHIFT)) & MCP25xxFD_##bf##_MASK)

/**
 * Helper that can be used to easily set a bitfield in a value that will be
 * used to write to a MCP25xxFD register. The specified value is modified in
 * place.
 *
 * @param[in]   value   The current value for the register. This will be
 *                      modified in place to have the desired value in the
 *                      bitfield.
 * @param[in]   newFieldValue The new value for the bitfield. Note that it
 *                            should start at bit 0. This API is responsible
 *                            for shifting it to the correct location.
 * @param[in]   bf      The NAME of the bitfield to set. This is used
 *                      to construct references to MCP25xxFD_<bf>_MASK and
 *                      MCP25xxFD_<bf>_SHIFT constants.
 */
#define MCP25xxFD_SET_BITFIELD(value, newFieldValue, bf) \
    (value) = (((value) & ~(MCP25xxFD_##bf##_MASK)) | MCP25xxFD_BITFIELD(newFieldValue, bf))

/** @name RegAddresses
 * Addresses for the various registers exposed by the MCP25xxFD.
 * See the datasheet for more details.
 *
 * All #defines have the form MCP25xxFD_<regName> where <regName> is
 * the name given in the MCP25xxFD datasheet.
 *
 * @note When there are multiple registers for something an 'm' version of
 *       #define is provided to get the address for the mth register. No
 *       bounds checking is done on the value of m.
 *
 *       For example. The address for FIFO 4's CiFIFOCONm register can be specified
 *       via MCP25xxFD_CiFIFOCONm(4).
 *       In addition specific #defines for each valid value of m will be present.
 *       So in addition to the above one could use MCP25xxFD_CiFIFOCON4
 */
/**@{*/
#define MCP25xxFD_OSC           0xE00
#define MCP25xxFD_IOCON         0xE04
#define MCP25xxFD_CRC           0xE08
#define MCP25xxFD_ECCCON        0xE0C
#define MCP25xxFD_ECCSTAT       0xE10
#define MCP25xxFD_DEVID         0xE14

#define MCP25xxFD_CiCON         0x000
#define MCP25xxFD_CiNBTCFG      0x004
#define MCP25xxFD_CiDBTCFG      0x008
#define MCP25xxFD_CiTDC         0x00C
#define MCP25xxFD_CiTBC         0x010
#define MCP25xxFD_CiTSCON       0x014
#define MCP25xxFD_CiVEC         0x018
#define MCP25xxFD_CiINT         0x01C
#define MCP25xxFD_CiRXIF        0x020
#define MCP25xxFD_CiTXIF        0x024
#define MCP25xxFD_CiRXOVIF      0x028
#define MCP25xxFD_CiTXATIF      0x02C
#define MCP25xxFD_CiTXREQ       0x030
#define MCP25xxFD_CiTREC        0x034
#define MCP25xxFD_CiBDIAG0      0x038
#define MCP25xxFD_CiBDIAG1      0x03C
#define MCP25xxFD_CiTEFCON      0x040
#define MCP25xxFD_CiTEFSTA      0x044
#define MCP25xxFD_CiTEFUA       0x048
// Reserved                     0x04C
#define MCP25xxFD_CiTXQCON      0x050
#define MCP25xxFD_CiTXQSTA      0x054
#define MCP25xxFD_CiTXQUA       0x058

#define MCP25xxFD_CiFIFOCONm(m) (0x05C + ((m)-1)*12)
#define MCP25xxFD_CiFIFOCON1    MCP25xxFD_CiFIFOCONm(1)
#define MCP25xxFD_CiFIFOCON2    MCP25xxFD_CiFIFOCONm(2)
#define MCP25xxFD_CiFIFOCON3    MCP25xxFD_CiFIFOCONm(3)
#define MCP25xxFD_CiFIFOCON4    MCP25xxFD_CiFIFOCONm(4)
#define MCP25xxFD_CiFIFOCON5    MCP25xxFD_CiFIFOCONm(5)
#define MCP25xxFD_CiFIFOCON6    MCP25xxFD_CiFIFOCONm(6)
#define MCP25xxFD_CiFIFOCON7    MCP25xxFD_CiFIFOCONm(7)
#define MCP25xxFD_CiFIFOCON8    MCP25xxFD_CiFIFOCONm(8)
#define MCP25xxFD_CiFIFOCON9    MCP25xxFD_CiFIFOCONm(9)
#define MCP25xxFD_CiFIFOCON10   MCP25xxFD_CiFIFOCONm(10)
#define MCP25xxFD_CiFIFOCON11   MCP25xxFD_CiFIFOCONm(11)
#define MCP25xxFD_CiFIFOCON12   MCP25xxFD_CiFIFOCONm(12)
#define MCP25xxFD_CiFIFOCON13   MCP25xxFD_CiFIFOCONm(13)
#define MCP25xxFD_CiFIFOCON14   MCP25xxFD_CiFIFOCONm(14)
#define MCP25xxFD_CiFIFOCON15   MCP25xxFD_CiFIFOCONm(15)
#define MCP25xxFD_CiFIFOCON16   MCP25xxFD_CiFIFOCONm(16)
#define MCP25xxFD_CiFIFOCON17   MCP25xxFD_CiFIFOCONm(17)
#define MCP25xxFD_CiFIFOCON18   MCP25xxFD_CiFIFOCONm(18)
#define MCP25xxFD_CiFIFOCON19   MCP25xxFD_CiFIFOCONm(19)
#define MCP25xxFD_CiFIFOCON20   MCP25xxFD_CiFIFOCONm(20)
#define MCP25xxFD_CiFIFOCON21   MCP25xxFD_CiFIFOCONm(21)
#define MCP25xxFD_CiFIFOCON22   MCP25xxFD_CiFIFOCONm(22)
#define MCP25xxFD_CiFIFOCON23   MCP25xxFD_CiFIFOCONm(23)
#define MCP25xxFD_CiFIFOCON24   MCP25xxFD_CiFIFOCONm(24)
#define MCP25xxFD_CiFIFOCON25   MCP25xxFD_CiFIFOCONm(25)
#define MCP25xxFD_CiFIFOCON26   MCP25xxFD_CiFIFOCONm(26)
#define MCP25xxFD_CiFIFOCON27   MCP25xxFD_CiFIFOCONm(27)
#define MCP25xxFD_CiFIFOCON28   MCP25xxFD_CiFIFOCONm(28)
#define MCP25xxFD_CiFIFOCON29   MCP25xxFD_CiFIFOCONm(29)
#define MCP25xxFD_CiFIFOCON30   MCP25xxFD_CiFIFOCONm(30)
#define MCP25xxFD_CiFIFOCON31   MCP25xxFD_CiFIFOCONm(31)

#define MCP25xxFD_CiFIFOSTAm(m) (0x060 + ((m)-1)*12)
#define MCP25xxFD_CiFIFOSTA1    MCP25xxFD_CiFIFOSTAm(1)
#define MCP25xxFD_CiFIFOSTA2    MCP25xxFD_CiFIFOSTAm(2)
#define MCP25xxFD_CiFIFOSTA3    MCP25xxFD_CiFIFOSTAm(3)
#define MCP25xxFD_CiFIFOSTA4    MCP25xxFD_CiFIFOSTAm(4)
#define MCP25xxFD_CiFIFOSTA5    MCP25xxFD_CiFIFOSTAm(5)
#define MCP25xxFD_CiFIFOSTA6    MCP25xxFD_CiFIFOSTAm(6)
#define MCP25xxFD_CiFIFOSTA7    MCP25xxFD_CiFIFOSTAm(7)
#define MCP25xxFD_CiFIFOSTA8    MCP25xxFD_CiFIFOSTAm(8)
#define MCP25xxFD_CiFIFOSTA9    MCP25xxFD_CiFIFOSTAm(9)
#define MCP25xxFD_CiFIFOSTA10   MCP25xxFD_CiFIFOSTAm(10)
#define MCP25xxFD_CiFIFOSTA11   MCP25xxFD_CiFIFOSTAm(11)
#define MCP25xxFD_CiFIFOSTA12   MCP25xxFD_CiFIFOSTAm(12)
#define MCP25xxFD_CiFIFOSTA13   MCP25xxFD_CiFIFOSTAm(13)
#define MCP25xxFD_CiFIFOSTA14   MCP25xxFD_CiFIFOSTAm(14)
#define MCP25xxFD_CiFIFOSTA15   MCP25xxFD_CiFIFOSTAm(15)
#define MCP25xxFD_CiFIFOSTA16   MCP25xxFD_CiFIFOSTAm(16)
#define MCP25xxFD_CiFIFOSTA17   MCP25xxFD_CiFIFOSTAm(17)
#define MCP25xxFD_CiFIFOSTA18   MCP25xxFD_CiFIFOSTAm(18)
#define MCP25xxFD_CiFIFOSTA19   MCP25xxFD_CiFIFOSTAm(19)
#define MCP25xxFD_CiFIFOSTA20   MCP25xxFD_CiFIFOSTAm(20)
#define MCP25xxFD_CiFIFOSTA21   MCP25xxFD_CiFIFOSTAm(21)
#define MCP25xxFD_CiFIFOSTA22   MCP25xxFD_CiFIFOSTAm(22)
#define MCP25xxFD_CiFIFOSTA23   MCP25xxFD_CiFIFOSTAm(23)
#define MCP25xxFD_CiFIFOSTA24   MCP25xxFD_CiFIFOSTAm(24)
#define MCP25xxFD_CiFIFOSTA25   MCP25xxFD_CiFIFOSTAm(25)
#define MCP25xxFD_CiFIFOSTA26   MCP25xxFD_CiFIFOSTAm(26)
#define MCP25xxFD_CiFIFOSTA27   MCP25xxFD_CiFIFOSTAm(27)
#define MCP25xxFD_CiFIFOSTA28   MCP25xxFD_CiFIFOSTAm(28)
#define MCP25xxFD_CiFIFOSTA29   MCP25xxFD_CiFIFOSTAm(29)
#define MCP25xxFD_CiFIFOSTA30   MCP25xxFD_CiFIFOSTAm(30)
#define MCP25xxFD_CiFIFOSTA31   MCP25xxFD_CiFIFOSTAm(31)

#define MCP25xxFD_CiFIFOUAm(m)  (0x064 + ((m)-1)*12)
#define MCP25xxFD_CiFIFOUA1     MCP25xxFD_CiFIFOUAm(1)
#define MCP25xxFD_CiFIFOUA2     MCP25xxFD_CiFIFOUAm(2)
#define MCP25xxFD_CiFIFOUA3     MCP25xxFD_CiFIFOUAm(3)
#define MCP25xxFD_CiFIFOUA4     MCP25xxFD_CiFIFOUAm(4)
#define MCP25xxFD_CiFIFOUA5     MCP25xxFD_CiFIFOUAm(5)
#define MCP25xxFD_CiFIFOUA6     MCP25xxFD_CiFIFOUAm(6)
#define MCP25xxFD_CiFIFOUA7     MCP25xxFD_CiFIFOUAm(7)
#define MCP25xxFD_CiFIFOUA8     MCP25xxFD_CiFIFOUAm(8)
#define MCP25xxFD_CiFIFOUA9     MCP25xxFD_CiFIFOUAm(9)
#define MCP25xxFD_CiFIFOUA10    MCP25xxFD_CiFIFOUAm(10)
#define MCP25xxFD_CiFIFOUA11    MCP25xxFD_CiFIFOUAm(11)
#define MCP25xxFD_CiFIFOUA12    MCP25xxFD_CiFIFOUAm(12)
#define MCP25xxFD_CiFIFOUA13    MCP25xxFD_CiFIFOUAm(13)
#define MCP25xxFD_CiFIFOUA14    MCP25xxFD_CiFIFOUAm(14)
#define MCP25xxFD_CiFIFOUA15    MCP25xxFD_CiFIFOUAm(15)
#define MCP25xxFD_CiFIFOUA16    MCP25xxFD_CiFIFOUAm(16)
#define MCP25xxFD_CiFIFOUA17    MCP25xxFD_CiFIFOUAm(17)
#define MCP25xxFD_CiFIFOUA18    MCP25xxFD_CiFIFOUAm(18)
#define MCP25xxFD_CiFIFOUA19    MCP25xxFD_CiFIFOUAm(19)
#define MCP25xxFD_CiFIFOUA20    MCP25xxFD_CiFIFOUAm(20)
#define MCP25xxFD_CiFIFOUA21    MCP25xxFD_CiFIFOUAm(21)
#define MCP25xxFD_CiFIFOUA22    MCP25xxFD_CiFIFOUAm(22)
#define MCP25xxFD_CiFIFOUA23    MCP25xxFD_CiFIFOUAm(23)
#define MCP25xxFD_CiFIFOUA24    MCP25xxFD_CiFIFOUAm(24)
#define MCP25xxFD_CiFIFOUA25    MCP25xxFD_CiFIFOUAm(25)
#define MCP25xxFD_CiFIFOUA26    MCP25xxFD_CiFIFOUAm(26)
#define MCP25xxFD_CiFIFOUA27    MCP25xxFD_CiFIFOUAm(27)
#define MCP25xxFD_CiFIFOUA28    MCP25xxFD_CiFIFOUAm(28)
#define MCP25xxFD_CiFIFOUA29    MCP25xxFD_CiFIFOUAm(29)
#define MCP25xxFD_CiFIFOUA30    MCP25xxFD_CiFIFOUAm(30)
#define MCP25xxFD_CiFIFOUA31    MCP25xxFD_CiFIFOUAm(31)

#define MCP25xxFD_CiFLTCONm(m)  (0x1D0 + (((m)/4)*4))
#define MCP25xxFD_CiFLTCON0     MCP25xxFD_CiFLTCONm(0)
#define MCP25xxFD_CiFLTCON1     MCP25xxFD_CiFLTCONm(1)
#define MCP25xxFD_CiFLTCON2     MCP25xxFD_CiFLTCONm(2)
#define MCP25xxFD_CiFLTCON3     MCP25xxFD_CiFLTCONm(3)
#define MCP25xxFD_CiFLTCON4     MCP25xxFD_CiFLTCONm(4)
#define MCP25xxFD_CiFLTCON5     MCP25xxFD_CiFLTCONm(5)
#define MCP25xxFD_CiFLTCON6     MCP25xxFD_CiFLTCONm(6)
#define MCP25xxFD_CiFLTCON7     MCP25xxFD_CiFLTCONm(7)

#define MCP25xxFD_CiFLTOBJm     (0x1F0 + ((m) * 8))
#define MCP25xxFD_CiFLTOBJ0     MCP25xxFD_CiFLTOBJm(0)
#define MCP25xxFD_CiFLTOBJ1     MCP25xxFD_CiFLTOBJm(1)
#define MCP25xxFD_CiFLTOBJ2     MCP25xxFD_CiFLTOBJm(2)
#define MCP25xxFD_CiFLTOBJ3     MCP25xxFD_CiFLTOBJm(3)
#define MCP25xxFD_CiFLTOBJ4     MCP25xxFD_CiFLTOBJm(4)
#define MCP25xxFD_CiFLTOBJ5     MCP25xxFD_CiFLTOBJm(5)
#define MCP25xxFD_CiFLTOBJ6     MCP25xxFD_CiFLTOBJm(6)
#define MCP25xxFD_CiFLTOBJ7     MCP25xxFD_CiFLTOBJm(7)
#define MCP25xxFD_CiFLTOBJ8     MCP25xxFD_CiFLTOBJm(8)
#define MCP25xxFD_CiFLTOBJ9     MCP25xxFD_CiFLTOBJm(9)
#define MCP25xxFD_CiFLTOBJ10    MCP25xxFD_CiFLTOBJm(10)
#define MCP25xxFD_CiFLTOBJ11    MCP25xxFD_CiFLTOBJm(11)
#define MCP25xxFD_CiFLTOBJ12    MCP25xxFD_CiFLTOBJm(12)
#define MCP25xxFD_CiFLTOBJ13    MCP25xxFD_CiFLTOBJm(13)
#define MCP25xxFD_CiFLTOBJ14    MCP25xxFD_CiFLTOBJm(14)
#define MCP25xxFD_CiFLTOBJ15    MCP25xxFD_CiFLTOBJm(15)
#define MCP25xxFD_CiFLTOBJ16    MCP25xxFD_CiFLTOBJm(16)
#define MCP25xxFD_CiFLTOBJ17    MCP25xxFD_CiFLTOBJm(17)
#define MCP25xxFD_CiFLTOBJ18    MCP25xxFD_CiFLTOBJm(18)
#define MCP25xxFD_CiFLTOBJ19    MCP25xxFD_CiFLTOBJm(19)
#define MCP25xxFD_CiFLTOBJ20    MCP25xxFD_CiFLTOBJm(20)
#define MCP25xxFD_CiFLTOBJ21    MCP25xxFD_CiFLTOBJm(21)
#define MCP25xxFD_CiFLTOBJ22    MCP25xxFD_CiFLTOBJm(22)
#define MCP25xxFD_CiFLTOBJ23    MCP25xxFD_CiFLTOBJm(23)
#define MCP25xxFD_CiFLTOBJ24    MCP25xxFD_CiFLTOBJm(24)
#define MCP25xxFD_CiFLTOBJ25    MCP25xxFD_CiFLTOBJm(25)
#define MCP25xxFD_CiFLTOBJ26    MCP25xxFD_CiFLTOBJm(26)
#define MCP25xxFD_CiFLTOBJ27    MCP25xxFD_CiFLTOBJm(27)
#define MCP25xxFD_CiFLTOBJ28    MCP25xxFD_CiFLTOBJm(28)
#define MCP25xxFD_CiFLTOBJ29    MCP25xxFD_CiFLTOBJm(29)
#define MCP25xxFD_CiFLTOBJ30    MCP25xxFD_CiFLTOBJm(30)
#define MCP25xxFD_CiFLTOBJ31    MCP25xxFD_CiFLTOBJm(31)

#define MCP25xxFD_CiMASKm(m)    (0x1F4 + ((m) * 8))
#define MCP25xxFD_CiMASK0       MCP25xxFD_CiMASKm(0)
#define MCP25xxFD_CiMASK1       MCP25xxFD_CiMASKm(1)
#define MCP25xxFD_CiMASK2       MCP25xxFD_CiMASKm(2)
#define MCP25xxFD_CiMASK3       MCP25xxFD_CiMASKm(3)
#define MCP25xxFD_CiMASK4       MCP25xxFD_CiMASKm(4)
#define MCP25xxFD_CiMASK5       MCP25xxFD_CiMASKm(5)
#define MCP25xxFD_CiMASK6       MCP25xxFD_CiMASKm(6)
#define MCP25xxFD_CiMASK7       MCP25xxFD_CiMASKm(7)
#define MCP25xxFD_CiMASK8       MCP25xxFD_CiMASKm(8)
#define MCP25xxFD_CiMASK9       MCP25xxFD_CiMASKm(9)
#define MCP25xxFD_CiMASK10      MCP25xxFD_CiMASKm(10)
#define MCP25xxFD_CiMASK11      MCP25xxFD_CiMASKm(11)
#define MCP25xxFD_CiMASK12      MCP25xxFD_CiMASKm(12)
#define MCP25xxFD_CiMASK13      MCP25xxFD_CiMASKm(13)
#define MCP25xxFD_CiMASK14      MCP25xxFD_CiMASKm(14)
#define MCP25xxFD_CiMASK15      MCP25xxFD_CiMASKm(15)
#define MCP25xxFD_CiMASK16      MCP25xxFD_CiMASKm(16)
#define MCP25xxFD_CiMASK17      MCP25xxFD_CiMASKm(17)
#define MCP25xxFD_CiMASK18      MCP25xxFD_CiMASKm(18)
#define MCP25xxFD_CiMASK19      MCP25xxFD_CiMASKm(19)
#define MCP25xxFD_CiMASK20      MCP25xxFD_CiMASKm(20)
#define MCP25xxFD_CiMASK21      MCP25xxFD_CiMASKm(21)
#define MCP25xxFD_CiMASK22      MCP25xxFD_CiMASKm(22)
#define MCP25xxFD_CiMASK23      MCP25xxFD_CiMASKm(23)
#define MCP25xxFD_CiMASK24      MCP25xxFD_CiMASKm(24)
#define MCP25xxFD_CiMASK25      MCP25xxFD_CiMASKm(25)
#define MCP25xxFD_CiMASK26      MCP25xxFD_CiMASKm(26)
#define MCP25xxFD_CiMASK27      MCP25xxFD_CiMASKm(27)
#define MCP25xxFD_CiMASK28      MCP25xxFD_CiMASKm(28)
#define MCP25xxFD_CiMASK29      MCP25xxFD_CiMASKm(29)
#define MCP25xxFD_CiMASK30      MCP25xxFD_CiMASKm(30)
#define MCP25xxFD_CiMASK31      MCP25xxFD_CiMASKm(31)
/**@}*/


/** @name RegFields
 * Masks and offsets for the various bitfields contained with the MCP25xxFD's
 * registers. See the datasheet for more details.
 *
 * For each field <fieldName> in register <regName> two #defines are provided:
 * - MCP25xxFD_<regName>_<fieldName>_MASK masks out the bitfield in the register
 * - MCP25xxFD_<regName>_<fieldName>_SHIFT the starting bit of the bitfield in
 *                                          the register
 *
 * @note When there are multiple registers all with the same bitfields, the
 *       'm' version of the <regName> is used.
 *
 *       For example. The <regName> for the fields of a FIFO's CiFIFOCONm register
 *       is MCP25xxFD_CiFIFOCONm_<fieldName>
 */
/**@{*/
// OSC fields
#define MCP25xxFD_OSC_PLLEN_MASK        0x00000001 // R/W-0
#define MCP25xxFD_OSC_OSCDI_MASK        0x00000004 // HS/C-0
#define MCP25xxFD_OSC_LPME_MASK         0x00000008 // R/W-0
#define MCP25xxFD_OSC_SCLKDIV_MASK      0x00000010 // R/W-0
#define MCP25xxFD_OSC_CLKODIV_MASK      0x00000060 // R/W-0b11
#define MCP25xxFD_OSC_PLLRDY_MASK       0x00000100 // R-0
#define MCP25xxFD_OSC_OSCRDY_MASK       0x00000400 // R-0
#define MCP25xxFD_OSC_SCLKRDY_MASK      0x00001000 // R-0
#define MCP25xxFD_OSC_PLLEN_SHIFT       0
#define MCP25xxFD_OSC_OSCDI_SHIFT       2
#define MCP25xxFD_OSC_LPME_SHIFT        3
#define MCP25xxFD_OSC_SCLKDIV_SHIFT     4
#define MCP25xxFD_OSC_CLKODIV_SHIFT     5
#define MCP25xxFD_OSC_PLLRDY_SHIFT      8
#define MCP25xxFD_OSC_OSCRDY_SHIFT      10
#define MCP25xxFD_OSC_SCLKRDY_SHIFT     12

// IOCON fields
#define MCP25xxFD_IOCON_TRIS0_MASK      0x00000001 // R/W-1
#define MCP25xxFD_IOCON_TRIS1_MASK      0x00000002 // R/W-1
#define MCP25xxFD_IOCON_XSTBYEN_MASK    0x00000040 // R/W-0
#define MCP25xxFD_IOCON_LAT0_MASK       0x00000100 // R/W-x
#define MCP25xxFD_IOCON_LAT1_MASK       0x00000200 // R/W-x
#define MCP25xxFD_IOCON_GPIO0_MASK      0x00010000 // R/W-x
#define MCP25xxFD_IOCON_GPIO1_MASK      0x00020000 // R/W-x
#define MCP25xxFD_IOCON_PM0_MASK        0x01000000 // R/W-1
#define MCP25xxFD_IOCON_PM1_MASK        0x02000000 // R/W-1
#define MCP25xxFD_IOCON_TXCANOD_MASK    0x10000000 // R/W-0
#define MCP25xxFD_IOCON_SOF_MASK        0x20000000 // R/W-0
#define MCP25xxFD_IOCON_INTOD_MASK      0x40000000 // R/W-0
#define MCP25xxFD_IOCON_TRIS0_SHIFT     0
#define MCP25xxFD_IOCON_TRIS1_SHIFT     1
#define MCP25xxFD_IOCON_XSTBYEN_SHIFT   6
#define MCP25xxFD_IOCON_LAT0_SHIFT      8
#define MCP25xxFD_IOCON_LAT1_SHIFT      9
#define MCP25xxFD_IOCON_GPIO0_SHIFT     16
#define MCP25xxFD_IOCON_GPIO1_SHIFT     17
#define MCP25xxFD_IOCON_PM0_SHIFT       24
#define MCP25xxFD_IOCON_PM1_SHIFT       25
#define MCP25xxFD_IOCON_TXCANOD_SHIFT   28
#define MCP25xxFD_IOCON_SOF_SHIFT       29
#define MCP25xxFD_IOCON_INTOD_SHIFT     30

// CRC fields
#define MCP25xxFD_CRC_CRC_MASK          0x0000FFFF // R-0
#define MCP25xxFD_CRC_CRCERRIF_MASK     0x00010000 // HS/C-0
#define MCP25xxFD_CRC_FERRIF_MASK       0x00020000 // HS/C-0
#define MCP25xxFD_CRC_CRCERRIE_MASK     0x01000000 // R/W-0
#define MCP25xxFD_CRC_FERRIE_MASK       0x02000000 // R/W-0
#define MCP25xxFD_CRC_CRC_SHIFT         0
#define MCP25xxFD_CRC_CRCERRIF_SHIFT    16
#define MCP25xxFD_CRC_FERRIF_SHIFT      17
#define MCP25xxFD_CRC_CRCERRIE_SHIFT    24
#define MCP25xxFD_CRC_FERRIE_SHIFT      25

// ECCCON fields
#define MCP25xxFD_ECCCON_ECCEN_MASK     0x00000001 // R/W-0
#define MCP25xxFD_ECCCON_SECIE_MASK     0x00000001 // R/W-0
#define MCP25xxFD_ECCCON_DEDIE_MASK     0x00000004 // R/W-0
#define MCP25xxFD_ECCCON_PARITY_MASK    0x00007F00 // R/W-0
#define MCP25xxFD_ECCCON_ECCEN_SHIFT    0
#define MCP25xxFD_ECCCON_SECIE_SHIFT    1
#define MCP25xxFD_ECCCON_DEDIE_SHIFT    2
#define MCP25xxFD_ECCCON_PARITY_SHIFT   8

// ECCSTAT fields
#define MCP25xxFD_ECCSTAT_SECIF_MASK    0x00000002 // HS/C-0
#define MCP25xxFD_ECCSTAT_DEDIF_MASK    0x00000004 // HS/C-0
#define MCP25xxFD_ECCSTAT_ERRADDR_MASK  0x0FFF0000 // R-0
#define MCP25xxFD_ECCSTAT_SECIF_SHIFT   1
#define MCP25xxFD_ECCSTAT_DEDIF_SHIFT   2
#define MCP25xxFD_ECCSTAT_ERRADDR_SHIFT 16

// DEVID fields
#define MCP25xxFD_DEVID_REV_MASK        0x0000000F // R-0
#define MCP25xxFD_DEVID_ID_MASK         0x000000F0 // R-0
#define MCP25xxFD_DEVID_REV_SHIFT       0
#define MCP25xxFD_DEVID_ID_SHIFT        4

// CiCON fields
#define MCP25xxFD_CiCON_DNCNT_MASK      0x0000001F // R/W-0
#define MCP25xxFD_CiCON_ISOCRCEN_MASK   0x00000020 // R/W-1
#define MCP25xxFD_CiCON_PXEDIS_MASK     0x00000040 // R/W-1
#define MCP25xxFD_CiCON_WAKFIL_MASK     0x00000100 // R/W-0
#define MCP25xxFD_CiCON_WFT_MASK        0x00000600 // R/W-1
#define MCP25xxFD_CiCON_BUSY_MASK       0x00000800 // R-0
#define MCP25xxFD_CiCON_BRSDIS_MASK     0x00001000 // R/W-0
#define MCP25xxFD_CiCON_RTXAT_MASK      0x00010000 // R/W-0
#define MCP25xxFD_CiCON_ESIGM_MASK      0x00020000 // R/W-0
#define MCP25xxFD_CiCON_SERR2LOM_MASK   0x00040000 // R/W-0
#define MCP25xxFD_CiCON_STEF_MASK       0x00080000 // R/W-1
#define MCP25xxFD_CiCON_TXQEN_MASK      0x00100000 // R/W-1
#define MCP25xxFD_CiCON_OPMOD_MASK      0x00E00000 // R-0
#define MCP25xxFD_CiCON_REQOP_MASK      0x07000000 // R/W-0
#define MCP25xxFD_CiCON_ABAT_MASK       0x08000000 // R/W-0
#define MCP25xxFD_CiCON_TXBWS_MASK      0xF0000000 // R/W-0
#define MCP25xxFD_CiCON_DNCNT_SHIFT     0
#define MCP25xxFD_CiCON_ISOCRCEN_SHIFT  5
#define MCP25xxFD_CiCON_PXEDIS_SHIFT    6
#define MCP25xxFD_CiCON_WAKFIL_SHIFT    8
#define MCP25xxFD_CiCON_WFT_SHIFT       9
#define MCP25xxFD_CiCON_BUSY_SHIFT      11
#define MCP25xxFD_CiCON_BRSDIS_SHIFT    12
#define MCP25xxFD_CiCON_RTXAT_SHIFT     16
#define MCP25xxFD_CiCON_ESIGM_SHIFT     17
#define MCP25xxFD_CiCON_SERR2LOM_SHIFT  18
#define MCP25xxFD_CiCON_STEF_SHIFT      19
#define MCP25xxFD_CiCON_TXQEN_SHIFT     20
#define MCP25xxFD_CiCON_OPMOD_SHIFT     21
#define MCP25xxFD_CiCON_REQOP_SHIFT     24
#define MCP25xxFD_CiCON_ABAT_SHIFT      27
#define MCP25xxFD_CiCON_TXBWS_SHIFT     28
enum {
    MCP25xxFD_OPMODE_NORMAL = 0,
    MCP25xxFD_OPMODE_SLEEP = 1,
    MCP25xxFD_OPMODE_INTERNAL_LOOPBACK = 2,
    MCP25xxFD_OPMODE_LISTEN_ONLY = 3,
    MCP25xxFD_OPMODE_CONFIG = 4,
    MCP25xxFD_OPMODE_EXTERNAL_LOOPBACK = 5,
    MCP25xxFD_OPMODE_NORMAL_CAN_2_0 = 6,
    MCP25xxFD_OPMODE_RESTRICTED = 7
};

// CiNBTCFG fields
#define MCP25xxFD_CiNBTCFG_SJW_MASK     0x0000007F // R/W-0x0F
#define MCP25xxFD_CiNBTCFG_TSEG2_MASK   0x00007F00 // R/W-0x0F
#define MCP25xxFD_CiNBTCFG_TSEG1_MASK   0x00FF0000 // R/W-0x3E
#define MCP25xxFD_CiNBTCFG_BRP_MASK     0xFF000000 // R/W-0
#define MCP25xxFD_CiNBTCFG_SJW_SHIFT    0
#define MCP25xxFD_CiNBTCFG_TSEG2_SHIFT  8
#define MCP25xxFD_CiNBTCFG_TSEG1_SHIFT  16
#define MCP25xxFD_CiNBTCFG_BRP_SHIFT    24

// CiDBTCFG fields
#define MCP25xxFD_CiDBTCFG_SJW_MASK     0x0000000F // R/W-0x03
#define MCP25xxFD_CiDBTCFG_TSEG2_MASK   0x00000F00 // R/W-0x03
#define MCP25xxFD_CiDBTCFG_TSEG1_MASK   0x001F0000 // R/W-0xE
#define MCP25xxFD_CiDBTCFG_BRP_MASK     0xFF000000 // R/W-0
#define MCP25xxFD_CiDBTCFG_SJW_SHIFT    0
#define MCP25xxFD_CiDBTCFG_TSEG2_SHIFT  8
#define MCP25xxFD_CiDBTCFG_TSEG1_SHIFT  16
#define MCP25xxFD_CiDBTCFG_BRP_SHIFT    24

// CiTDC
#define MCP25xxFD_CiTDC_TDCV_MASK       0x0000003F // R/W-0
#define MCP25xxFD_CiTDC_TDCO_MASK       0x00007F00 // R/W-16
#define MCP25xxFD_CiTDC_TDCMOD_MASK     0x00030000 // R/W-2
#define MCP25xxFD_CiTDC_SID11EN_MASK    0x01000000 // R/W-0
#define MCP25xxFD_CiTDC_EDGFLTEN_MASK   0x02000000 // R/W-0
#define MCP25xxFD_CiTDC_TDCV_SHIFT      0
#define MCP25xxFD_CiTDC_TDCO_SHIFT      8
#define MCP25xxFD_CiTDC_TDCMOD_SHIFT    16
#define MCP25xxFD_CiTDC_SID11EN_SHIFT   24
#define MCP25xxFD_CiTDC_EDGFLTEN_SHIFT  25

// CiTBC
#define MCP25xxFD_CiTBC_TBC_MASK        0xFFFFFFFF // R/W-0
#define MCP25xxFD_CiTBC_TBC_SHIFT       0

// CiTSCON
#define MCP25xxFD_CiTSCON_TBCPRE_MASK   0x000003FF // R/W-0
#define MCP25xxFD_CiTSCON_TBCEN_MASK    0x00010000 // R/W-0
#define MCP25xxFD_CiTSCON_TSEOF_MASK    0x00020000 // R/W-0
#define MCP25xxFD_CiTSCON_TSRES_MASK    0x00040000 // R/W-0
#define MCP25xxFD_CiTSCON_TBCPRE_SHIFT  0
#define MCP25xxFD_CiTSCON_TBCEN_SHIFT   16
#define MCP25xxFD_CiTSCON_TSEOF_SHIFT   17
#define MCP25xxFD_CiTSCON_TSRES_SHIFT   18

// CiVEC
#define MCP25xxFD_CiVEC_ICODE_MASK      0x0000007F // R-64
#define MCP25xxFD_CiVEC_FILHIT_MASK     0x00001F00 // R-0
#define MCP25xxFD_CiVEC_TXCODE_MASK     0x007F0000 // R-64
#define MCP25xxFD_CiVEC_RXCODE_MASK     0x7F000000 // R-64
#define MCP25xxFD_CiVEC_ICODE_SHIFT     0
#define MCP25xxFD_CiVEC_FILHIT_SHIFT    8
#define MCP25xxFD_CiVEC_TXCODE_SHIFT    16
#define MCP25xxFD_CiVEC_RXCODE_SHIFT    24

// CiINT
#define MCP25xxFD_CiINT_TXIF_MASK       0x00000001 // R-0
#define MCP25xxFD_CiINT_RXIF_MASK       0x00000002 // R-0
#define MCP25xxFD_CiINT_TBCIF_MASK      0x00000004 // HS/C-0
#define MCP25xxFD_CiINT_MODIF_MASK      0x00000008 // HS/C-0
#define MCP25xxFD_CiINT_TEFIF_MASK      0x00000010 // R-0
#define MCP25xxFD_CiINT_ECCIF_MASK      0x00000100 // R-0
#define MCP25xxFD_CiINT_SPICRCIF_MASK   0x00000200 // R-0
#define MCP25xxFD_CiINT_TXATIF_MASK     0x00000400 // R-0
#define MCP25xxFD_CiINT_RXOVIF_MASK     0x00000800 // R-0
#define MCP25xxFD_CiINT_SERRIF_MASK     0x00001000 // HS/C-0
#define MCP25xxFD_CiINT_CERRIF_MASK     0x00002000 // HS/C-0
#define MCP25xxFD_CiINT_WAKIF_MASK      0x00004000 // HS/C-0
#define MCP25xxFD_CiINT_IVMIF_MASK      0x00008000 // HS/C-0
#define MCP25xxFD_CiINT_TXIE_MASK       0x00010000 // R/W-0
#define MCP25xxFD_CiINT_RXIE_MASK       0x00020000 // R/W-0
#define MCP25xxFD_CiINT_TBCIE_MASK      0x00040000 // R/W-0
#define MCP25xxFD_CiINT_MODIE_MASK      0x00080000 // R/W-0
#define MCP25xxFD_CiINT_TEFIE_MASK      0x00100000 // R/W-0
#define MCP25xxFD_CiINT_ECCIE_MASK      0x01000000 // R/W-0
#define MCP25xxFD_CiINT_SPICRCIE_MASK   0x02000000 // R/W-0
#define MCP25xxFD_CiINT_TXATIE_MASK     0x04000000 // R/W-0
#define MCP25xxFD_CiINT_RXOVIE_MASK     0x08000000 // R/W-0
#define MCP25xxFD_CiINT_SERRIE_MASK     0x10000000 // R/W-0
#define MCP25xxFD_CiINT_CERRIE_MASK     0x20000000 // R/W-0
#define MCP25xxFD_CiINT_WAKIE_MASK      0x40000000 // R/W-0
#define MCP25xxFD_CiINT_IVMIE_MASK      0x80000000 // R/W-0
#define MCP25xxFD_CiINT_TXIF_SHIFT      0
#define MCP25xxFD_CiINT_RXIF_SHIFT      1
#define MCP25xxFD_CiINT_TBCIF_SHIFT     2
#define MCP25xxFD_CiINT_MODIF_SHIFT     3
#define MCP25xxFD_CiINT_TEFIF_SHIFT     4
#define MCP25xxFD_CiINT_ECCIF_SHIFT     8
#define MCP25xxFD_CiINT_SPICRCIF_SHIFT  9
#define MCP25xxFD_CiINT_TXATIF_SHIFT    10
#define MCP25xxFD_CiINT_RXOVIF_SHIFT    11
#define MCP25xxFD_CiINT_SERRIF_SHIFT    12
#define MCP25xxFD_CiINT_CERRIF_SHIFT    13
#define MCP25xxFD_CiINT_WAKIF_SHIFT     14
#define MCP25xxFD_CiINT_IVMIF_SHIFT     15
#define MCP25xxFD_CiINT_TXIE_SHIFT      16
#define MCP25xxFD_CiINT_RXIE_SHIFT      17
#define MCP25xxFD_CiINT_TBCIE_SHIFT     18
#define MCP25xxFD_CiINT_MODIE_SHIFT     19
#define MCP25xxFD_CiINT_TEFIE_SHIFT     20
#define MCP25xxFD_CiINT_ECCIE_SHIFT     24
#define MCP25xxFD_CiINT_SPICRCIE_SHIFT  25
#define MCP25xxFD_CiINT_TXATIE_SHIFT    26
#define MCP25xxFD_CiINT_RXOVIE_SHIFT    27
#define MCP25xxFD_CiINT_SERRIE_SHIFT    28
#define MCP25xxFD_CiINT_CERRIE_SHIFT    29
#define MCP25xxFD_CiINT_WAKIE_SHIFT     30
#define MCP25xxFD_CiINT_IVMIE_SHIFT     31

// CiRXIF
#define MCP25xxFD_CiRXIF_RFIF_MASK      0xFFFFFFFE // R-0
#define MCP25xxFD_CiRXIF_RFIF_SHIFT     1

// CiRXOVIF
#define MCP25xxFD_CiRXOVIF_RFOVIF_MASK  0xFFFFFFFE // R-0
#define MCP25xxFD_CiRXOVIF_RFOVIF_SHIFT 1

// CiTXIF
// NOTE: The docs don't break out the bits for the FIFOs and the TXQueue.
//       It just notes that TFIF[0] (bit 0) is for the TXQueue.
//       I've provided #defines for both views.
#define MCP25xxFD_CiTXIF_TFIF_MASK      0xFFFFFFFF // R-0
#define MCP25xxFD_CiTXIF_TFIF_SHIFT      0
#define MCP25xxFD_CiTXIF_TFIF_TXQ_MASK  0x00000001 // R-0
#define MCP25xxFD_CiTXIF_TFIF_FIFO_MASK 0xFFFFFFFE // R-0
#define MCP25xxFD_CiTXIF_TFIF_TXQ_SHIFT  0
#define MCP25xxFD_CiTXIF_TFIF_FIFO_SHIFT 1

// CiTXATIF
// NOTE: The docs don't break out the bits for the FIFOs and the TXQueue.
//       It just notes that TFIF[0] (bit 0) is for the TXQueue.
//       I've provided #defines for both views.
#define MCP25xxFD_CiTXATIF_TFATIF_MASK  0xFFFFFFFF // R-0
#define MCP25xxFD_CiTXATIF_TFATIF_SHIFT 0
#define MCP25xxFD_CiTXATIF_TFATIF_TXQ_MASK      0x00000001 // R-0
#define MCP25xxFD_CiTXATIF_TFATIF_FIFO_MASK     0xFFFFFFFE // R-0
#define MCP25xxFD_CiTXATIF_TFATIF_TXQ_SHIFT     0
#define MCP25xxFD_CiTXATIF_TFATIF_FIFO_SHIFT    1

// CiTXREQ
// NOTE: The docs don't break out the bits for the FIFOs and the TXQueue.
//       It just notes that TFIF[0] (bit 0) is for the TXQueue.
//       I've provided #defines for both views.
#define MCP25xxFD_CiTXREQ_TXREQ_MASK    0xFFFFFFFF // S/HC-0
#define MCP25xxFD_CiTXREQ_TXREQ_SHIFT   0
#define MCP25xxFD_CiTXREQ_TXREQ_TXQ_MASK        0x00000001 // S/HC-0
#define MCP25xxFD_CiTXREQ_TXREQ_FIFO_MASK       0xFFFFFFFE // S/HC-0
#define MCP25xxFD_CiTXREQ_TXREQ_TXQ_SHIFT       0
#define MCP25xxFD_CiTXREQ_TXREQ_FIFO_SHIFT      1

// CiTREC
#define MCP25xxFD_CiTREC_REC_MASK       0x000000FF // R-0
#define MCP25xxFD_CiTREC_TEC_MASK       0x0000FF00 // R-0
#define MCP25xxFD_CiTREC_EWARN_MASK     0x00010000 // R-0
#define MCP25xxFD_CiTREC_RXWARN_MASK    0x00020000 // R-0
#define MCP25xxFD_CiTREC_TXWARN_MASK    0x00040000 // R-0
#define MCP25xxFD_CiTREC_RXBP_MASK      0x00080000 // R-0
#define MCP25xxFD_CiTREC_TXBP_MASK      0x00100000 // R-0
#define MCP25xxFD_CiTREC_TXBO_MASK      0x00200000 // R-1
#define MCP25xxFD_CiTREC_REC_SHIFT      0
#define MCP25xxFD_CiTREC_TEC_SHIFT      8
#define MCP25xxFD_CiTREC_EWARN_SHIFT    16
#define MCP25xxFD_CiTREC_RXWARN_SHIFT   17
#define MCP25xxFD_CiTREC_TXWARN_SHIFT   18
#define MCP25xxFD_CiTREC_RXBP_SHIFT     19
#define MCP25xxFD_CiTREC_TXBP_SHIFT     20
#define MCP25xxFD_CiTREC_TXBO_SHIFT     21

// CiBDIAG0
#define MCP25xxFD_CiBDIAG0_NRERRCNT_MASK        0x000000FF // R/W-0
#define MCP25xxFD_CiBDIAG0_NTERRCNT_MASK        0x0000FF00 // R/W-0
#define MCP25xxFD_CiBDIAG0_DRERRCNT_MASK        0x00FF0000 // R/W-0
#define MCP25xxFD_CiBDIAG0_DTERRCNT_MASK        0xFF000000 // R/W-0
#define MCP25xxFD_CiBDIAG0_NRERRCNT_SHIFT       0
#define MCP25xxFD_CiBDIAG0_NTERRCNT_SHIFT       8
#define MCP25xxFD_CiBDIAG0_DRERRCNT_SHIFT       16
#define MCP25xxFD_CiBDIAG0_DTERRCNT_SHIFT       24

// CiBDIAG1
#define MCP25xxFD_CiBDIAG1_EFMSGCNT_MASK        0x0000FFFF // R/W-0
#define MCP25xxFD_CiBDIAG1_NBIT0ERR_MASK        0x00010000 // R/W-0
#define MCP25xxFD_CiBDIAG1_NBIT1ERR_MASK        0x00020000 // R/W-0
#define MCP25xxFD_CiBDIAG1_NACKERR_MASK         0x00040000 // R/W-0
#define MCP25xxFD_CiBDIAG1_NFORMERR_MASK        0x00080000 // R/W-0
#define MCP25xxFD_CiBDIAG1_NSTUFERR_MASK        0x00100000 // R/W-0
#define MCP25xxFD_CiBDIAG1_NCRCERR_MASK         0x00200000 // R/W-0
#define MCP25xxFD_CiBDIAG1_TXBOERR_MASK         0x00800000 // R/W-0
#define MCP25xxFD_CiBDIAG1_DBIT0ERR_MASK        0x01000000 // R/W-0
#define MCP25xxFD_CiBDIAG1_DBIT1ERR_MASK        0x02000000 // R/W-0
#define MCP25xxFD_CiBDIAG1_DFORMERR_MASK        0x08000000 // R/W-0
#define MCP25xxFD_CiBDIAG1_DSTUFERR_MASK        0x10000000 // R/W-0
#define MCP25xxFD_CiBDIAG1_DCRCERR_MASK         0x20000000 // R/W-0
#define MCP25xxFD_CiBDIAG1_ESI_MASK             0x40000000 // R/W-0
#define MCP25xxFD_CiBDIAG1_DLCMM_MASK           0x80000000 // R/W-0
#define MCP25xxFD_CiBDIAG1_EFMSGCNT_SHIFT       0
#define MCP25xxFD_CiBDIAG1_NBIT0ERR_SHIFT       16
#define MCP25xxFD_CiBDIAG1_NBIT1ERR_SHIFT       17
#define MCP25xxFD_CiBDIAG1_NACKERR_SHIFT        18
#define MCP25xxFD_CiBDIAG1_NFORMERR_SHIFT       19
#define MCP25xxFD_CiBDIAG1_NSTUFERR_SHIFT       20
#define MCP25xxFD_CiBDIAG1_NCRCERR_SHIFT        21
#define MCP25xxFD_CiBDIAG1_TXBOERR_SHIFT        23
#define MCP25xxFD_CiBDIAG1_DBIT0ERR_SHIFT       24
#define MCP25xxFD_CiBDIAG1_DBIT1ERR_SHIFT       25
#define MCP25xxFD_CiBDIAG1_DFORMERR_SHIFT       27
#define MCP25xxFD_CiBDIAG1_DSTUFERR_SHIFT       28
#define MCP25xxFD_CiBDIAG1_DCRCERR_SHIFT        29
#define MCP25xxFD_CiBDIAG1_ESI_SHIFT            30
#define MCP25xxFD_CiBDIAG1_DLCMM_SHIFT          31

// CiTEFCON
#define MCP25xxFD_CiTEFCON_TEFNEIE_MASK         0x00000001 // R/W-0
#define MCP25xxFD_CiTEFCON_TEFHIE_MASK          0x00000002 // R/W-0
#define MCP25xxFD_CiTEFCON_TEFFIE_MASK          0x00000004 // R/W-0
#define MCP25xxFD_CiTEFCON_TEFOVIE_MASK         0x00000008 // R/W-0
#define MCP25xxFD_CiTEFCON_TEFTSEN_MASK         0x00000020 // R/W-0
#define MCP25xxFD_CiTEFCON_UINC_MASK            0x00000100 // S/HC-0
#define MCP25xxFD_CiTEFCON_FRESET_MASK          0x00000400 // S/HC-1
#define MCP25xxFD_CiTEFCON_FSIZE_MASK           0x1F000000 // R/W-0
#define MCP25xxFD_CiTEFCON_TEFNEIE_SHIFT        0
#define MCP25xxFD_CiTEFCON_TEFHIE_SHIFT         1
#define MCP25xxFD_CiTEFCON_TEFFIE_SHIFT         2
#define MCP25xxFD_CiTEFCON_TEFOVIE_SHIFT        3
#define MCP25xxFD_CiTEFCON_TEFTSEN_SHIFT        5
#define MCP25xxFD_CiTEFCON_UINC_SHIFT           8
#define MCP25xxFD_CiTEFCON_FRESET_SHIFT         10
#define MCP25xxFD_CiTEFCON_FSIZE_SHIFT          24

// CiTEFSTA
#define MCP25xxFD_CiTEFSTA_TEFNEIF_MASK         0x00000001 // R-0
#define MCP25xxFD_CiTEFSTA_TEFHIF_MASK          0x00000002 // R-0
#define MCP25xxFD_CiTEFSTA_TEFFIF_MASK          0x00000004 // R-0
#define MCP25xxFD_CiTEFSTA_TEFOVIF_MASK         0x00000008 // HS/C-0
#define MCP25xxFD_CiTEFSTA_TEFNEIF_SHIFT        0
#define MCP25xxFD_CiTEFSTA_TEFHIF_SHIFT         1
#define MCP25xxFD_CiTEFSTA_TEFFIF_SHIFT         2
#define MCP25xxFD_CiTEFSTA_TEFOVIF_SHIFT        3

// CiTEFUA
#define MCP25xxFD_CiTEFUA_TEFUA_MASK            0xFFFFFFFF // R-x
#define MCP25xxFD_CiTEFUA_TEFUA_SHIFT           0

// CiTXQCON
#define MCP25xxFD_CiTXQCON_TXQNIE_MASK          0x00000001 // R/W-0
#define MCP25xxFD_CiTXQCON_TXQEIE_MASK          0x00000004 // R/W-0
#define MCP25xxFD_CiTXQCON_TXATIE_MASK          0x00000010 // R/W-0
#define MCP25xxFD_CiTXQCON_TXEN_MASK            0x00000080 // R/W-1
#define MCP25xxFD_CiTXQCON_UINC_MASK            0x00000100 // S/HC-0
#define MCP25xxFD_CiTXQCON_TXREQ_MASK           0x00000200 // R/W/HC-0
#define MCP25xxFD_CiTXQCON_FRESET_MASK          0x00000400 // S/HC-1
#define MCP25xxFD_CiTXQCON_TXPRI_MASK           0x001F0000 // R/W-0
#define MCP25xxFD_CiTXQCON_TXAT_MASK            0x00600000 // R/W-3
#define MCP25xxFD_CiTXQCON_FSIZE_MASK           0x1F000000 // R/W-0
#define MCP25xxFD_CiTXQCON_PLSIZE_MASK          0xE0000000 // R/W-0
#define MCP25xxFD_CiTXQCON_TXQNIE_SHIFT         0
#define MCP25xxFD_CiTXQCON_TXQEIE_SHIFT         2
#define MCP25xxFD_CiTXQCON_TXATIE_SHIFT         4
#define MCP25xxFD_CiTXQCON_TXEN_SHIFT           7
#define MCP25xxFD_CiTXQCON_UINC_SHIFT           8
#define MCP25xxFD_CiTXQCON_TXREQ_SHIFT          9
#define MCP25xxFD_CiTXQCON_FRESET_SHIFT         10
#define MCP25xxFD_CiTXQCON_TXPRI_SHIFT          16
#define MCP25xxFD_CiTXQCON_TXAT_SHIFT           21
#define MCP25xxFD_CiTXQCON_FSIZE_SHIFT          24
#define MCP25xxFD_CiTXQCON_PLSIZE_SHIFT         29

// CiTXQSTA
#define MCP25xxFD_CiTXQSTA_TXQNIF_MASK          0x00000001 // R-1
#define MCP25xxFD_CiTXQSTA_TXQEIF_MASK          0x00000004 // R-1
#define MCP25xxFD_CiTXQSTA_TXATIF_MASK          0x00000010 // HS/C-0
#define MCP25xxFD_CiTXQSTA_TXERR_MASK           0x00000020 // HS/C-0
#define MCP25xxFD_CiTXQSTA_TXLARB_MASK          0x00000040 // HS/C-0
#define MCP25xxFD_CiTXQSTA_TXABT_MASK           0x00000080 // HS/C-0
#define MCP25xxFD_CiTXQSTA_TXQCI_MASK           0x00001F00 // R-0
#define MCP25xxFD_CiTXQSTA_TXQNIF_SHIFT         0
#define MCP25xxFD_CiTXQSTA_TXQEIF_SHIFT         2
#define MCP25xxFD_CiTXQSTA_TXATIF_SHIFT         4
#define MCP25xxFD_CiTXQSTA_TXERR_SHIFT          5
#define MCP25xxFD_CiTXQSTA_TXLARB_SHIFT         6
#define MCP25xxFD_CiTXQSTA_TXABT_SHIFT          7
#define MCP25xxFD_CiTXQSTA_TXQCI_SHIFT          8

// CiTXQUA
#define MCP25xxFD_CiTXQUA_TXQUA_MASK            0xFFFFFFFF // R-x
#define MCP25xxFD_CiTXQUA_TXQUA_SHIFT           0

// CiFIFOCONm
#define MCP25xxFD_CiFIFOCONm_TFNRFNIE_MASK      0x00000001 // R/W-0
#define MCP25xxFD_CiFIFOCONm_TFHRFHIE_MASK      0x00000002 // R/W-0
#define MCP25xxFD_CiFIFOCONm_TFERFFIE_MASK      0x00000004 // R/W-0
#define MCP25xxFD_CiFIFOCONm_RXOVIE_MASK        0x00000008 // R/W-0
#define MCP25xxFD_CiFIFOCONm_TXATIE_MASK        0x00000010 // R/W-0
#define MCP25xxFD_CiFIFOCONm_RXTSEN_MASK        0x00000020 // R/W-0
#define MCP25xxFD_CiFIFOCONm_RTREN_MASK         0x00000040 // R/W-0
#define MCP25xxFD_CiFIFOCONm_TXEN_MASK          0x00000080 // R/W-0
#define MCP25xxFD_CiFIFOCONm_UINC_MASK          0x00000100 // S/HC-0
#define MCP25xxFD_CiFIFOCONm_TXREQ_MASK         0x00000200 // R/W/HC-0
#define MCP25xxFD_CiFIFOCONm_FRESET_MASK        0x00000400 // S/HC-1
#define MCP25xxFD_CiFIFOCONm_TXPRI_MASK         0x001F0000 // R/W-0
#define MCP25xxFD_CiFIFOCONm_TXAT_MASK          0x00600000 // R/W-3
#define MCP25xxFD_CiFIFOCONm_FSIZE_MASK         0x1F000000 // R/W-0
#define MCP25xxFD_CiFIFOCONm_PLSIZE_MASK        0xE0000000 // R/W-0
#define MCP25xxFD_CiFIFOCONm_TFNRFNIE_SHIFT     0
#define MCP25xxFD_CiFIFOCONm_TFHRFHIE_SHIFT     1
#define MCP25xxFD_CiFIFOCONm_TFERFFIE_SHIFT     2
#define MCP25xxFD_CiFIFOCONm_RXOVIE_SHIFT       3
#define MCP25xxFD_CiFIFOCONm_TXATIE_SHIFT       4
#define MCP25xxFD_CiFIFOCONm_RXTSEN_SHIFT       5
#define MCP25xxFD_CiFIFOCONm_RTREN_SHIFT        6
#define MCP25xxFD_CiFIFOCONm_TXEN_SHIFT         7
#define MCP25xxFD_CiFIFOCONm_UINC_SHIFT         8
#define MCP25xxFD_CiFIFOCONm_TXREQ_SHIFT        9
#define MCP25xxFD_CiFIFOCONm_FRESET_SHIFT       10
#define MCP25xxFD_CiFIFOCONm_TXPRI_SHIFT        16
#define MCP25xxFD_CiFIFOCONm_TXAT_SHIFT         21
#define MCP25xxFD_CiFIFOCONm_FSIZE_SHIFT        24
#define MCP25xxFD_CiFIFOCONm_PLSIZE_SHIFT       29
enum {
    CiFIFOCONm_PLSIZE_8 = 0,
    CiFIFOCONm_PLSIZE_12 = 1,
    CiFIFOCONm_PLSIZE_16 = 2,
    CiFIFOCONm_PLSIZE_20 = 3,
    CiFIFOCONm_PLSIZE_24 = 4,
    CiFIFOCONm_PLSIZE_32 = 5,
    CiFIFOCONm_PLSIZE_48 = 6,
    CiFIFOCONm_PLSIZE_64 = 7
};

// CiFIFOSTAm
#define MCP25xxFD_CiFIFOSTAm_TFNRFNIF_MASK     0x00000001 // R-0
#define MCP25xxFD_CiFIFOSTAm_TFHRFHIF_MASK     0x00000002 // R-0
#define MCP25xxFD_CiFIFOSTAm_TFERFFIF_MASK     0x00000004 // R-0
#define MCP25xxFD_CiFIFOSTAm_RXOVIF_MASK       0x00000008 // HS/C-0
#define MCP25xxFD_CiFIFOSTAm_TXATIF_MASK       0x00000010 // HS/C-0
#define MCP25xxFD_CiFIFOSTAm_TXERR_MASK        0x00000020 // HS/C-0
#define MCP25xxFD_CiFIFOSTAm_TXLARB_MASK       0x00000040 // HS/C-0
#define MCP25xxFD_CiFIFOSTAm_TXABT_MASK        0x00000080 // HS/C-0
#define MCP25xxFD_CiFIFOSTAm_FIFOC_MASK        0x00001F00 // R-0
#define MCP25xxFD_CiFIFOSTAm_TFNRFNIF_SHIFT    0
#define MCP25xxFD_CiFIFOSTAm_TFHRFHIF_SHIFT    1
#define MCP25xxFD_CiFIFOSTAm_TFERFFIF_SHIFT    2
#define MCP25xxFD_CiFIFOSTAm_RXOVIF_SHIFT      3
#define MCP25xxFD_CiFIFOSTAm_TXATIF_SHIFT      4
#define MCP25xxFD_CiFIFOSTAm_TXERR_SHIFT       5
#define MCP25xxFD_CiFIFOSTAm_TXLARB_SHIFT      6
#define MCP25xxFD_CiFIFOSTAm_TXABT_SHIFT       7
#define MCP25xxFD_CiFIFOSTAm_FIFOC_SHIFT       8

// CiFIFOUAm
#define MCP25xxFD_CiFIFOUAm_FIFOUA_MASK         0xFFFFFFFF // R-x
#define MCP25xxFD_CiFIFOUAm_FIFOUA_SHIFT        0

// CiFLTCONm
// Each CiFLTCONm register has settings for 4 filters.
// For Filter 'm' first get the register via (int)(m / 4)
// Then which settings in the register via m % 4
#define MCP25xxFD_CiFLTCONm_FnBP_MASK(n)        (0x0000001F << (8 * ((n) % 4))) // R/W-0
#define MCP25xxFD_CiFLTCONm_FLTENn_MASK(n)      (0x00000080 << (8 * ((n) % 4))) // R/W-0
#define MCP25xxFD_CiFLTCONm_F0BP_MASK           MCP25xxFD_CiFLTCONm_FnBP_MASK(0)
#define MCP25xxFD_CiFLTCONm_FLTEN0_MASK         MCP25xxFD_CiFLTCONm_FLTENn_MASK(0)
#define MCP25xxFD_CiFLTCONm_F1BP_MASK           MCP25xxFD_CiFLTCONm_FnBP_MASK(1)
#define MCP25xxFD_CiFLTCONm_FLTEN1_MASK         MCP25xxFD_CiFLTCONm_FLTENn_MASK(1)
#define MCP25xxFD_CiFLTCONm_F2BP_MASK           MCP25xxFD_CiFLTCONm_FnBP_MASK(2)
#define MCP25xxFD_CiFLTCONm_FLTEN2_MASK         MCP25xxFD_CiFLTCONm_FLTENn_MASK(2)
#define MCP25xxFD_CiFLTCONm_F3BP_MASK           MCP25xxFD_CiFLTCONm_FnBP_MASK(3)
#define MCP25xxFD_CiFLTCONm_FLTEN3_MASK         MCP25xxFD_CiFLTCONm_FLTENn_MASK(3)
#define MCP25xxFD_CiFLTCONm_FnBP_SHIFT(n)       (8 * ((n) % 4))
#define MCP25xxFD_CiFLTCONm_FLTENn_SHIFT(n)     ((8 * ((n) % 4)) + 7)
#define MCP25xxFD_CiFLTCONm_F0BP_SHIFT          MCP25xxFD_CiFLTCONm_FnBP_SHIFT(0)
#define MCP25xxFD_CiFLTCONm_FLTEN0_SHIFT        MCP25xxFD_CiFLTCONm_FLTENn_SHIFT(0)
#define MCP25xxFD_CiFLTCONm_F1BP_SHIFT          MCP25xxFD_CiFLTCONm_FnBP_SHIFT(1)
#define MCP25xxFD_CiFLTCONm_FLTEN1_SHIFT        MCP25xxFD_CiFLTCONm_FLTENn_SHIFT(1)
#define MCP25xxFD_CiFLTCONm_F2BP_SHIFT          MCP25xxFD_CiFLTCONm_FnBP_SHIFT(2)
#define MCP25xxFD_CiFLTCONm_FLTEN2_SHIFT        MCP25xxFD_CiFLTCONm_FLTENn_SHIFT(2)
#define MCP25xxFD_CiFLTCONm_F3BP_SHIFT          MCP25xxFD_CiFLTCONm_FnBP_SHIFT(3)
#define MCP25xxFD_CiFLTCONm_FLTEN3_SHIFT        MCP25xxFD_CiFLTCONm_FLTENn_SHIFT(3)

// CiFLTOBJm
#define MCP25xxFD_CiFLTOBJm_SID_MASK            0x000007FF // R/W-0
#define MCP25xxFD_CiFLTOBJm_EID_MASK            0x1FFFF800 // R/W-0
#define MCP25xxFD_CiFLTOBJm_SID11_MASK          0x20000000 // R/W-0
#define MCP25xxFD_CiFLTOBJm_EXIDE_MASK          0x40000000 // R/W-0
#define MCP25xxFD_CiFLTOBJm_SID_SHIFT           0
#define MCP25xxFD_CiFLTOBJm_EID_SHIFT           11
#define MCP25xxFD_CiFLTOBJm_SID11_SHIFT         29
#define MCP25xxFD_CiFLTOBJm_EXIDE_SHIFT         30
// Not an actual field but makes writing a single 29bit MID easier
#define MCP25xxFD_CiFLTOBJm_MID_MASK            0x1FFFFFFF // R/W-0
#define MCP25xxFD_CiFLTOBJm_MID_SHIFT           0

// CiMASKm
#define MCP25xxFD_CiMASKm_MSID_MASK             0x000007FF // R/W-0
#define MCP25xxFD_CiMASKm_MEID_MASK             0x1FFFF800 // R/W-0
#define MCP25xxFD_CiMASKm_MSID11_MASK           0x20000000 // R/W-0
#define MCP25xxFD_CiMASKm_MIDE_MASK             0x40000000 // R/W-0
#define MCP25xxFD_CiMASKm_MSID_SHIFT            0
#define MCP25xxFD_CiMASKm_MEID_SHIFT            11
#define MCP25xxFD_CiMASKm_MSID11_SHIFT          29
#define MCP25xxFD_CiMASKm_MIDE_SHIFT            30
// Not an actual field but makes writing a single 29bit MID easier
#define MCP25xxFD_CiMASKm_MID_MASK              0x1FFFFFFF // R/W-0
#define MCP25xxFD_CiMASKm_MID_SHIFT             0

/**@}*/

/** @name MsgObjFields
 *
 * Masks and offsets for the various bitfields in a Tx/Rx message object.
 *
 * For each field <fieldName> in header byte <hdr> two #defines are provided:
 * - MCP25xxFD_MSGOBJ_<fieldName>_MASK masks out the bitfield in the header
 * - MCP25xxFD_MSGOBJ_<fieldName>_SHIFT the starting bit of the bitfield in the header
 */
/**@{*/
#define MCP25xxFD_MSGOBJ_SID11_MASK     0x20000000
#define MCP25xxFD_MSGOBJ_EID_MASK       0x1FFFF800
#define MCP25xxFD_MSGOBJ_SID_MASK       0x000007FF
#define MCP25xxFD_MSGOBJ_SID11_SHIFT    29
#define MCP25xxFD_MSGOBJ_EID_SHIFT      11
#define MCP25xxFD_MSGOBJ_SID_SHIFT      0

#define MCP25xxFD_MSGOBJ_DLC_MASK               0x0000000F
#define MCP25xxFD_MSGOBJ_IDE_MASK               0x00000010
#define MCP25xxFD_MSGOBJ_RTR_MASK               0x00000020
#define MCP25xxFD_MSGOBJ_BRS_MASK               0x00000040
#define MCP25xxFD_MSGOBJ_FDF_MASK               0x00000080
#define MCP25xxFD_MSGOBJ_ESI_MASK               0x00000100
#define MCP25xxFD_MSGOBJ_RX_FILHIT_MASK         0x0000F800
#define MCP25xxFD_MSGOBJ_TX_SEQ_MASK            0xFFFFFE00
#define MCP25xxFD_MSGOBJ_DLC_SHIFT              0
#define MCP25xxFD_MSGOBJ_IDE_SHIFT              4
#define MCP25xxFD_MSGOBJ_RTR_SHIFT              5
#define MCP25xxFD_MSGOBJ_BRS_SHIFT              6
#define MCP25xxFD_MSGOBJ_FDF_SHIFT              7
#define MCP25xxFD_MSGOBJ_ESI_SHIFT              8
#define MCP25xxFD_MSGOBJ_RX_FILHIT_SHIFT        11
#define MCP25xxFD_MSGOBJ_TX_SEQ_SHIFT           9

/***********************************
 * SPI Commands
 **********************************/
/**
 * The commands/instructions that can be issued to the MCP25xxFD via SPI.
 */
/**@{*/
#define MCP25xxFD_SPI_CMD_RESET         0x0
#define MCP25xxFD_SPI_CMD_READ          0x3
#define MCP25xxFD_SPI_CMD_WRITE         0x2
#define MCP25xxFD_SPI_CMD_READ_CRC      0xB
#define MCP25xxFD_SPI_CMD_WRITE_CRC     0xA
#define MCP25xxFD_SPI_CMD_WRITE_SAFE    0xC

#define MCP25xxFD_MAKE_CMD(cmd, addr) ((((cmd) & 0xF) << 4) | (((addr) >> 8) & 0xF) | (((addr) & 0xFF) << 8))
/**@}*/

/** Minimum oscillator clock frequency required by the MCP25xxFD */
#define MCP25xxFD_MIN_OSC_FREQUENCY 2000000
/** Maximum bit clock frequency supported by the MCP25xxFD.
 */
#define MCP25xxFD_MAX_OSC_FREQUENCY 40000000
#define MCP25xxFD_MAX_TDCO 63

#define MCP25xxFD_MAX_FIFO 31
#define MCP25xxFD_MAX_FILTER 31

/**
 * Total amount of RAM available for FIFOs
 */
#define MCP25xxFD_RAM_SIZE 2048

/**
 * The size of a slot in a transmit FIFO.
 * 64 for the maximum possible payload (64 bytes)
 * 8 for the message overhead
 */
#define MCP25xxFD_TX_FIFO_SLOT_SIZE (64+8)

/**
 * The size of a slot in a receive FIFO.
 * 64 for the maximum possible payload (64 bytes)
 * 8 for the message overhead
 * 4 for the timestamp
 */
#define MCP25xxFD_RX_FIFO_SLOT_SIZE (64+8+4)

/**
 * The size of a slot in the Tx Event FIFO
 * 8 for the message overhead
 * 4 for the timestamp
 */
#define MCP25xxFD_TE_FIFO_SLOT_SIZE (8+4)

/**
 * Maximum number of slots for a FIFO.
 */
#define MCP25xxFD_MAX_FIFO_SLOTS 32

/**
 * Make an address to a message object in the MCP25xxFD's internal
 * memory based on its user address.
 */
#define MCP25xxFD_MAKE_MSGOBJ_ADDR(ua) (0x400 + (ua))

/**
 * Interrupt event callback, defined in mcp25xxfd.c. ctx is the mcp25xxfd_dev_t*
 * for the interface the interrupt fired on; dispatched from event.c's IST.
 */
int mcp25xxfd_event_cb(void *ctx, int event, int irq);

/**
 * Bring up every configured MCP25xxFD chip (hint.mcp25xxfd.<unit>.dev). Called
 * by mcp25xxfd_module.c on MOD_LOAD.
 */
int mcp25xxfd_attach(void);

/**
 * Tear down every attached instance. Called by mcp25xxfd_module.c on unload
 * (SYSUNINIT).
 */
void mcp25xxfd_detach(void);

#endif  // MCP25xxFD_H_
