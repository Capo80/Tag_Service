#include "../include/structs.h"


//array of tags and versions (defined in device header);
tag* tag_array[TAGS_MAX];
short tag_versions[TAGS_MAX];
wait_queue_head_t* tag_wqs[TAGS_MAX];

// tables of descriptors
td_hash_table tables;
DEFINE_MUTEX(table_mutex);

//array that associates tag number with the key
//INT_MAX means that tag is free, this number cannot be used as a key
int key_array[TAGS_MAX] = { [0 ... TAGS_MAX-1 ] = INT_MAX  };
struct key_node* insert_list = NULL;

// ####### descriptors #############

//get descriptor info of process in table, pointer is NULL if not present
descriptor_info* get_descriptor_info(td_hash_table* table, int pid) {

    int table_index = pid % HASH_TABLE_SIZE;
    td_hash_table* temp = table;

    AUDIT
    printk("%s: aaa searching for %d, table index: %d\n", MODNAME, pid, table_index);

    while (temp != NULL) {
        if (temp->table != NULL && temp->table[table_index] != NULL && temp->table[table_index]->real_pid == pid)
            return temp->table[table_index];
        temp = temp->next;
    }

    return NULL;

}

int insert_info(td_hash_table* td_table, int pid, descriptor_info* info) {

    int i; 

    //search for space in existing tables
    while (true) {

        if (td_table->table[pid%HASH_TABLE_SIZE] == NULL) {
            td_table->table[pid%HASH_TABLE_SIZE] = info;
            return 1;
        } else if (td_table->table[pid%HASH_TABLE_SIZE]->real_pid == pid)
            return -1;

        if (td_table->next == NULL)
            break;

        td_table = td_table->next;
    }

    mutex_lock_interruptible(&table_mutex);
    if (td_table->next == NULL) {
        // allocate new table
        td_table->next = kmalloc(sizeof(td_hash_table), GFP_KERNEL);
        td_table->next->table = kmalloc(HASH_TABLE_SIZE*sizeof(descriptor_info*), GFP_KERNEL);

        for (i = 0; i < HASH_TABLE_SIZE; i++)
            td_table->next->table[i] = NULL;

        td_table->next->next = NULL;
    }
    mutex_unlock(&table_mutex);
    
    td_table->next->table[pid%HASH_TABLE_SIZE] = info;

    return 1;

}
descriptor_info* allocate_new_descriptor(pid_t real_pid, unsigned char real_index, short version, int batch) {

    // allocate new structure and return descriptor 0
    descriptor_info* new_info = kmalloc(sizeof(descriptor_info), GFP_KERNEL);

    new_info->real_pid = real_pid;
    new_info->descriptors = kmalloc(sizeof(char)*batch, GFP_KERNEL);
    new_info->descriptor_bitmap = get_new_bitmap(batch);
    new_info->versions = kmalloc(sizeof(short)*batch, GFP_KERNEL);
    new_info->descriptors[0] = real_index;
    set_bit_one(new_info->descriptor_bitmap, 0);
    new_info->versions[0] = version;
    new_info->curr_alloc = batch;

    return new_info;

}

void free_desc_info(descriptor_info* info) {
    if (info != NULL)    
        kfree(info);
}

int add_new_descriptor(descriptor_info* info, unsigned char real_index, short version) {

    // search for free descriptor
    int i = 0; 
    while (i < info->curr_alloc && check_bit(info->descriptor_bitmap, i))
        i++;
    
    // array of descriptors is full, make it bigger
    if (i == info->curr_alloc) {

        //TODO function for this
        char* new_array = kmalloc(info->curr_alloc+DESCRIPTOR_BATCH, GFP_KERNEL);
        short* new_version_array = kmalloc(sizeof(short)*(info->curr_alloc+DESCRIPTOR_BATCH), GFP_KERNEL);
        memcpy(new_array, info->descriptors, info->curr_alloc);
        memcpy(new_version_array, info->versions, info->curr_alloc*sizeof(short*));
        kfree(info->descriptors);
        kfree(info->versions);
        info->descriptors = new_array;
        info->versions = new_version_array;
        info->curr_alloc = info->curr_alloc + DESCRIPTOR_BATCH;

        AUDIT
        printk("%s: descriptor table expanded", MODNAME);
    }

    // add new descriptor
    info->descriptors[i] = real_index;
    set_bit_one(info->descriptor_bitmap, i);
    info->versions[i] = version;

    AUDIT
    printk("%s: got descriptor %d", MODNAME, i);
    return i;

}


