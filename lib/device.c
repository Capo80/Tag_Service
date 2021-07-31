#include "../include/device.h"

char* device_file = NULL;
time64_t last_read = 0;
unsigned short reference_counter;
rw_mutex_array* elimination_lock = NULL;

int dev_open(struct inode *inode, struct file *file) {
	
	AUDIT
	printk("%s: dev open\n", MODNAME);

	if (elimination_lock == NULL)
		elimination_lock = get_mutex_array(1);

	__sync_fetch_and_add(&reference_counter, 1);

	take_read_lock(elimination_lock, 0);

	if (device_file == NULL) {		
		char* new_alloc = kmalloc(FILE_SIZE, GFP_KERNEL); 
		if (!__sync_bool_compare_and_swap(&device_file, NULL, new_alloc)) {
			//someone was faster in the allocation - free
			kfree(new_alloc);
		}
	}
	
	release_read_lock(elimination_lock, 0);

	return 0;
}

ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

	int i, j, index = 0;
	AUDIT
	printk("%s: dev read\n", MODNAME);

	//read if it is been lonf from last read
	if (last_read == 0 || ktime_get_seconds()-last_read > DEVICE_TIMER) {
		index += snprintf(device_file, 12*5+2, "%-12s%-12s%-12s%-12s%-12s\n", "Number", "Key", "Owner", "Level", "Waiting");
		index += 1;
		for (i = 0; i < TAGS_MAX; i++) {
	        if (tag_array[i] != NULL) {
	            for (j = 0; j < LEVELS_MAX; j++){
	            	if (tag_array[i]->owner.val == INT_MAX)
	                	index += snprintf(device_file+index, 12*5+2, "%-12d%-12d%-12s%-12d%-12d\n", i, key_array[i], "NO_OWN", j, tag_array[i]->priority_levels[j]->reference_counter);
	            	else
	                	index += snprintf(device_file+index, 12*5+2, "%-12d%-12d%-12d%-12d%-12d\n", i, key_array[i], tag_array[i]->owner.val, j, tag_array[i]->priority_levels[j]->reference_counter);
                	index += 1;

	            }
        	}

   		}

		last_read = ktime_get_seconds();
	} else
		index = strlen(device_file) + 1;

	AUDIT
	printk("%s: off %lld, index %d, strlen %ld, len %ld\n", MODNAME, *off, index, strlen(device_file) + 1, len);

	AUDIT
	printk("%s: %s\n", MODNAME, device_file);

	//check that offset is whithin boundaries
	if (*off >= index)
		return 0;
	else if (*off + len > index)
		len = index - *off;

	AUDIT
	printk("%s: off %lld, index %d, strlen %ld, len %ld\n", MODNAME, *off, index, strlen(device_file) + 1, len);

	if (copy_to_user(buff, device_file, len))
		return -EFAULT;

	*off += len;
	return len;
}

int dev_release(struct inode *inode, struct file *file) {

	AUDIT
	printk("%s: dev close\n", MODNAME);

	//if last read is very far in the past release memory
	//make sure no other thread ha the device open
	if (__sync_fetch_and_sub(&reference_counter, 1) == 1 && device_file != NULL && (last_read == 0 || ktime_get_seconds()-last_read > DEVICE_MEMORY_RELEASE)) {
		
		take_write_lock(elimination_lock, 0);
		if (reference_counter == 0 && device_file != NULL)
			kfree(device_file);
		device_file = NULL;
		release_write_lock(elimination_lock, 0);

		AUDIT
		printk("%s: memory released\n", MODNAME);
	}

	return 0;
}

struct tag_device_data dev_data;

//device fops
struct file_operations fops = {
  .owner = THIS_MODULE,
  .read = dev_read,
  .open = dev_open,
  .release = dev_release
};

