#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "global.h"

void list_init(list_t *list) {
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	list->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
}

void list_append(list_t *list, void* value) {
	list_elem_t *new_elem = malloc(sizeof(list_elem_t));
	if (!new_elem) {
		ERR("Unable to allocate list element", errno);
	}

	new_elem->elem = value;
	new_elem->next = NULL;
	new_elem->prev = list->tail;

	if (list->head == NULL) { // Implicitly, list->tail == NULL too
		list->head = new_elem;
	} else {
		list->tail->next = new_elem;
	}
	list->tail = new_elem;

	list->size++;
}

void list_delete(list_t *list, list_elem_t *elem) {
	if (list->head == elem) {
		list->head = elem->next;
	}
	if (list->tail == elem) {
		list->tail = elem->prev;
	}

	// Global case
	if (elem->next) {
		elem->next->prev = elem->prev;
	}
	if (elem->prev) {
		elem->prev->next = elem->next;
	}

	list->size--;

	free(elem);
}

size_t list_size(list_t *list) {
	return list->size;
}

void list_lock(list_t *list) {
	pthread_mutex_lock(&list->mutex);
}

void list_unlock(list_t *list) {
	pthread_mutex_unlock(&list->mutex);
}