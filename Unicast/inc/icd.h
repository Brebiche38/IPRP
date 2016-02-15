/**\file icd.h
 * Header file for icd.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_ICD_
#define __IPRP_ICD_

#include "types.h"

int main(int argc, char const *argv[]);
void control_routine(int recv_pipe_write, int send_pipe_write);
void* receiver_routine(void *arg);
void* receiver_sendcap_routine(void *arg);
void* sender_routine(void *arg);

#endif /* __IPRP_ICD_ */