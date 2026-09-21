#ifndef _LINUX_SOCKET_H
#define _LINUX_SOCKET_H

#include <sys/socket.h>

#include "sockios.h"

#define SO_RCVBUFFORCE (SO_VENDOR+1)
#define SO_PRIORITY (SO_VENDOR+2)
#define SO_MARK (SO_VENDOR+3)
#define SO_TIMESTAMPNS (SO_VENDOR+4)
#define SO_TIMESTAMPING (SO_VENDOR+5)

#define SCM_TIMESTAMPNS SO_TIMESTAMPNS
#define SCM_TIMESTAMPING SO_TIMESTAMPING

#ifdef PF_CAN
#undef PF_CAN
#endif
#ifdef AF_CAN
#undef AF_CAN
#endif

#define AF_CAN                  (56)
#define PF_CAN                  AF_CAN

// Not the best place for this but it should cause CLOCK_TAI to point
// to the realtime clock
#ifdef CLOCK_TAI
#undef CLOCK_TAI
#define CLOCK_TAI CLOCK_REALTIME
#endif

#endif /* _LINUX_SOCKET_H */
