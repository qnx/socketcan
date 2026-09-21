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

#include <hw/libsockcan.h>

// Must be included after
#include <qnx/qnx_modload.h>

#include "sockcan_debug.h"
#include "sockcan_domain.h"
#include "sockcan_usrreq.h"

int mod_ver = IOSOCK_VERSION_CUR;

SYSCTL_INT(_qnx_module, OID_AUTO, sample, CTLFLAG_RD, &mod_ver, 0, "Version");

static void run_module(void) {
    CAN_TRACE();
    sockcan_global_init();
    sockcan_domain_create();
}

static int module_event_handler(module_t mod, int what, void *arg __unused) {
    switch (what) {
        case MOD_LOAD:
            run_module();
            break;
        default:
            printf("Unsupported module event:%d\n", what);
            return (EOPNOTSUPP);
    }

    return (0);
}

static moduledata_t modsocan_moduledata = {"module_sockcan", module_event_handler, NULL};

MODULE_VERSION(sockcan, 1);
DECLARE_MODULE(sockcan, modsocan_moduledata, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);

struct _iosock_module_version iosock_module_version = IOSOCK_MODULE_VER_SYM_INIT;

static void sockcan_uninit(void *arg) {}
SYSUNINIT(sockcan_uninit, SI_SUB_DUMMY, SI_ORDER_ANY, sockcan_uninit, NULL);
