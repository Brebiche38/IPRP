/**\file config.h
 * Error code definitions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_ERROR_
#define __IPRP_ERROR_

enum {
	IPRP_ERR_NOINIT,
	IPRP_ERR_NULLPTR,
	IPRP_ERR_BADFORMAT,
	IPRP_ERR_LOOKUPFAIL,
	IPPR_ERR_MULTIPLE_SAME_IND
};

#endif /* __IPRP_ERROR_ */