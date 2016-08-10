/**\file sender.h
 * Header file for lib/sender.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_SENDER_
#define __IPRP_SENDER_

#include "global.h"

#define IPRP_PEERBASE_PATH_LENGTH 57
#define IPRP_T_PB_CACHE 3

/* Structures */
typedef struct iprp_peerbase {
	iprp_link_t link;
	iprp_host_t host;
	iprp_ind_bitmap_t inds;
	// struct in_addr dest_addr[IPRP_MAX_INDS]; // Unicast only
} iprp_peerbase_t;

/* Disk functions */
int peerbase_store(const char* path, iprp_peerbase_t *base);
int peerbase_load(const char* path, iprp_peerbase_t *base);

#endif /* __IPRP_SENDER_ */