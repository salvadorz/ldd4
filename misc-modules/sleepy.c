/*
 * sleepy.c -- the writers awake the readers
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>  /* current and everything */
#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */
#include <linux/types.h>  /* size_t */
#include <linux/cdev.h>
#include <linux/wait.h>

MODULE_LICENSE("Dual BSD/GPL");

static int sleepy_major = 0;
static int sleepy_minor = 0;
struct cdev *sleepy_cdev;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;

static ssize_t sleepy_read(struct file *filp, char __user *buf, size_t count,
		loff_t *pos)
{
  pr_debug("process %i (%s) going to sleep\n", current->pid,
           current->comm);
  wait_event_interruptible(wq, flag != 0);
  flag = 0;
  pr_debug("awoken %i (%s)\n", current->pid, current->comm);
  return 0; /* EOF */
}

static ssize_t sleepy_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	printk(KERN_DEBUG "process %i (%s) awakening the readers...\n",
			current->pid, current->comm);
	flag = 1;
	wake_up_interruptible(&wq);
	return count; /* Succeed, to avoid retrial. */
}

struct file_operations sleepy_fops = {
	.owner = THIS_MODULE,
	.read =  sleepy_read,
	.write = sleepy_write,
};

static int __init sleepy_init(void)
{
	int result;
	dev_t dev;

	/* Register dynamically. */
	result = alloc_chrdev_region(&dev, sleepy_minor, 1, "sleepy");

	if (result < 0) {
		printk(KERN_WARNING "sleepy: couldn't get major number assignment\n");
		return result;
	}

	sleepy_major = MAJOR(dev);

	sleepy_cdev = cdev_alloc();

	if (!sleepy_cdev) {
		printk(KERN_WARNING "Failed to allocate cdev\n");
		return -ENOMEM;
	}

    cdev_init(sleepy_cdev, &sleepy_fops);
    sleepy_cdev->owner = THIS_MODULE;
	sleepy_cdev->ops = &sleepy_fops;
	cdev_add(sleepy_cdev, dev, 1);

	return 0;
}

static void __exit sleepy_cleanup(void)
{
	dev_t dev = MKDEV(sleepy_major, sleepy_minor);
	cdev_del(sleepy_cdev);
	unregister_chrdev_region(dev, 1);
}

module_init(sleepy_init);
module_exit(sleepy_cleanup);
