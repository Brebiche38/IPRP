/**\file log.h
 * Debugging definitions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_LOG_
#define __IPRP_LOG_

#include <stdlib.h>
#include <stdio.h>

typedef enum {
	DEBUG_PROD,
	DEBUG_LIGHT,
	DEBUG_VERBOSE
} debug_level_t;

#define DEBUG_LEVEL DEBUG_VERBOSE

#define ERR(msg, var)	printf("Error: %s (%d)\n", msg, var); exit(EXIT_FAILURE)
#define LOG(msg)		if (DEBUG_LEVEL == DEBUG_VERBOSE) printf("%s\n", msg)

#endif /* __IPRP_LOG_ */