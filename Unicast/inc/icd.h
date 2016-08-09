/**\file icd.h
 * Header file for ICD
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_ICD_
#define __IPRP_ICD_

#include "global.h"
#include "peerbase.h"
#include "activesenders.h"
#include "senderifaces.h"

// Begin cleaned up defines
#define ICD_T_PORTS 10
#define IPRP_T_SI_CACHE 3
#define ICD_SI_TEXP 60
#define IPRP_PB_TEXP 60
// End cleaned up defines

#define IPRP_BACKOFF_D 10
#define IPRP_BACKOFF_LAMBDA 2.5

/* ICD Structures */
typedef struct {
	iprp_link_t link;
	iprp_ind_bitmap_t inds;
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
void* si_routine(void *arg);

#endif /* __IPRP_ICD_ */