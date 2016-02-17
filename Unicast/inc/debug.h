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

// Debugging definitions
typedef enum {
	DEBUG_PROD,
	DEBUG_LIGHT,
	DEBUG_VERBOSE
} debug_level_t;

#define DEBUG_LEVEL DEBUG_VERBOSE

#define ERR(msg, var)	printf("Error: %s (%d)\n", msg, var); exit(EXIT_FAILURE)
#define LOG(msg)		if (DEBUG_LEVEL == DEBUG_VERBOSE) printf("%s\n", msg)

#endif /* __IPRP_DEBUG_ */