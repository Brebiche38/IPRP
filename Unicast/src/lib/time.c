#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "global.h"

time_t curr_time = 0;

void *time_routine(void* arg) {
	while(true) {
		curr_time = time(NULL);
		sleep(1);
	}
}