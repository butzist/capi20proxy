/*
 * Capi 2.0 Linux Kernel module
 *
 * Written by Matt Wright (at the moment, just by me)
 *
 * This module is released under the GNU Public License.
 * Copyright (c) Matt Wright 2002
 *
 * Changelog:
 * 
 *  14 Feb 2002: Just laid down the base module code. At the moment all
 *  it does is load and unload.
 *
*/

#include <linux/module.h>

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <linux/fs.h>
#include <linux/slab.h>

#include <asm/unistd.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/smp_lock.h>

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) ((a)*65536+(b)*256+(c))
#endif

#define ALPHA_CODE

MODULE_LICENSE("GPL");

static int __init init(void) {

#ifdef ALPHA_CODE
  printk(KERN_INFO "Capi2: Test module installed.\n");
#endif
  return 0;
}

static void __exit cleanup(void) {

#ifdef ALPHA_CODE
  printk(KERN_INFO "Capi2: Test module removed\n");
#endif
  
}

module_init(init);
module_exit(cleanup);
