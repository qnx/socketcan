#ifndef _LINUX_SOCKIOS_H
#define _LINUX_SOCKIOS_H

#include <sys/sockio.h>
#include <sys/time.h>

// Add additional IOs.
// Give them a unique class so that, hopefully, they don't conflict with
// existing ones.
#define	SIOCGSTAMP      _IOR('@',  1, struct timeval)		/* get timestamp */
#define	SIOCGIFNAME  _IOWR('s',  29, struct ifreq) /*Get the interface name */

// QNX struct ifreq doesn't have a ifr_ifindex member, so use a different one
#define	ifr_ifindex	ifr_ifru.ifru_index	/* interface index */
#endif /* _LINUX_SOCKIOS_H */
