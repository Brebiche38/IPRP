/**\file types.h
 * General custom type definitions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_TYPES_
#define __IPRP_TYPES_

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

#include "config.h"

#define IPRP_CTLMSG_SECRET 0x3dbf391e
#define IPRP_IFACE_NAME_LENGTH 10

typedef uint32_t iprp_version_t;
typedef uint8_t iprp_ind_t;
typedef struct iprp_ctlmsg iprp_ctlmsg_t;
typedef struct iprp_capmsg iprp_capmsg_t;
typedef struct iprp_ackmsg iprp_ackmsg_t;
typedef enum iprp_msgtype iprp_msgtype_t;
typedef struct iprp_interface iprp_iface_t;
typedef struct iprp_host iprp_host_t;
//typedef struct iprp_host iprp_sender_t;
//typedef struct iprp_host iprp_recv_t;

/* Library functions */
struct iprp_interface {
	char name[IPRP_IFACE_NAME_LENGTH];
	iprp_ind_t ind;
	struct in_addr addr;
};

struct iprp_host {
	uint32_t id;
	size_t nb_ifaces;
	iprp_iface_t ifaces[IPRP_MAX_IFACE];
};

enum iprp_msgtype {
	IPRP_CAP,
	IPRP_ACK
};

struct iprp_capmsg {
	iprp_version_t iprp_version;
	iprp_host_t receiver;
	uint16_t src_port; // Source port of the UDP packet that triggered the CAP message
	uint16_t dest_port; // Dest port of the UDP packet that triggered the CAP message TODO why?
};

// Not unicast
struct iprp_ackmsg {
	/* TODO */
};

struct iprp_ctlmsg {
	uint32_t secret;
	iprp_msgtype_t msg_type;
	union {
		iprp_capmsg_t cap_message;
		iprp_ackmsg_t ack_message;
	} message;
};

#endif /* __IPRP_TYPES_ */