#define EXPORT_SYMTAB
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <linux/syscalls.h>
#include "./include/vtpmo.h"
#include "./include/usctm.h"
#include "./include/bitmap.h"
#include "./include/structs.h"
#include "./include/parameters.h"
#include "./include/device.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pasquale Caporaso <caporasopasquale97@gmail.com>");
MODULE_DESCRIPTION("TAG_SERVICE");

dev_t device_major;
struct class *device_class;
DEFINE_MUTEX(insert_mutex);
DECLARE_WAIT_QUEUE_HEAD(insert_wq);

// ###### helpers ################

void recycle(ktime_t test_time) {
    int i;
    for(i = 0; i < TAGS_MAX; i++) {
        if (tag_array[i] != NULL && test_time - tag_array[i]->last_use > RECYCLE_TIMER) {
            AUDIT
            printk("%s: trying to recycle %lld, %lld", MODNAME, test_time, tag_array[i]->last_use);
            remove_tag(i);
        }
    }
}

int check_key(int key) {
    int i = 0;
    while (i < TAGS_MAX) {
        if (key_array[i] == key) 
            return i;
        i++;
    }
    return -1;
}

// ###### syscalls definition ##################

// The tag_get syscall check if the parameters are correct
// Then tries to find a tag to open if need or return an existing one if it is already open
// After finding the correct tag it attachs a descriptor and return it to the user

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _tag_get, int, key, int, command, int, permission){
#else
asmlinkage int sys_tag_get(int key, int command, int permission) {
#endif

    int i = 0;
    int j = 0;
    short version;
    int index;
    int wq_res = 0;
    tag* new_tag;
    struct key_node* my_key;


    AUDIT
    printk("%s: called tag_get, pid: %d, parent_pid: %d, real parent: %d, group leader: %d\n",MODNAME, current->pid, current->parent->pid, current->real_parent->pid, current->group_leader->pid);
    // INT_MAX cannot be used as key
    // command must be either 0 or IPC_PRIVATE
    // permisson must be either 0 or T_PERSONAL 
    if (key == INT_MAX)
        return -EKEYREJECTED;
    if ((command != 1 && command != IPC_PRIVATE) || (permission != 0 && permission != T_PERSONAL))
        return -EINVAL;


    //recycle dead tags before inserting
    recycle(ktime_get_seconds());

    AUDIT
    printk("%s: called tag_get, parameters are correct\n",MODNAME);

    // check if key is associated with existing tag
    // if it is, check the private flag and owner flag, if correct return the descriptor
    index = check_key(key);        
    if (index != -1) {
        AUDIT
        printk("%s: key found checking flags\n",MODNAME);
        if (tag_array[index]->private != IPC_PRIVATE && (tag_array[index]->owner.val == INT_MAX || (tag_array[index]->owner.val == current->cred->uid.val))) {
            tag_array[index]->last_use = ktime_get_seconds();
            return get_new_descriptor(&tables, index, tag_versions[index]);
        } else
            return -EPERM;
    }

    AUDIT
    printk("%s: called tag_get, key does not exist\n",MODNAME); 

    AUDIT
    print_list(insert_list);

    //coordinate with other creation threads
    //check if another is inserting, if it is wait of him to finish
    mutex_lock_interruptible(&insert_mutex);

    AUDIT
    printk("%s: searching key\n", MODNAME);

    my_key = find_key(insert_list, key); 
    if (my_key == NULL) {
        insert_key(&insert_list, key);
        insert_list->ref_counter++;
        mutex_unlock(&insert_mutex);
        my_key = find_key(insert_list, key);
    } else {
        my_key->ref_counter++;
        mutex_unlock(&insert_mutex);
        if (!my_key->complete)
            wq_res = wait_event_interruptible(insert_wq, my_key->complete);
        // remove ref
        if (__sync_fetch_and_sub(&(my_key->ref_counter), 1) == 1) {
            mutex_lock_interruptible(&insert_mutex);
            remove_key(&insert_list, key);
            mutex_unlock(&insert_mutex);
        }
        if (wq_res != 0)
            return -ERESTARTSYS;

    }
    AUDIT
    printk("%s: entered protected zone\n",MODNAME);

    //check again if key is inside now that we are the only one that can insert with this key
    index = check_key(key);        
    if (index != -1) {
        //signal other threads that key has been inserted
        my_key->complete = true;
        wake_up_interruptible(&insert_wq);
        
        if (tag_array[index] != IPC_PRIVATE && (tag_array[index]->owner.val == INT_MAX || (tag_array[index]->owner.val == current->cred->uid.val))) {
            tag_array[index]->last_use = ktime_get_seconds();
            return get_new_descriptor(&tables, index, tag_versions[index]);
        } else
            return -EPERM;
    }

    // create new tag
    new_tag = allocate_new_tag(command, permission);

    AUDIT
    printk("%s: new tag created\n",MODNAME);

    // try to insert tag in key array, loop over array twice, fails if there is no open space
    for (; j < 2; j++)
        for (i = 0; i < TAGS_MAX; i++) {
            AUDIT
            printk("%s: trying %d\n",MODNAME, i);
                
            if (__sync_bool_compare_and_swap(key_array + i,INT_MAX,key)) {
                tag_array[i] = new_tag;
                
                AUDIT
                printk("%s: key saved in %d\n",MODNAME, i);
                
                //no need for sync update of version, there are no other descriptors in use  
                version = ++tag_versions[i];

                //signal other threads that key has been inserted
                my_key->complete = true;
                if (__sync_fetch_and_sub(&(my_key->ref_counter), 1) == 1) {
                    mutex_lock_interruptible(&insert_mutex);
                    remove_key(&insert_list, key);
                    mutex_unlock(&insert_mutex);
                }
                wake_up_interruptible(&insert_wq);

                AUDIT
                printk("%s: others have awoken\n",MODNAME);

                return get_new_descriptor(&tables, i, version);

                
            }
        }

    //other threads may retry to insert
    my_key->complete = true;
    if (__sync_fetch_and_sub(&(my_key->ref_counter), 1) == 1) {
        mutex_lock_interruptible(&insert_mutex);
        remove_key(&insert_list, key);
        mutex_unlock(&insert_mutex);
    }
    wake_up_interruptible(&insert_wq);

    //no free tags
    return -EBUSY;

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
static unsigned long sys_tag_get = (unsigned long) __x64_sys_tag_get;	
#else
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(4, _tag_send, int, tag, int, level, char*, buffer, size_t, size){
#else
asmlinkage int sys_tag_send(int tag, int level, char* buffer, size_t size) {
#endif

    descriptor_info* my_info;
    unsigned char real_tag;

    AUDIT
    printk("%s: called tag_send, pid: %d, parent_pid: %d, real parent: %d, group leader: %d\n",MODNAME, current->pid, current->parent->pid, current->real_parent->pid, current->group_leader->pid);
    //check if level is whitin boundaries
    if (level < 0 || level >= LEVELS_MAX) {
        AUDIT
        printk("%s: Invalid level\n", MODNAME);
        return -EINVAL;
    }

    //check if process has open descriptors
    my_info = find_check_descriptor(&tables, tag);
    if (my_info == NULL)
        return -EINVAL;
    real_tag = my_info->descriptors[tag];

    tag_array[real_tag]->last_use = ktime_get_seconds();
    
    AUDIT
    printk("%s: all checks passed in send\n", MODNAME);

    return epoch_write(real_tag, level, buffer, size);

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
static unsigned long sys_tag_send = (unsigned long) __x64_sys_tag_send;	
#else
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(4, _tag_receive, int, tag, int, level, char*, buffer, size_t, size){
#else
asmlinkage int sys_tag_receive(int tag, int level, char* buffer, size_t size){
#endif

    descriptor_info* my_info;
    unsigned char real_tag = 0;
    prio_level* my_level;
    short ref_counter;
    int ret;


    AUDIT
    printk("%s: called tag_receive, pid: %d, parent_pid: %d, real parent: %d, group leader: %d\n",MODNAME, current->pid, current->parent->pid, current->real_parent->pid, current->group_leader->pid);
    
    //check if level is whitin boundaries
    if (level < 0 || level >= LEVELS_MAX) {
        AUDIT
        printk("%s: Invalid level\n", MODNAME);
        return -EINVAL;
    }

    
    //check if process has open descriptors
    my_info = find_check_descriptor(&tables, tag);
    if (my_info == NULL)
        return -EINVAL;
    real_tag = my_info->descriptors[tag];

    tag_array[real_tag]->last_use = ktime_get_seconds();

    AUDIT
    printk("%s: all checks have passed\n", MODNAME);    

    //update reference, need to check for value -1 for tag that is about to get removed
    //need sync
    do {    
        ref_counter = tag_array[real_tag]->reference_counter;
        if (ref_counter == -1) {
            AUDIT
            printk("%s: tag is about to be eliminated, cannot listen", MODNAME);
            return -EBUSY;
        }
    } while (!__sync_bool_compare_and_swap(&(tag_array[real_tag]->reference_counter), ref_counter, ref_counter + 1));

    //epoch read, an epoch has been chosen by the reader, an epoch cannot be replaced without a write so he will eventually read something
    my_level = tag_array[real_tag]->priority_levels[level];

    __sync_fetch_and_add(&(my_level->reference_counter), 1);

    AUDIT
    printk("%s: reference updated\n", MODNAME);
        
    // wait for write
    if (wait_event_interruptible(*tag_wqs[real_tag], my_level->buffer != NULL) == 0) {
        if (my_level->size != -1) {
            ret = size < my_level->size ? size : my_level->size;
            copy_to_user(buffer, my_level->buffer, ret);
        } else
            ret = 0;
    } else {
        ret = -ERESTARTSYS;
    }

    //if we are the last to read realease the epoch
    if (__sync_fetch_and_sub(&(my_level->reference_counter), 1) == 1 && my_level->buffer != NULL && my_level->size != -1) {
        AUDIT
        printk("%s: %p, %p", MODNAME, my_level->buffer, my_level);
        kfree(my_level->buffer);
        kfree(my_level);
    }

    __sync_fetch_and_sub(&(tag_array[real_tag]->reference_counter), 1);
    
    AUDIT
    printk("%s: message received\n", MODNAME);

    return ret;

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
static unsigned long sys_tag_receive = (unsigned long) __x64_sys_tag_receive;	
#else
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _tag_ctl, int, tag, int, command){
#else
asmlinkage int  sys_tag_ctl(int tag, int command) {
#endif

    descriptor_info* my_info;
    unsigned char current_tag;
    int i;

    AUDIT
    printk("%s: called tag_ ctl, pid: %d, parent_pid: %d, real parent: %d, group leader: %d\n",MODNAME, current->pid, current->parent->pid, current->real_parent->pid, current->group_leader->pid);

    
    //check if process has open descriptors
    my_info = find_check_descriptor(&tables, tag);
    if (my_info == NULL)
        return -EINVAL;
    current_tag = my_info->descriptors[tag];

    tag_array[current_tag]->last_use = ktime_get_seconds();

    AUDIT
    printk("%s: check passed in ctl\n", MODNAME);


    if (command == AWAKE_ALL) {
        AUDIT
        printk("%s: awaking everyone on tag %d\n", MODNAME, current_tag);
        //epoch write on every level
        for (i = 0; i < LEVELS_MAX; i++)    
            epoch_write(current_tag, i, NULL, -1);
        return 0;
    }
    
    if (command == REMOVE) {
        set_bit_zero(my_info->descriptor_bitmap, tag);
        return remove_tag(current_tag);
    }

    return -EINVAL;

}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
static unsigned long sys_tag_ctl = (unsigned long) __x64_sys_tag_ctl;	
#else
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _audit, int, param){
#else
asmlinkage int sys_audit(int param) {
#endif

    int i, j;
    // print tables
    td_hash_table* temp = &tables;
    while (temp != NULL) {
        if (temp->table != NULL) {
            for (i = 0; i < HASH_TABLE_SIZE; i++) {
                printk("%s: pid %d", MODNAME, i);
                if (temp->table[i] != NULL) {
                    if (temp->table[i]->descriptors != NULL) {
                        print_bitmap(temp->table[i]->descriptor_bitmap, temp->table[i]->curr_alloc);
                        for (j = 0; j < temp->table[i]->curr_alloc; j++)
                            printk("%s: desc %d, real index %d\n", MODNAME, j, temp->table[i]->descriptors[j]);
                    }
                }
            }
        }
        temp = temp->next;
    }

    //print tags
    for (i = 0; i < TAGS_MAX; i++) {
        if (tag_array[i] != NULL) {
            printk("%s: tag %d, key %d", MODNAME, i, key_array[i]);
            //print levels
            for (j = 0; j < LEVELS_MAX; j++){
                    printk("%s: level %d, pointer: %p\n", MODNAME, j, tag_array[i]->priority_levels + j);
                if (tag_array[i]->priority_levels[j] != NULL)
                    printk("%s: level %d, number of threads in wait: %d\n", MODNAME, j, tag_array[i]->priority_levels[j]->reference_counter);

            }
        }

    }

    //printk("%s:\nbitmap: %d\n", MODNAME);

    return 42;

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
static unsigned long sys_audit = (unsigned long) __x64_sys_audit;   
#else
#endif


// ########## helpers ########################

unsigned long cr0;

static inline void
write_cr0_forced(unsigned long val)
{
    unsigned long __force_order;

    /* __asm__ __volatile__( */
    asm volatile(
        "mov %0, %%cr0"
        : "+r"(val), "+m"(__force_order));
}

static inline void
protect_memory(void)
{
    write_cr0_forced(cr0);
}

static inline void
unprotect_memory(void)
{
    write_cr0_forced(cr0 & ~X86_CR0_WP);
}

// ########## init and clean up ##############

int init_module(void) {

    int i = 0;

    AUDIT
    printk(KERN_DEBUG "%s: initializing\n",MODNAME);
	
    // Add syscalls
	syscall_table_finder();

	if(!hacked_syscall_tbl){
		printk("%s: failed to find the sys_call_table\n",MODNAME);
		return -1;
	}

	cr0 = read_cr0();
    unprotect_memory();
    hacked_syscall_tbl[FIRST_NI_SYSCALL] = (unsigned long*) sys_tag_get;
    hacked_syscall_tbl[SECOND_NI_SYSCALL] = (unsigned long*) sys_tag_send;
    hacked_syscall_tbl[THIRD_NI_SYSCALL] = (unsigned long*) sys_tag_receive;
    hacked_syscall_tbl[FOURTH_NI_SYSCALL] = (unsigned long*) sys_tag_ctl;
    hacked_syscall_tbl[FIFTH_NI_SYSCALL] = (unsigned long*) sys_audit;
    protect_memory();
    
    AUDIT
	printk("%s: tag_get installed on the sys_call_table at displacement %d\n",MODNAME,FIRST_NI_SYSCALL);	
    AUDIT
    printk("%s: tag_send installed on the sys_call_table at displacement %d\n",MODNAME,SECOND_NI_SYSCALL);   
    AUDIT
    printk("%s: tag_receive installed on the sys_call_table at displacement %d\n",MODNAME,THIRD_NI_SYSCALL);   
    AUDIT
    printk("%s: tag_ctl installed on the sys_call_table at displacement %d\n",MODNAME,FOURTH_NI_SYSCALL);   

    // Allocate first hash desciptor table
    tables.table = kmalloc(HASH_TABLE_SIZE*sizeof(descriptor_info*), GFP_KERNEL);

    for (i = 0; i < HASH_TABLE_SIZE; i++)
        tables.table[i] = NULL;

    tables.next = NULL;

    AUDIT
    printk("%s: tables allocated", MODNAME);

    for (i = 0; i < TAGS_MAX; i++) {
        tag_wqs[i] = kmalloc(sizeof(wait_queue_head_t), GFP_KERNEL);
        init_waitqueue_head(tag_wqs[i]);
    }

    if (alloc_chrdev_region(&device_major , 0, 1, DEVICE_NAME) < 0) {
        AUDIT
        printk("%s: region registering of device failed\n", MODNAME);
        return -1;
    }

    device_class = class_create(THIS_MODULE, "my_driver_class");

    cdev_init(&(dev_data.cdev), &fops);
    device_create(device_class, NULL, device_major, NULL, "tag_service");
    if (cdev_add(&(dev_data.cdev), device_major, 1)){
        AUDIT
        printk("%s: add registering of device failed\n", MODNAME);
        return -1;
    }

    AUDIT
    printk("%s: device registered, it is assigned major number %d\n", MODNAME, MAJOR(device_major));
    
    AUDIT
    printk("%s: module correctly mounted\n",MODNAME);

	return 0;

}

void cleanup_module(void) {
    
    AUDIT
    printk("%s: starting cleanup\n", MODNAME);

    device_destroy(device_class, device_major);
    class_destroy(device_class);
    cdev_del(&(dev_data.cdev));
    unregister_chrdev_region(device_major, 1);

    printk("%s: device unregistered, it was assigned major number %d\n", MODNAME, device_major);

    //syscall_table_finder();

    if(!hacked_syscall_tbl){
        printk("%s: failed to find the sys_call_table\n",MODNAME);
        return;
    }

    cr0 = read_cr0();
    unprotect_memory();
    hacked_syscall_tbl[FIRST_NI_SYSCALL] = (unsigned long*) hacked_syscall_tbl[SEVENTH_NI_SYSCALL];
    hacked_syscall_tbl[SECOND_NI_SYSCALL] = (unsigned long*) hacked_syscall_tbl[SEVENTH_NI_SYSCALL];
    hacked_syscall_tbl[THIRD_NI_SYSCALL] = (unsigned long*) hacked_syscall_tbl[SEVENTH_NI_SYSCALL];
    hacked_syscall_tbl[FOURTH_NI_SYSCALL] = (unsigned long*) hacked_syscall_tbl[SEVENTH_NI_SYSCALL];
    hacked_syscall_tbl[FIFTH_NI_SYSCALL] = (unsigned long*) hacked_syscall_tbl[SEVENTH_NI_SYSCALL];
    protect_memory();
    
    AUDIT
    printk("%s: syscalls correctly unistalled\n",MODNAME);    
    
    AUDIT
    printk("%s: shutting down\n",MODNAME);
        
}
