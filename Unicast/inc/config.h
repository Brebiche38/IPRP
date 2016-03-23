/**\file config.h
 * Configuration values
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_CONFIG_
#define __IPRP_CONFIG_

#define IPRP_VERSION 1
#define IPRP_TCAP 3 // 30 seconds
#define IPRP_MAX_SENDERS 64
#define IPRP_CTL_PORT 1000
#define IPRP_DATA_PORT 1001
#define IPRP_MAX_IFACE 4
#define IPRP_MAX_INDS 16
#define IPRP_PATH_LENGTH 50
#define IPRP_PACKET_BUFFER_SIZE 4096

#define IPRP_ISD_BINARY_LOC "bin/isd"
#define IPRP_IRD_BINARY_LOC "bin/ird"
#define IPRP_IMD_BINARY_LOC "bin/imd"

// TODO Convention: all error handling is done in icd/imd/isd/ird files, library files return error codes

#endif /* __IPRP_CONFIG_ */