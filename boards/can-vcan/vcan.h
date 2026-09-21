/*
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
 *
 * BlackBerry Limited and its licensors  retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from BlackBerry Limited
 * is strictly prohibited.
 */

#ifndef _VCAN_H_
#define _VCAN_H_

#include <hw/libsockcan.h>

/*
 * Interface between the two halves of the vcan driver:
 *
 *   vcan_module.c - io-sock/newbus module glue (boilerplate; copy for a new driver)
 *   vcan.c        - the actual driver implementation (what you write)
 *
 * The glue calls vcan_attach() at module load and vcan_detach() at unload.
 */

/* Set up the driver at module load. Returns 0 on success or an errno. */
int vcan_attach(void);

/* Tear the driver down at module unload. */
void vcan_detach(void);

#endif /* _VCAN_H_ */
