/**\file global.c
 * Utility functions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "../../inc/global.h"

/**
 Fills a sockaddr_in structure with the given information
*/
void sockaddr_fill(struct sockaddr_in *sockaddr, struct in_addr addr, uint16_t port) {
	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);
	sockaddr->sin_addr = addr;
	memset(sockaddr->sin_zero, 0, sizeof(sockaddr->sin_zero));
}

/**
 Returns the matching INDs between the given host and INDs
*/
iprp_ind_bitmap_t ind_match(iprp_host_t *sender, iprp_ind_bitmap_t receiver_inds) {
	iprp_ind_bitmap_t sender_inds = 0;

	for (int i = 0; i < sender->nb_ifaces; ++i) {
		sender_inds |= (1 << sender->ifaces[i].ind);
	}

	return sender_inds & receiver_inds;
}

/**
 Returns the current thread name (used for debugging)
*/
char* iprp_thr_name(iprp_thread_t thread) {
	switch(thread) {
		case ICD_MAIN: return "icd";
		case ICD_CTL: return "icd-control";
		case ICD_PORTS: return "icd-ports";
		case ICD_AS: return "icd-as";
		case ICD_PB: return "icd-pb";
		case ICD_SI: return "icd-si";

		case ISD_MAIN: return "isd";
		case ISD_HANDLE: return "isd-handle";
		case ISD_PB: return "isd-pb";

		case IMD_MAIN: return "imd";
		case IMD_HANDLE: return "imd-handle";
		case IMD_AS: return "imd-as";

		case IRD_MAIN: return "ird";
		case IRD_HANDLE: return "ird-handle";
		case IRD_SI: return "ird-si";

		default: return "???";
	}	
}