#ifndef LOCK_H
#define LOCK_H

#include <linux/mutex.h>
#include <linux/slab.h>

typedef struct {

	struct mutex* write_mutex_array;
	struct mutex* read_mutex_array;
	int* readers;

} rw_mutex_array;

rw_mutex_array* get_mutex_array(int size);
int take_read_lock(rw_mutex_array* mutex_array, int index);
int release_read_lock(rw_mutex_array* mutex_array, int index);
int take_write_lock(rw_mutex_array* mutex_array, int index);
void release_write_lock(rw_mutex_array* mutex_array, int index);

#endif