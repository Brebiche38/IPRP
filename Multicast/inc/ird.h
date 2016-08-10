/**\file ird.h
 * Header file for IRD
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_IRD_
#define __IPRP_IRD_

#include <stdbool.h>
#include <stdint.h>
//#include <libnetfilter_queue/libnetfilter_queue.h>
//#include <linux/ip.h>

#include "global.h"

// Begin cleaned up defines
#define IRD_T_EXP 120
#define IRD_T_CLEANUP 5
#define IPRP_DD_MAX_LOST_PACKETS 1024
// End cleaned up defines

#define IRD_SI_T_CACHE 3

/* Typedefs */
typedef struct iprp_receiver_link iprp_receiver_link_t;

/* Thread routines */
void* handle_routine(void* arg);
void* si_routine(void* arg);

/* Receiver link structure */
struct iprp_receiver_link {
	// Info (fixed) vars
	struct in_addr src_addr;
	uint16_t src_port;
	unsigned char snsid[20];
	// State (variable) vars
	uint32_t list_sn[IPRP_DD_MAX_LOST_PACKETS];
	uint32_t high_sn;
	time_t last_seen;
};

//#ifndef MCAST_JOIN_SOURCE_GROUP
//#define MCAST_JOIN_SOURCE_GROUP 46

struct ip_mreqn {
	struct in_addr imr_multiaddr;
	struct in_addr imr_address;
	int imr_ifindex;
};

struct ip_mreq_source {
	struct in_addr imr_multiaddr;
	struct in_addr imr_interface;
	struct in_addr imr_sourceaddr;
};
//#endif

#endif /* __IPRP_IRD_ */