#include "/usr/src/linux/drivers/isdn/avmb1/capiutil.h"

/* exportetd constants to include for daemons */

#define CAPIPROXY_MAXCONTR	4

#define CAPIPROXY_MAJOR		69
#define MSG_QUEUE_LENGTH	16

#define CAPIPROXY_VERSIONLEN	8
#define CAPIPROXY_VER_DRIVER	0

#define TYPE_PROXY		0x00
#define TYPE_MESSAGE		0x01
#define TYPE_REGISTER_APP	0x02
#define TYPE_RELEASE_APP	0x03
#define TYPE_SHUTDOWN		0x04

#define IOCTL_SET_DATA		0x01
#define IOCTL_ERROR_REGFAIL	0x02
#define IOCTL_APPL_REGISTERED	0x03

/*
 * Before we send any messages to the server, we
 * determine whether it has been registered
 */

#define APPL_FREE		0x00
#define APPL_CONFIRMED		0x01 /* Server registered the application */
#define APPL_WAITING		0x02 /* Server has not yet answered */
#define APPL_REVOKED		0x03

#define CARD_FREE		0x00
#define CARD_RUNNING		0x01
#define CARD_REMOVING		0x02
#define CARD_NOT_READY		0x03

/*
 * Error codes for possible ioctl() errors
 * to be sent to the daemon
 */

#define ENOTREG			1 /* Controller not registered */

/* capiutil extensions */

#define CAPIMSG_SETL3C(m, lev)		capimsg_setu32(m, 6, lev) 
#define CAPIMSG_SETDBC(m, dbc)		capimsg_setu32(m, 10, dbc)
#define	CAPIMSG_SETDBL(m, dbl)		capimsg_setu32(m, 14, dbl)

#define CAPIMSG_L3C(m)	CAPIMSG_U32(m, 6)
#define CAPIMSG_DBC(m)	CAPIMSG_U32(m, 10)
#define CAPIMSG_DBL(m)	CAPIMSG_U32(m, 14)

