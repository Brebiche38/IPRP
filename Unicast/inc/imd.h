#ifndef __IPRP_IMD_
#define __IPRP_IMD_

#include "global.h"
#include "activesenders.h"

// Begin cleaned up defines
#define IMD_T_AS_CACHE 3
#define IMD_AS_TEXP 120
// End cleaned up defines

/* Thread routines */
void* handle_routine(void* arg);
void* ird_handle_routine(void* arg);
void* as_routine(void* arg);

#endif /* __IPRP_IMD_ */