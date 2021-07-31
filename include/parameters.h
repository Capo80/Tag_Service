#ifndef PARAMS_H
#define PARAMS_H

#define MODNAME "TAG_SERVICE"

// external flags
#define T_PERSONAL  1
#define T_SHARED 0
#define IPC_PUBLIC 1
#ifndef IPC_PRIVATE	
#define IPC_PRIVATE 0
#endif
#define AWAKE_ALL 0
#define REMOVE 1

// module macros
#define DEBUG       1
#define AUDIT       if(DEBUG)
#define TAGS_MAX    256
#define LEVELS_MAX 32
#define HASH_TABLE_SIZE 512
#define INT_MAX     ((int)(~0U >> 1))
#define DESCRIPTOR_BATCH 32
#define LEVELS_MAX 32
#define DEVICE_NAME "tag_device"
#define CLASS_TAG "tag_class"
#define RECYCLE_TIMER 10 //time of inactivity before cleaning up
#define DEVICE_TIMER 2 // update delay, in seconds
#define DEVICE_MEMORY_RELEASE 30 //memmory release delay, in seconds

#endif
