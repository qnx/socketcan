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

#ifndef MCP25xxFD_UTIL_H_
#define MCP25xxFD_UTIL_H_

#include <stdint.h>
#include <stdio.h>
#include <time.h>

/**
 * Clamp a value to be within a specified range.
 *
 * @param[in] x         The value to clamp
 * @param[in] lower     The lower bound of the valid range.
 * @param[in] upper     The lower bound of the valid range.
 *
 * @return The value x if it is in the range lower <= x <= upper. Otherwise
 *         it returns lower if x < lower or upper if x > upper.
 */
#define CLAMP(x, lower, upper) ((x) <= (upper) ? ((x) >= (lower) ? (x) : (lower)) : (upper))

/**
 * Calculates the absolute difference, ignoring signing, of two values.
 *
 * @param[in] a The first value
 * @param[in] b The second value
 *
 * @return The absolute difference between a and b
 */
#define ABS_DIFF(a, b) ((a) <= (b) ? (b) - (a) : (a) - (b))

/**
 * Converts a value in seconds to the equivalent number of nanoseconds.
 *
 * @param[in] s The number of seconds
 *
 * @return The equivalent number of nanoseconds
 */
#define SEC_TO_NS(s) ((s) * (1000ull * 1000ull * 1000ull))

/**
 * Get a 32bit SW timestamp, in us.
 *
 * @return A 32bit SW timestamp in us. It starts at 0 on system reset and
 *         rolls over every 71 minutes or so.
 */
static inline uint32_t get_sw_timestamp() {
  struct timespec ts = {0, 0};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(timespec2nsec(&ts) / 1000);  // NOLINT
}

#endif  // MCP25xxFD_UTIL_H_
