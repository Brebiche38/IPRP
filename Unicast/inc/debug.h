/**\file debug.h
 * Debugging definitions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_DEBUG_
#define __IPRP_DEBUG_

#include <stdlib.h>
#include <stdio.h>

// Errors
enum {
	IPRP_ERR_NOINIT,
	IPRP_ERR_NULLPTR,
	IPRP_ERR_BADFORMAT,
	IPRP_ERR_LOOKUPFAIL,
	IPPR_ERR_MULTIPLE_SAME_IND,
	IPRP_ERR_NFQUEUE
};

#define ERR(msg, var)	printf("Error: %s (%d)\n", msg, var); exit(EXIT_FAILURE)

// Threads
typedef enum {
	IPRP_ICD = (1 << 0),
	IPRP_ICD_RECV = (1 << 1),
	IPRP_ICD_SEND = (1 << 2),
	IPRP_ICD_SENDCAP = (1 << 3),
	IPRP_ICD_PORTS = (1 << 4),

	IPRP_ISD = (1 << 8),
	IPRP_ISD_SEND = (1 << 9),
	IPRP_ISD_CACHE = (1 << 10),
	IPRP_ISD_HANDLE = (1 << 11),

	IPRP_IMD = (1 << 16),
	IPRP_IMD_MONITOR = (1 << 17),
	IPRP_IMD_CLEANUP = (1 << 18),
	IPRP_IMD_HANDLE = (1 << 19),

	IPRP_IRD = (1 << 24),
	IPRP_IRD_RECV = (1 << 25),
	IPRP_IRD_CLEANUP = (1 << 26),
	IPRP_IRD_HANDLE = (1 << 27),
} iprp_thread_t;

char* iprp_thr_name(iprp_thread_t thread) {
	switch(thread) {
		case IPRP_ICD: return "icd";
		case IPRP_ICD_RECV: return "icd-receiver";
		case IPRP_ICD_SEND: return "icd-sender";
		case IPRP_ICD_SENDCAP: return "icd-sendcap";
		case IPRP_ICD_PORTS: return "icd-ports";

		case IPRP_ISD: return "isd";
		case IPRP_ISD_SEND: return "isd-send";
		case IPRP_ISD_CACHE: return "isd-cache";
		case IPRP_ISD_HANDLE: return "isd-handle";

		case IPRP_IMD: return "imd";
		case IPRP_IMD_MONITOR: return "imd-monitor";
		case IPRP_IMD_CLEANUP: return "imd-cleanup";
		case IPRP_IMD_HANDLE: return "imd-handle";

		case IPRP_IRD: return "ird";
		case IPRP_IRD_RECV: return "ird-recv";
		case IPRP_IRD_CLEANUP: return "ird-cleanup";
		case IPRP_IRD_HANDLE: return "ird-handle";
	}	
}

// Debugging
#define DEBUG_INFO
//#define DEBUG_NONE
//#define DEBUG_FULL

#define MSG(thread, msg) \
	printf("[%s] ", iprp_thr_name(thread)); \
	printf("%s\n", msg);

#ifdef DEBUG_NONE
	#define LOG(thread, msg) {}
	#define DEBUG(thread, msg) {}
#endif

#ifdef DEBUG_INFO
	#define LOG(thread, msg) MSG(thread, msg)
	#define DEBUG(thread, msg) {}
#endif

#ifdef DEBUG_FULL
	// Threads needing debugging (flags)
	#define DEBUG_THREADS 0

	#define LOG(thread, msg) MSG(thread, msg)

	#define DEBUG(thread, msg) \
		if (thread & DEBUG_THREADS) { MSG(thread, msg) } else {}
#endif

#endif /* __IPRP_DEBUG_ */