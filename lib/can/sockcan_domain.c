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

#include "sockcan_domain.h"

#include "sockcan_usrreq.h"

#include "sockcan_debug.h"

#include <sys/domain.h>



/**
 * Creates the domain for AF_CAN.
 *
 * @note Currently only CAN_RAW is supported.
 */
struct domain sockcandomain = {.dom_family   = AF_CAN,
                               .dom_name     = "can",
                               .dom_nprotosw = nitems(sockcan_protosw),
                               .dom_protosw  = {&sockcan_protosw[0]},
                            };

DOMAIN_SET(sockcan);