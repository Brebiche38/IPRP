/**\file icd.c
 * iPRP Control Daemon
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */
#define IPRP_FILE ICD_MAIN

#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "icd.h"

/* Thread descriptors */
pthread_t control_thread;
pthread_t ports_thread;
pthread_t as_thread;
pthread_t pb_thread;
pthread_t si_thread;
pthread_t time_thread;

/* Global variables */
iprp_host_t this; /** Information about the current machine */

/**
 Control daemon entry point

 The ICD first parses the arguments given to it to create the structure representing the current host.
 It then assigns the specific receiver-side netfilter queue numbers to transmit to the IRD and IMD.
 It finally launches all the ICD routines and then waits forever.
*/
int main(int argc, char const *argv[]) {
	DEBUG("In routine");
	int err;

	// Manual setup (creation of this)
	if (argc < 2) return EXIT_FAILURE;
	this.nb_ifaces = atoi(argv[1]);
	if (this.nb_ifaces < 1 || this.nb_ifaces > IPRP_MAX_INDS || argc != this.nb_ifaces + 2) return EXIT_FAILURE;

	for (int i = 0; i < this.nb_ifaces; ++i) {
		this.ifaces[i].ind = i;
		inet_pton(AF_INET, argv[i+2], &this.ifaces[i].addr);
	}
	DEBUG("Interface setup complete");

	/* Phase 1: Setup */
	// TODO launch time routine

	// Seed random generator
	srand(time(NULL)); // TODO thread-safe
	iprp_icd_recv_queues_t recv_queues;
	do {
		recv_queues.ird = rand() % 65535;
		recv_queues.imd = rand() % 65535;
		recv_queues.ird_imd = rand() % 65535;
	} while (recv_queues.ird == recv_queues.imd || recv_queues.ird == recv_queues.ird_imd || recv_queues.imd == recv_queues.ird_imd);
	DEBUG("Receiver-side queue numbers assigned");

	if ((err = pthread_create(&time_thread, NULL, time_routine, NULL))) {
		ERR("Unable to setup time thread", err);
	}
	DEBUG("Time thread setup");

	if ((err = pthread_create(&ports_thread, NULL, ports_routine, &recv_queues))) {
		ERR("Unable to setup ports thread", err);
	}
	DEBUG("Ports thread setup");

	// Setup receiver thread
	if ((err = pthread_create(&control_thread, NULL, control_routine, NULL))) {
		ERR("Unable to setup control thread", err);
	}
	DEBUG("Control thread setup");

	// Setup sender thread
	if ((err = pthread_create(&as_thread, NULL, as_routine, NULL))) {
		ERR("Unable to setup active senders thread", err);
	}
	DEBUG("Active senders thread setup");

	// Setup sendcap routine
	if ((err = pthread_create(&pb_thread, NULL, pb_routine, NULL)) != 0) {
		ERR("Unable to setup peerbase thread", err);
	}
	DEBUG("Peerbase thread created");

	// TODO Setup sender interfaces cleanup routine
	if ((err = pthread_create(&si_thread, NULL, si_routine, NULL)) != 0) {
		ERR("Unable to setup sender interfaces thread", err);
	}
	DEBUG("Sender interfaces thread created");

	LOG("Control daemon successfully launched");

	while(1);

	/*
	// Join on other threads (not expected to happen)
	void* return_value;
	if ((err = pthread_join(time_thread, &return_value))) {
		ERR("Unable to join on time thread", err);
	}
	ERR("Time thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(ports_thread, &return_value))) {
		ERR("Unable to join on ports thread", err);
	}
	ERR("Ports thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(control_thread, &return_value))) {
		ERR("Unable to join on control thread", err);
	}
	ERR("Control thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(as_thread, &return_value))) {
		ERR("Unable to join on active senders thread", err);
	}
	ERR("Active senders thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(pb_thread, &return_value))) {
		ERR("Unable to join on peerbase thread", err);
	}
	ERR("Peerbase thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(si_thread, &return_value))) {
		ERR("Unable to join on sender interfaces thread", err);
	}
	ERR("Sender interfaces thread unexpectedly finished execution", (int) return_value);
	*/

	/* Should not reach this part */
	LOG("Last man standing at the end of the apocalypse");
	return EXIT_FAILURE;
}