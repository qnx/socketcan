/*
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
 *
 * BlackBerry Limited and its licensors  retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from BlackBerry Limited
 * is strictly prohibited.
 */

/*
 * vcan io-sock driver.
 */

#include "vcan.h"

static int s_drvr_ver = IOSOCK_VERSION_CUR;
static SYSCTL_NODE(_hw, OID_AUTO, vcan, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "sample driver global parameters");
SYSCTL_INT(_qnx_driver, OID_AUTO, sam, CTLFLAG_RD, &s_drvr_ver, 0, "Version");

static int module_event_handler(module_t mod, int what, void *arg __unused) {
    int err = 0;
    switch (what) {
        case MOD_LOAD:
            err = vcan_attach();
            break;
        default:
            printf("Unsupported module event:%d\n", what);
            return (EOPNOTSUPP);
    }
    return (err);
}

static moduledata_t vcan_moduledata = {"vcan", module_event_handler, NULL};

DECLARE_MODULE(vcan, vcan_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_DEPEND(vcan, can, 1, 1, 1);
MODULE_VERSION(vcan, 1);

struct _iosock_module_version iosock_module_version = IOSOCK_MODULE_VER_SYM_INIT;

SYSUNINIT(vcan_detach, SI_SUB_DUMMY, SI_ORDER_ANY, vcan_detach, NULL);
