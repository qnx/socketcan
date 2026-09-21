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

#ifndef _SOCKCAN_DOMAIN_H_
#define _SOCKCAN_DOMAIN_H_

/**
 * @brief Initialize AF_CAN domain
 *
 * @note once internal dependencies are removed this function can safely be ignored.
 *
 * @return 0 on success, otherwise an error code.
 */
int sockcan_domain_create(void);


extern struct domain sockcandomain;

#endif  // _SOCKCAN_DOMAIN_H_
