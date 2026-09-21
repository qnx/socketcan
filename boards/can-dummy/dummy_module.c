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

/*
 * can-dummy io-sock driver.
 */

#include "dummy.h"

#include <hw/libsockcan.h>
#include <sockcan_debug.h>
#include <hw/socketcan.h>

/* Driver version, exported as a sysctl. */
static SYSCTL_NODE(_hw, OID_AUTO, candummy, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "dummy can driver global parameters");
int dummy_drvr_ver = IOSOCK_VERSION_CUR;
SYSCTL_INT(_qnx_driver, OID_AUTO, candummy, CTLFLAG_RD, &dummy_drvr_ver, 0, "Version");

static int module_event_handler(module_t mod, int what, void *arg __unused) {
    int err = 0;
    switch (what) {
        case MOD_LOAD:
            err = candummy_attach();
            break;
        default:
            printf("Unsupported module event:%d\n", what);
            return (EOPNOTSUPP);
    }
    return (err);
}

static moduledata_t candummy_moduledata = {"candummy", module_event_handler, NULL};

DECLARE_MODULE(candummy, candummy_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_DEPEND(candummy, can, 1, 1, 1);
MODULE_VERSION(candummy, 1);

struct _iosock_module_version iosock_module_version = IOSOCK_MODULE_VER_SYM_INIT;

SYSUNINIT(candummy_detach, SI_SUB_DRIVERS, SI_ORDER_ANY, candummy_detach, NULL);
