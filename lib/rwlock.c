#include "../include/rwlock.h"

// NO CHECKS ON ARRAY SIZE - USE WITH CARE

rw_mutex_array* get_mutex_array(int size) {

	int i;
	rw_mutex_array* new_mutex = kmalloc(sizeof(rw_mutex_array), GFP_KERNEL);
	new_mutex->read_mutex_array = kmalloc(sizeof(struct mutex)*size, GFP_KERNEL);
	new_mutex->write_mutex_array = kmalloc(sizeof(struct mutex)*size, GFP_KERNEL);
	new_mutex->readers = kmalloc(sizeof(int)*size, GFP_KERNEL);

	for (i = 0; i < size; i++) {
		mutex_init(new_mutex->read_mutex_array + i);
		mutex_init(new_mutex->write_mutex_array + i);
		new_mutex->readers[i] = 0;
	}

	return new_mutex;

}

int take_read_lock(rw_mutex_array* mutex_array, int index) {

	int res = mutex_lock_interruptible(mutex_array->read_mutex_array + index);
	if (res == -1)
		return res;

	if (mutex_array->readers[index] == 0) {
		int res = mutex_lock_interruptible(mutex_array->write_mutex_array + index);
		if (res == -1)
			return res;
	}

	mutex_array->readers[index]++;

	mutex_unlock(mutex_array->read_mutex_array + index);

	return 1;
}


int release_read_lock(rw_mutex_array* mutex_array, int index) {

	int res = mutex_lock_interruptible(mutex_array->read_mutex_array + index);
	if (res == -1)
		return res;

	if (mutex_array->readers[index] == 1)
		mutex_unlock(mutex_array->write_mutex_array + index);


	mutex_array->readers[index]--;

	mutex_unlock(mutex_array->read_mutex_array + index);

	return 1;

}

int take_write_lock(rw_mutex_array* mutex_array, int index) {

	return mutex_lock_interruptible(mutex_array->write_mutex_array+index);

}


void release_write_lock(rw_mutex_array* mutex_array, int index) {

	mutex_unlock(mutex_array->write_mutex_array+index);

}