int get_new_descriptor(td_hash_table* td_table, unsigned char real_index, short version) {

    int i, ret;
    pid_t current_pid = current->pid;
    descriptor_info* my_info;
    td_hash_table* start = td_table;

    AUDIT
    printk("%s: got pid %d, position %d\n", MODNAME, current_pid, ((int)current_pid) % HASH_TABLE_SIZE);        

    //this will always exit, at the worst case we are allocating a new table
    
    while(true) {
        my_info = td_table->table[((int)current_pid) % HASH_TABLE_SIZE];

        //clean up dead threads descriptors on the way
        if (my_info != NULL && (check_all_zero(my_info->descriptor_bitmap) || pid_task(find_vpid(my_info->real_pid),PIDTYPE_PID) == NULL)) {
            free_desc_info(my_info);
            td_table->table[((int)current_pid) % HASH_TABLE_SIZE] = NULL;            
        }
        
        if (my_info == NULL) {

            // check if we need to inherit from father, if not we allocate new strcture
            descriptor_info* parent_info = get_descriptor_info(start, current->group_leader->pid);

            AUDIT
            printk("%s: spot is free, allocating\n", MODNAME);        

            if (parent_info != NULL) {
                my_info = kmalloc(sizeof(descriptor_info), GFP_KERNEL);
                memcpy(my_info, parent_info, sizeof(descriptor_info));
                ret = add_new_descriptor(my_info, real_index, version);
            } else {
                my_info = allocate_new_descriptor(current_pid, real_index, version, DESCRIPTOR_BATCH);
                ret = 0;
            }

            td_table->table[((int)current_pid) %HASH_TABLE_SIZE] = my_info;

            AUDIT
            printk("%s: allocated new pid\n", MODNAME);  

            return ret;

        } else {

            AUDIT
            printk("%s: spot not free\n", MODNAME);        

            if (my_info->real_pid == current_pid)
                return add_new_descriptor(my_info, real_index, version);

        }

        AUDIT
        printk("%s: looking at next table\n", MODNAME);
        
        // collision look at next table
        if (td_table->next == NULL) {

            mutex_lock_interruptible(&table_mutex);
            if (td_table->next == NULL) {
                // allocate new table
                td_table->next = kmalloc(sizeof(td_hash_table), GFP_KERNEL);
                td_table->next->table = kmalloc(HASH_TABLE_SIZE*sizeof(descriptor_info*), GFP_KERNEL);

                for (i = 0; i < HASH_TABLE_SIZE; i++)
                    td_table->next->table[i] = NULL;

                td_table->next->next = NULL;
            }
            mutex_unlock(&table_mutex);
        }

        td_table = td_table->next;
    }

}

//finds descriptor info of current thread and returns only if tag is valid 
descriptor_info* find_check_descriptor(td_hash_table* tables, int tag) {

    int real_tag;
    //check if process has open descriptors
    descriptor_info* my_info = get_descriptor_info(tables, current->pid);
    AUDIT
    printk("%s: search complete\n", MODNAME);
    if (my_info == NULL) {
        // check if we can inherit from father, if not we fail
        descriptor_info* parent_info = get_descriptor_info(tables, current->group_leader->pid);
        if (parent_info != NULL) {
            AUDIT
            printk("%s: inheriting from parent", MODNAME);
            my_info = kmalloc(sizeof(descriptor_info), GFP_KERNEL);
            memcpy(my_info, parent_info, sizeof(descriptor_info));
            insert_info(tables, current->pid, my_info);
        } else {
            AUDIT
            printk("%s: process has no open descriptors\n", MODNAME);
            return NULL;
        }
    }

    //check that descriptor is valid
    if (tag >= my_info->curr_alloc || !check_bit(my_info->descriptor_bitmap, tag)) {
        AUDIT
        printk("%s: descriptor is closed\n", MODNAME);
        return NULL;
    }

    real_tag = my_info->descriptors[tag];
    //check that tag is valid
    if (tag_array[real_tag] == NULL) {
        AUDIT
        printk("%s: tag is closed\n", MODNAME);
        return NULL;
    }

    //check version
    if (tag_versions[real_tag] != my_info->versions[tag]) {
        AUDIT
        printk("%s: version is not correct\n", MODNAME);
        set_bit_zero(my_info->descriptor_bitmap, tag);
        return NULL;
    }

    return my_info;

}

