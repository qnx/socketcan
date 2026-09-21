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

/**
 * This file contains basic logging for socket can used when debugging issues with the platform.
 *
 * When not used logs are compiled out to reduce wasted performance.
 *
 * To increase the global logging update CAN_LOG_LEVEL below where
 *  TRACE  = 3
 *  DEBUG  = 2
 *  INFO   = 1
 *  NTC/WRN/ERR = 0
 *
 * To increase a per file logging define CAN_LOG_LEVEL before including this header.
 */

#ifndef _SOCKCAN_DEBUG_H_
#define _SOCKCAN_DEBUG_H_

#ifndef CAN_LOG_LEVEL
#define CAN_LOG_LEVEL 1
#endif  // CAN_LOG_LEVEL

#define CAN_LOG(_f, ...) printf("sockcan(%s): " _f "\n", __func__, ##__VA_ARGS__)

#if CAN_LOG_LEVEL >= 3
#define CAN_TRACE() CAN_LOG("")
#define CAN_LOG_DEBUG2(_f, ...) CAN_LOG("DBG2 " _f, ##__VA_ARGS__)
#else
#define CAN_TRACE()
#define CAN_LOG_DEBUG2(_f, ...)
#endif

#if CAN_LOG_LEVEL >= 2
#define CAN_LOG_DEBUG(_f, ...) CAN_LOG("DBG " _f, ##__VA_ARGS__)
#else
#define CAN_LOG_DEBUG(_f, ...)
#endif

#if CAN_LOG_LEVEL >= 1
#define CAN_LOG_INFO(_f, ...) CAN_LOG("INF " _f, ##__VA_ARGS__)
#else
#define CAN_LOG_INFO(_f, ...)
#endif

#define CAN_LOG_WRN(_f, ...) CAN_LOG("NTC " _f, ##__VA_ARGS__)
#define CAN_LOG_NTC(_f, ...) CAN_LOG("NTC " _f, ##__VA_ARGS__)
#define CAN_LOG_ERR(_f, ...) CAN_LOG("ERR " _f, ##__VA_ARGS__)

/**
 * @brief Log a CAN frame.
 *
 * @note This logs at CAN_LOG_DBG so level must be correctly set to see the frame.
 *
 * @param where Who is setting the frame (if it is a TX or RX path)
 * @param ifp The source interface of the message
 * @param m The mbuf containing a can_frame.
 */
void sockcan_log_frame(const char *where, struct ifnet *ifp, struct mbuf *m);

/**
 * @brief Log the CAN bit timing
 *
 * @note This logs at CAN_LOG_DBG so level must be correctly set to see the frame.
 *
 * @param where Who is setting the bt.
 * @param ifp Interface which bt are being set for
 * @param bt Bit timing settings.
 */
void sockcan_log_bittiming(const char *where, struct ifnet *ifp, const can_bittiming_t *bt);

#endif  // _SOCKCAN_DEBUG_H_