/**\file icd.h
 * Header file for ICD
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */

#ifndef __IPRP_ICD_
#define __IPRP_ICD_

#include "global.h"
#include "peerbase.h"
#include "activesenders.h"
#ifdef IPRP_MULTICAST
 #include "senderifaces.h"
#endif

// Begin cleaned up defines
#define ICD_T_PORTS 10
#define IPRP_T_SI_CACHE 3
#ifdef IPRP_MULTICAST
 #define ICD_SI_TEXP 60
#endif
#define IPRP_PB_TEXP 60
#define IPRP_CTLMSG_SECRET 0x3dbf391e
#define IPRP_TCAP 3 // 30 seconds
#define IPRP_BACKOFF_D 10
#define IPRP_BACKOFF_LAMBDA 2.5

/* Control messages */
typedef enum {
	IPRP_CAP,
	IPRP_ACK
} iprp_msgtype_t;

typedef struct {
	iprp_version_t iprp_version;
#ifndef IPRP_MULTICAST
	iprp_host_t receiver;
#endif
	struct in_addr src_addr;
	struct in_addr dest_addr;
	iprp_ind_bitmap_t inds;
	uint16_t src_port;
	uint16_t dest_port;
} iprp_capmsg_t;

#ifdef IPRP_MULTICAST
typedef struct {
	iprp_host_t host;
	struct in_addr group_addr;
	struct in_addr src_addr;
} iprp_ackmsg_t;
#endif

typedef struct {
	uint32_t secret;
#ifndef IPRP_MULTICAST
	iprp_capmsg_t cap_message;
#else
	iprp_msgtype_t msg_type;
	union {
		iprp_capmsg_t cap_message;
		iprp_ackmsg_t ack_message;
	} message;
#endif
} iprp_ctlmsg_t;

/* ICD Structures */
typedef struct {
	iprp_link_t link;
	iprp_ind_bitmap_t inds;
#ifndef IPRP_MULTICAST
	struct in_addr dest_addr[IPRP_MAX_INDS];
#endif	
	uint16_t queue_id;
	pid_t isd_pid;
	time_t last_cap;
} iprp_icd_base_t;

typedef struct {
	uint16_t ird;
	uint16_t imd;
	uint16_t ird_imd;
} iprp_icd_recv_queues_t;

/* Control flow routines */
void* control_routine(void *arg);
void* ports_routine(void* arg);
void* as_routine(void *arg);
void* pb_routine(void *arg);
#ifdef IPRP_MULTICAST
void* si_routine(void *arg);
#endif

#endif /* __IPRP_ICD_ */