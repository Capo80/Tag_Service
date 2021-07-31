#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0xf654425, "module_layout" },
	{ 0x2933c44e, "cdev_del" },
	{ 0xaa279e7c, "kmalloc_caches" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x991e89dc, "cdev_init" },
	{ 0x658dec53, "param_ops_int" },
	{ 0x754d539c, "strlen" },
	{ 0xa9320d27, "ktime_get_seconds" },
	{ 0xe9ace199, "find_vpid" },
	{ 0x77f3e1e5, "device_destroy" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x3c75f900, "pv_ops" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xb44ad4b3, "_copy_to_user" },
	{ 0xfb578fc5, "memset" },
	{ 0x25613621, "current_task" },
	{ 0x2db3d320, "mutex_lock_interruptible" },
	{ 0x977f511b, "__mutex_init" },
	{ 0xc5850110, "printk" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0x633e2cb3, "device_create" },
	{ 0x4634c772, "pid_task" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x86e95978, "cdev_add" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0x1000e51, "schedule" },
	{ 0xb8b9f817, "kmalloc_order_trace" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5362bb62, "kmem_cache_alloc_trace" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x37a0cba, "kfree" },
	{ 0x69acdf38, "memcpy" },
	{ 0x3675766c, "param_array_ops" },
	{ 0x67521b43, "class_destroy" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x656e4a6e, "snprintf" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0x9be9ebca, "param_ops_ulong" },
	{ 0x26ec0919, "__class_create" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "2C2881D2B27BC1E63BF030C");
