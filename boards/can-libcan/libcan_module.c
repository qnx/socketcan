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
 * can-libcan io-sock driver.
 */

#include <hw/libsockcan.h>
#include <sockcan_debug.h>
#include <hw/socketcan.h>

#include "libcan.h"

/* Driver version, exported as a sysctl. */
static SYSCTL_NODE(_hw, OID_AUTO, libcan, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "libcan shim global parameters");
int libcan_drvr_ver = IOSOCK_VERSION_CUR;
SYSCTL_INT(_qnx_driver, OID_AUTO, libcan, CTLFLAG_RD, &libcan_drvr_ver, 0, "Version");

static int module_event_handler(module_t mod, int what, void *arg __unused) {
    int err = 0;
    switch (what) {
        case MOD_LOAD:
            err = libcan_attach();
            break;
        default:
            printf("Unsupported module event:%d\n", what);
            return (EOPNOTSUPP);
    }
    return (err);
}

static moduledata_t libcan_moduledata = {"libcan", module_event_handler, NULL};

DECLARE_MODULE(libcan, libcan_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_DEPEND(libcan, can, 1, 1, 1);
MODULE_VERSION(libcan, 1);

struct _iosock_module_version iosock_module_version = IOSOCK_MODULE_VER_SYM_INIT;

SYSUNINIT(libcan_detach, SI_SUB_DRIVERS, SI_ORDER_ANY, libcan_detach, NULL);
