#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>	/* printk(), min() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>

#include "char_device.h"

struct char_pipe 
{
        wait_queue_head_t inq, outq;       /* read and write queues */
        char *buffer, *end;                /* begin of buf, end of buf */
        int buffersize;                    /* used in pointer arithmetic */
        char *rp, *wp;                     /* where to read, where to write */
        int nreaders, nwriters;            /* number of openings for r/w */
        struct fasync_struct *async_queue; /* asynchronous readers */
        struct mutex lock;              /* mutual exclusion mutex */
        struct cdev cdev;                  /* Char device structure */
} *char_p_device ;

#define CHAR_PIPE "char_pipe"
static int char_p_dev_num;
static int char_p_major_num;
static int char_p_minor_num = 1;
static int char_p_devno;
static int char_p_nr_devs = 1;

struct file_operations char_pipe_fops = 
{
	.owner =	THIS_MODULE,
	/*.llseek =	no_llseek,
	.read =		scull_p_read,
	.write =	scull_p_write,
	.poll =		scull_p_poll,
	.unlocked_ioctl = scull_ioctl,
	.open =		scull_p_open,
	.release =	scull_p_release,
	.fasync =	scull_p_fasync,*/
};

/*
 * Set up a cdev entry.
 */
static void char_p_setup_cdev(struct char_pipe *dev, int index)
{
	int err, devno = char_p_devno + index;
    
	cdev_init(&dev->cdev, &char_pipe_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
}

 

/*
 * Initialize the pipe devs; return how many we did.
 */
int char_p_init(dev_t dev)
{
	int i, result;

	result = register_chrdev_region(dev, char_p_nr_devs, CHAR_PIPE);
	if (result < 0) 
	{
		DBG("Unable to get char pipe region, error %d", result);
		return 0;
	}
	//char_p_major_num = MAJOR(char_p_dev_num);
	DBG("Load Char Pipe Device");
	//DBG("Major number=%d", char_p_major_num);
	//DBG("Use mknod /dev/%s c %d 0\" for device file", CHAR_PIPE, char_p_major_num);
	char_p_dev_num = dev;
	char_p_device = kmalloc(char_p_nr_devs * sizeof(struct char_pipe), GFP_KERNEL);
	if (char_p_device == NULL) 
	{
		unregister_chrdev_region(char_p_dev_num, char_p_nr_devs);
		return 0;
	}
	memset(char_p_device, 0, char_p_nr_devs * sizeof(struct char_pipe));
	//for (i = 0; i < scull_p_nr_devs; i++) 
	//{
		init_waitqueue_head(&(char_p_device->inq));
		init_waitqueue_head(&(char_p_device->outq));
		mutex_init(&char_p_device->lock);
		char_p_setup_cdev(char_p_device, 0);
	//}
	return char_p_nr_devs;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void char_p_cleanup(void)
{
	if(char_p_device)
	{
		cdev_del(&char_p_device->cdev);
		kfree(char_p_device->buffer);
		kfree(char_p_device);
		unregister_chrdev_region(char_p_dev_num, char_p_nr_devs);
		char_p_device = NULL; 
	}
}
