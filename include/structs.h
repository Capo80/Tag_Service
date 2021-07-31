#ifndef STRUCTS_H
#define STRUCTS_H

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/wait.h>
#include <linux/pid.h>
#include <linux/uaccess.h>
#include "../include/bitmap.h"
#include "../include/parameters.h"

struct key_node{

    int key;
    bool complete;
    short ref_counter;
    struct key_node* next;

};

/*
    The scruct tag contains information for the tag
    
    @version: version of the tag, incremented at every open
    @private: 1 if it cannot be re-opened, 0 if it can
    @owner: uid of the owner (in case of private tag), INT_MAX if it does not have one
    @priority_levels: struct that keeps information on the levels
    @bitmaps: bitmaps of the thread that are waiting for a message, one per level
    @reference_counter: number of threads on wait on the whole tag, used to prevent removals

    The struct level contains information for the level of a specific tag
    This strcture is swapped on epochs change

    @reference_counter: numeber of threads on wait on this level, used for writes
    @buffer: buffer to use when a message is passed
*/
typedef struct {

    unsigned short reference_counter;
    char* buffer;
    size_t size;

} prio_level;
typedef struct {

    char private;
    short reference_counter;
    kuid_t owner;
    prio_level* priority_levels[LEVELS_MAX];
    ktime_t last_use;
    
} tag;

/*
    hash table that keeps tracks of file descriptos.
    The hash table maps pids to an array of file descriptor.
    The table operates with key of max 512 pids, in case of collision a new table is allocated, every table is contained in 1 pages of 4Kb.
    @descriptos: array of file descriptors, it uses the td of the thread as index and contains the real tag index in the least significant byte.
    @descriptor_bitmap: indicates which descriptor are valid in in the array
    @real_pid: contains the pid of the thread associated with the structures used to resolve collisions 
    @curr_alloc: the number of descriptors that can be allocated wothuot making the structure bigger
    @version: version of the tag, used to detect when a tag is closed
*/
typedef struct {

    short* versions;
    bitmap descriptor_bitmap;
    int curr_alloc;
    pid_t real_pid;
    unsigned char* descriptors;

} descriptor_info;
typedef struct td_hash_table{

    descriptor_info** table;
    struct td_hash_table* next;

} td_hash_table;

descriptor_info* allocate_new_descriptor(pid_t real_pid, unsigned char real_index, short version, int batch);
tag* allocate_new_tag(char private, int owner);
descriptor_info* get_descriptor_info(td_hash_table* table, int pid);
int get_new_descriptor(td_hash_table* td_table, unsigned char real_index, short version);
void free_desc_info(descriptor_info* info);
void print_list(struct key_node* list);
void remove_key(struct key_node** list, int value);
void insert_key(struct key_node** list, int value);
struct key_node* allocate_key(int value);
struct key_node* find_key(struct key_node* list, int value);
int insert_info(td_hash_table* td_table, int pid, descriptor_info* info);
int epoch_write(int real_tag, int level, char* buffer, size_t size);
int remove_tag(int current_tag);
descriptor_info* find_check_descriptor(td_hash_table* tables, int tag);

//array of tags and versions (defined in device header);
extern tag* tag_array[TAGS_MAX];
extern short tag_versions[TAGS_MAX];
extern wait_queue_head_t* tag_wqs[TAGS_MAX];

// tables of descriptors
extern td_hash_table tables;

//array that associates tag number with the key
//INT_MAX means that tag is free, this number cannot be used as a key
extern int key_array[TAGS_MAX];
extern struct key_node* insert_list;


#endif