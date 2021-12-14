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
        struct mutex lock;                 /* mutual exclusion mutex */
        struct cdev cdev;                  /* Char device structure */
} ;
static struct char_pipe *char_p_device = NULL; // A single char_pipe device

#define CHAR_PIPE "char_pipe"
static int char_p_dev_num; // device number
//static int char_p_major_num;
//static int char_p_minor_num = 1;	
static int char_p_nr_devs = 1;	// number of devices
static int char_p_buffer = 4000;

// performing blocking on open
static wait_queue_head_t open_wait_queue;
static int wait_flag = 0;


static int char_p_fasync(int fd, struct file *filp, int mode)
{
	struct char_pipe *dev = filp->private_data;
	DBG("pid=%d (%s) mode=%d", current->pid, current->comm, mode);
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int char_p_open(struct inode *inode, struct file *filep) // filep represents an open file, differs between different open processes
{
	struct char_pipe *dev;
	dev = container_of(inode->i_cdev, struct char_pipe, cdev); // has the same address pointed to the device structure
	filep->private_data = dev;  
	DBG("process opened , pid=%d (%s) , faddr=%p, inodeaddr=%p",
		current->pid, current->comm, filep, inode);

	if (mutex_lock_interruptible(&dev->lock))
	{
	    return -ERESTARTSYS;
	}

	if (!dev->buffer) 
	{
	   /* allocate the buffer */
	   DBG("allocate device buffer pid=%d (%s)",current->pid, current->comm);
	   dev->buffer = kmalloc(char_p_buffer, GFP_KERNEL);
	   if (!dev->buffer) 
	   {
	      mutex_unlock(&dev->lock);
	      return -ENOMEM;
	   }			
	}
	dev->buffersize = char_p_buffer;
	dev->end = dev->buffer + dev->buffersize;
	dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */
	
	/* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
	if (filep->f_mode & FMODE_READ)
	{
	    dev->nreaders++;
	    if(dev->nwriters <= 0)
	    {
	    	mutex_unlock(&dev->lock);
	    	if(filep->f_flags & O_NONBLOCK) // if non-block mode, return 
	    	{
	    	    dev->nreaders--;
	    	    return -ERESTARTSYS;	
	    	}
	    	DBG("reader blocks, pid=%d (%s)",current->pid, current->comm);
	    	if(wait_event_interruptible(open_wait_queue, wait_flag == 1))
	    	{
	   	    return -ERESTARTSYS;
	    	}
	    	if (mutex_lock_interruptible(&dev->lock))
	        {
	           return -ERESTARTSYS;
	        }
	    }
	    DBG("reader opens, pid=%d (%s)",current->pid, current->comm);
	    
	}
	if (filep->f_mode & FMODE_WRITE)
	{
	    DBG("writter opens, pid=%d (%s)",current->pid, current->comm);
	    dev->nwriters++;
	    wait_flag = 1;
	    wake_up_interruptible(&open_wait_queue); // wakes up all the reading process
	}

	mutex_unlock(&dev->lock);
	return nonseekable_open(inode, filep);
}

static int char_p_release(struct inode *inode, struct file *filp)
{
	struct char_pipe *dev = filp->private_data;
	/* remove this filp from the asynchronously notified filp's */
	char_p_fasync(-1, filp, 0);
	mutex_lock(&dev->lock);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
	if (dev->nreaders + dev->nwriters == 0) 
	{
	   DBG("free buffer, pid=%d (%s)",current->pid, current->comm);
	   kfree(dev->buffer);
	   dev->buffer = NULL; /* the other fields are not checked on open */
	}
	if(dev->nwriters == 0 && (dev->rp == dev->wp)) // if there are no writers and no data, set wait_flag to 0
        {
            wait_flag = 0;
        }
	mutex_unlock(&dev->lock);
	return 0;
}

/*
 * Data management: read and write
 */

static ssize_t char_p_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct char_pipe *dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	while (dev->rp == dev->wp) /* nothing to read */
	{ 
		mutex_unlock(&dev->lock); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
		{
		    DBG("pid (%d,\"%s\") reading: nonblock mode, try again\n", current->pid, current->comm);
		    return -EAGAIN;
		}
		DBG("pid (%d,\"%s\") reading: going to sleep\n", current->pid, current->comm);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	if (dev->wp > dev->rp)
		count = min(count, (size_t)(dev->wp - dev->rp));
	else /* the write pointer has wrapped, return data up to dev->end */
		count = min(count, (size_t)(dev->end - dev->rp));
	if (copy_to_user(buf, dev->rp, count)) 
	{
		mutex_unlock (&dev->lock);
		return -EFAULT;
	}
	dev->rp += count;
	if (dev->rp == dev->end)
		dev->rp = dev->buffer; /* wrapped */
	mutex_unlock (&dev->lock);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	DBG("pid (%d,\"%s\") did read %li bytes\n",current->pid, current->comm, (long)count);
	return count;
}


/* How much space is free? */
static int spacefree(struct char_pipe *dev)
{
	if (dev->rp == dev->wp)
		return dev->buffersize - 1;
	return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int char_getwritespace(struct char_pipe *dev, struct file *filp)
{
	while (spacefree(dev) == 0) /* full */
	{ 
		DEFINE_WAIT(wait);
		
		mutex_unlock(&dev->lock);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		DBG("pid (%d,\"%s\") writing: going to sleep\n",current->pid, current->comm);
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (spacefree(dev) == 0)
			schedule();
		finish_wait(&dev->outq, &wait);
		DBG("pid (%d,\"%s\") writing: wake up\n",current->pid, current->comm);
		if (signal_pending(current))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	}	
	return 0;
}	


static ssize_t char_p_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct char_pipe *dev = filp->private_data;
	int result;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	/* Make sure there's space to write */
	result = char_getwritespace(dev, filp);
	if (result)
		return result; /* scull_getwritespace called up(&dev->sem) */

	/* ok, space is there, accept something */
	count = min(count, (size_t)spacefree(dev));
	if (dev->wp >= dev->rp)
		count = min(count, (size_t)(dev->end - dev->wp)); /* to end-of-buf */
	else /* the write pointer has wrapped, fill up to rp-1 */
		count = min(count, (size_t)(dev->rp - dev->wp - 1));
	DBG("Going to accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
	if (copy_from_user(dev->wp, buf, count)) {
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}
	dev->wp += count;
	if (dev->wp == dev->end)
		dev->wp = dev->buffer; /* wrapped */
	mutex_unlock(&dev->lock);

	/* finally, awake any reader */
	wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */

	/* and signal asynchronous readers, explained late in chapter 5 */
	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	DBG("\"%s\" did write %li bytes\n",current->comm, (long)count);
	return count;
}


static unsigned int scull_p_poll(struct file *filp, poll_table *wait)
{
	//DBG("pid (%d,\"%s\") polling \n",current->pid, current->comm);
	struct char_pipe *dev = filp->private_data;
	unsigned int mask = 0;
	/*
	 * The buffer is circular; it is considered full
	 * if "wp" is right behind "rp" and empty if the
	 * two are equal.
	 */
	mutex_lock(&dev->lock);
	poll_wait(filp, &dev->inq,  wait);
	poll_wait(filp, &dev->outq, wait);
	if (dev->rp != dev->wp)
	{
		mask |= POLLIN | POLLRDNORM;	/* readable */
		DBG("pid (%d,\"%s\") polling readable \n",current->pid, current->comm);
	}
	if (spacefree(dev))
	{
		mask |= POLLOUT | POLLWRNORM;	/* writable */
		DBG("pid (%d,\"%s\") polling writable \n",current->pid, current->comm);
	}
	mutex_unlock(&dev->lock);
	return mask;
}

struct file_operations char_pipe_fops = 
{
	.owner =	THIS_MODULE,
	.read =		char_p_read,
	.write =	char_p_write,
	.open =		char_p_open,
	.release =	char_p_release,
	.unlocked_ioctl = ioctl_test,
	.fasync =	char_p_fasync,
	.poll =		scull_p_poll,
	.llseek =	no_llseek,   // disable llseek
};

/*
 * Set up a cdev entry.
 */
static void char_p_setup_cdev(struct char_pipe *dev, int index)
{
	int err, devno = char_p_dev_num + index;
    
	cdev_init(&dev->cdev, &char_pipe_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
	  printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
}

 

/*
 * Initialize the pipe devs; return how many we did.
 */
int char_p_init(dev_t dev)
{
	int i = 0, result = 0;

	result = register_chrdev_region(dev, char_p_nr_devs, CHAR_PIPE);
	if (result < 0) 
	{
		DBG("Unable to get char pipe region, error %d", result);
		return result;
	}
	char_p_dev_num = dev;
	DBG("Load Char pipe device success");
	DBG("Major number=%d", MAJOR(dev));
	
	char_p_device = kmalloc(char_p_nr_devs * sizeof(struct char_pipe), GFP_KERNEL);
	if (char_p_device == NULL) 
	{
		unregister_chrdev_region(char_p_dev_num, char_p_nr_devs);
		return 0;
	}
	memset(char_p_device, 0, char_p_nr_devs * sizeof(struct char_pipe));
	//for (i = 0; i < char_p_nr_devs; i++) 
	//{	
		init_waitqueue_head(&(char_p_device->inq));
		init_waitqueue_head(&(char_p_device->outq));
		mutex_init(&char_p_device->lock);
		char_p_setup_cdev(char_p_device, i);
	//}

	init_waitqueue_head(&open_wait_queue);

	return result;
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
