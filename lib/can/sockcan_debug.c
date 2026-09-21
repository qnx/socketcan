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

#include "hw/libsockcan.h"

#include "sockcan_debug.h"

void sockcan_log_frame(const char *where, struct ifnet *ifp, struct mbuf *m) {
#if CAN_LOG_LEVEL >= 2
    if (m->m_len < (int)sizeof(can_frame_t)) {
        return;
    }
    const can_frame_t *f = mtod(m, const can_frame_t *);
    // Classic can_dlc and FD len live at the same offset, and data[] starts at the
    // same offset in both frame types, so one decode handles both. Cap the byte
    // count by the FD maximum and by what the mbuf actually carries.
    const uint8_t *data  = (const uint8_t *)f + __builtin_offsetof(can_frame_t, data);
    unsigned       avail = (unsigned)m->m_len - __builtin_offsetof(can_frame_t, data);
    unsigned       n     = f->can_dlc;
    if (n > avail) {
        n = avail;
    }
    if (n > CANFD_MAX_DLEN) {
        n = CANFD_MAX_DLEN;
    }
    char hex[2 * CANFD_MAX_DLEN + 1];
    for (unsigned i = 0; i < n; i++) {
        snprintf(&hex[i * 2], 3, "%02x", data[i]);
    }
    hex[n * 2] = '\0';
    CAN_LOG_DEBUG("%s %s id=0x%x len=%u data=0x%s", where, ifp->if_xname, f->can_id, f->can_dlc, hex);
#else
    (void)where;
    (void)ifp;
    (void)m;
#endif
}

void sockcan_log_bittiming(const char *where, struct ifnet *ifp, const can_bittiming_t *bt) {
    CAN_LOG_DEBUG("%s %s bitrate=%u clkrate=%u sample_point=%u tq=%u prop_seg=%u "
                  "phase_seg1=%u phase_seg2=%u sjw=%u brp=%u",
                  where, ifp->if_xname, bt->bitrate, bt->clkrate, bt->sample_point, bt->tq, bt->prop_seg,
                  bt->phase_seg1, bt->phase_seg2, bt->sjw, bt->brp);
}