// ########### tags ######################

tag* allocate_new_tag(char private, int permission) {

    int i;    
    tag* new_tag = kmalloc(sizeof(tag), GFP_KERNEL);
    
    new_tag->private = private;
    if (permission == 1)    
        new_tag->owner = current->cred->uid;
    else {    
        kuid_t no_own;
        no_own.val = (uid_t) INT_MAX;
        new_tag->owner = no_own;
    }
    new_tag->reference_counter = 0;
    new_tag->last_use = ktime_get_seconds();

    for(i = 0; i < LEVELS_MAX; i++) {
        new_tag->priority_levels[i] = kmalloc(sizeof(prio_level), GFP_KERNEL);
        new_tag->priority_levels[i]->reference_counter = 0;
        new_tag->priority_levels[i]->buffer = 0;
    }

    return new_tag;
}

//removes tag from tag array
int remove_tag(int current_tag) {

    AUDIT
    printk("%s: checking if threads are listening\n", MODNAME);
    
    if  (!__sync_bool_compare_and_swap(&(tag_array[current_tag]->reference_counter), 0, -1)) {
        AUDIT
        printk("%s: threads are listening, cannot remove\n", MODNAME);
        return -EBUSY;
    }

    AUDIT
    printk("%s: starting close\n", MODNAME);

    //free tag
    tag_versions[current_tag]++;

    kfree(tag_array[current_tag]);
    tag_array[current_tag] = NULL;

    key_array[current_tag] = INT_MAX;

    AUDIT
    printk("%s: tag removed\n", MODNAME);

    return 0;
}

// write on level swapping buffers
int epoch_write(int real_tag, int level, char* buffer, size_t size) {

    prio_level* my_level;
    prio_level* new_epoch = NULL;

    //check if someone is listening on the level
    do {
        my_level = tag_array[real_tag]->priority_levels[level];

        if (my_level->reference_counter == 0) {
            AUDIT
            printk("%s: no one is listening\n", MODNAME);
            //we have allocated a new epoch but someone was faster than us, free it
            if (new_epoch != NULL)
                kfree(new_epoch);
            return 0;
        }

        //someone is, start epoch write

        //allocate new epoch
        if (new_epoch == NULL)
            new_epoch = kmalloc(sizeof(prio_level), GFP_KERNEL);

        //try to start new epoch, someone might have already done so, if so restart    
    } while (!__sync_bool_compare_and_swap((tag_array[real_tag]->priority_levels) + level, my_level, new_epoch));


    //here we are the only one operating on the level, new writes and reads will happen on new epoch
    if (size != -1) {
        my_level->buffer = kmalloc(size, GFP_KERNEL);
        my_level->size = size;
        copy_from_user(my_level->buffer, buffer, size);
    } else {
        my_level->buffer = (char*) -1;
        my_level->size = -1;
    }

    wake_up_interruptible(tag_wqs[real_tag]);

    AUDIT
    printk("%s: message sent\n", MODNAME);

    return size;

}


// ######## key list ###########

struct key_node* allocate_key(int value) {

    struct key_node* new_key = kmalloc(sizeof(struct key_node), GFP_KERNEL);
    new_key->key = value;
    new_key->ref_counter = 0;
    new_key->complete = false;
    new_key->next = NULL;

    return new_key;

}

// insert on head - new keys are most likely to be searched for
void insert_key(struct key_node** list, int value) {

    struct key_node* new_key = allocate_key(value);

    new_key->next = *list;
    *list = new_key;

}

// serach for key return null if not present
struct key_node* find_key(struct key_node* list, int value) {

    while (list != NULL && list->key != value)
        list = list->next;

    return list;
}

void remove_key(struct key_node** list, int value) {

    struct key_node* temp = *list;
    struct key_node* temp2;

    if (*list == NULL)
        return;

    if ((*list)->key == value) {
        *list = (*list)->next;
        kfree(temp);
        return;
    }

    while (temp->next != NULL) {

        printk("%d\n", temp->key);
        printk("n: %d\n", temp->next->key);
        
        if (temp->next->key == value) {
            temp2 = temp->next;
            temp->next = temp->next->next;
            kfree(temp2);
        }

        temp = temp->next;
    }
}

void print_list(struct key_node* list) {

    printk("lista:\n");
    while(list != NULL) {
        printk("%d\n", list->key);
        list = list->next;
    }

}