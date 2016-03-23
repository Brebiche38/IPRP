/**\file global.h
 * Header file for lib/global.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_GLOBAL_
#define __IPRP_GLOBAL_

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <pthread.h>

#include "config.h"
#include "debug.h"

#define IPRP_CTLMSG_SECRET 0x3dbf391e
#define IPRP_IFACE_NAME_LENGTH 10
#define IPRP_MONITORED_PORTS_FILE "files/ports.txt"
#define IPRP_MAX_MONITORED_PORTS 16
#define IPRP_TPORT 3
#define IPRP_NFQUEUE_MAX_LENGTH 100


typedef uint32_t iprp_version_t;
typedef uint8_t iprp_ind_t;
typedef struct iprp_ctlmsg iprp_ctlmsg_t;
typedef struct iprp_capmsg iprp_capmsg_t;
typedef struct iprp_ackmsg iprp_ackmsg_t;
typedef enum iprp_msgtype iprp_msgtype_t;
typedef struct iprp_interface iprp_iface_t;
typedef struct iprp_host iprp_host_t;
typedef struct iprp_header iprp_header_t;
//typedef struct iprp_host iprp_sender_t;
//typedef struct iprp_host iprp_recv_t;

/* Library functions */
struct iprp_interface {
	char name[IPRP_IFACE_NAME_LENGTH];
	iprp_ind_t ind;
	struct in_addr addr;
};

struct iprp_host {
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
	uint16_t dest_port; // Dest port of the UDP packet that triggered the CAP message
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

struct iprp_header {
	uint8_t version;
	unsigned char snsid[20]; // TODO check if size possible
	uint32_t seq_nb;
	uint16_t dest_port;
	iprp_ind_t ind;
	char hmac[160];
};

int compare_hosts(iprp_host_t *h1, iprp_host_t *h2);
iprp_iface_t *get_iface_from_ind(iprp_host_t *host, iprp_ind_t ind);
int ind_match(iprp_host_t *sender, iprp_host_t *receiver);

/* List structure */
typedef struct list list_t;
typedef struct list_elem list_elem_t;

struct list_elem {
	void *elem;
	list_elem_t *prev;
	list_elem_t *next;
};

struct list {
	list_elem_t *head;
	list_elem_t *tail;
	size_t size;
	pthread_mutex_t mutex;
};

void list_init(list_t *list);
void list_append(list_t *list, void* value);
void list_delete(list_t *list, list_elem_t *elem);
size_t list_size(list_t *list);
// ATTENTION: doubly linked but not cyclical

// Not used yet
#define LIST_ITERATE(list, type, var, body) 	\
	void *iterator = (list);					\
	while(iterator != NULL) {					\
		type *var = (type *) iterator->elem;	\
		{ body }								\
		iterator = iterator->next;				\
	}


#endif /* __IPRP_GLOBAL_ */