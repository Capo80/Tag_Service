#ifndef DEVICE_H
#define DEVICE_H

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include "../include/parameters.h"
#include "../include/structs.h"
#include "../include/rwlock.h"

#define FILE_SIZE 250000

int dev_open(struct inode *inode, struct file *file);
ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off);
int dev_release(struct inode *inode, struct file *file);

struct tag_device_data {
    struct cdev cdev;
};

extern struct file_operations fops;
extern struct tag_device_data dev_data;

#endif