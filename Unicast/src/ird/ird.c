#define IPRP_FILE IRD_MAIN

#include <stdlib.h>
#include <pthread.h>

#include "ird.h"

int imd_queue_id;

pthread_t time_thread;
pthread_t handle_thread;
pthread_t si_thread;

int main(int argc, char const *argv[]) {
	int err;
	
	// Get arguments
	int queue_id = atoi(argv[1]);
	imd_queue_id = atoi(argv[2]);
	DEBUG("Started");

	// Launch time routine
	if ((err = pthread_create(&time_thread, NULL, time_routine, NULL))) {
		ERR("Unable to setup time thread", err);
	}
	DEBUG("Time thread created");

	// Launch receiving routine
	if ((err = pthread_create(&handle_thread, NULL, handle_routine, (void*) queue_id))) {
		ERR("Unable to setup receive thread", err);
	}
	DEBUG("Receive thread created");

	// Launch subscribe routine
	if ((err = pthread_create(&si_thread, NULL, si_routine, NULL))) {
		ERR("Unable to setup sender interfaces thread", err);
	}
	DEBUG("Sender interfaces thread created");

	LOG("Receiver daemon successfully created");

	while(1);

	/*
	// Join on threads (should not happen)
	void* return_value;
	if ((err = pthread_join(time_thread, &return_value))) {
		ERR("Unable to join on time thread", err);
	}
	ERR("Time thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(handle_thread, &return_value))) {
		ERR("Unable to join on handle thread", err);
	}
	ERR("Handle thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(si_thread, &return_value))) {
		ERR("Unable to join on sender interfaces thread", err);
	}
	ERR("Sender interfaces thread unexpectedly finished execution", (int) return_value);
	*/
	
	LOG("Last man standing at the end of the apocalypse");
	return EXIT_FAILURE;
}