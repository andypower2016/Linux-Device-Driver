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
#include "ioctrl.h"

#define DBG(format, arg...) printk("[%s]:%d => " format "\n",__FUNCTION__,__LINE__,##arg)

struct char_pipe 
{
	  wait_queue_head_t inq, outq;       /* read and write queues */
	  char *buffer; 		     /* begin of buf, circular queue */
	  int buffersize;                    /* size of the buffer */
	  int front;                         /* points to the first index of available data */
	  int rear;                          /* points to the last index of available data */
	  int nreaders, nwriters;            /* number of openings for r/w */
	  struct fasync_struct *async_queue; /* asynchronous readers */
	  struct mutex lock;                 /* mutual exclusion mutex */
	  struct cdev cdev;                  /* Char device structure */
};
static struct char_pipe *char_p_device = NULL; // A single char_pipe device

#define CHAR_PIPE "char_pipe"
dev_t char_p_dev;
static int char_p_major_num;
static int char_p_minor_num = 0;	
static int char_p_nr_devs = 1;	// number of devices
static int char_p_buffer = 5;   // pipe buffer size

// performing blocking on open
static wait_queue_head_t open_wait_queue;
static int wait_flag = 0;

// for circular queue
static int isEmpty(struct char_pipe* dev)
{
	return dev->front == -1 ? 1 : 0;
}
static int isFull(struct char_pipe* dev)
{
	return (dev->front == 0 && dev->rear == dev->buffersize - 1) || ((dev->rear+1) == dev->front);
}

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
	   dev->front = dev->rear = -1;   
	   dev->buffersize = char_p_buffer;
	}
	
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
	if(dev->nwriters == 0 && isEmpty(dev)) // if there are no writers and no data, set wait_flag to 0
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

	while (isEmpty(dev)) 
	{ 
		mutex_unlock(&dev->lock); 
		if (filp->f_flags & O_NONBLOCK)
		{
		    DBG("pid (%d,\"%s\") reading: nonblock mode, try again\n", current->pid, current->comm);
		    return -EAGAIN;
		}
		DBG("pid (%d,\"%s\") reading: going to sleep\n", current->pid, current->comm);
		if (wait_event_interruptible(dev->inq, isEmpty(dev) == 0))
			return -ERESTARTSYS; 
	
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	}
	// 0 1 2 3 4
	if (dev->rear > dev->front) 
	{
		size_t gap = (size_t)(dev->rear - dev->front + 1);
		DBG("pid (%d,\"%s\") reading: (dev->rear %d - dev->front %d + 1)=%ld\n", current->pid, current->comm, dev->rear, dev->front, gap);
		count = min(count, gap);
	}
	else if(dev->rear < dev->front)
	{ 
		size_t gap = (size_t)(dev->buffersize - dev->front);
		DBG("pid (%d,\"%s\") reading: (dev->buffersize - dev->front)=%ld\n", current->pid, current->comm, gap);
		count = min(count, gap);
	}
	else // rear == front (one bye of data left)
	{
	        count = 1;
        }
	
	if (copy_to_user(buf, &dev->buffer[dev->front], count)) 
	{
		mutex_unlock (&dev->lock);
		return -EFAULT;
	}

	if(dev->front == dev->rear) // 1 byte data left
	{
		dev->front = dev->rear = -1;
	}
	else
	{ 
		int res = dev->front + count - 1 ;
		if(res == dev->rear)
		{
		       dev->front = dev->rear = -1;
		}
		else
		{
		       // front points the the next available data, may equal to rear, in this case the queue has only one byte of available data left.
		       dev->front = (res+1) % dev->buffersize;
		}
	}
	mutex_unlock (&dev->lock);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	DBG("pid (%d,\"%s\") did read %li bytes\n",current->pid, current->comm, (long)count);
	return count;
}

/* How much space is free? */
static int spacefree(struct char_pipe *dev)
{
	if (isEmpty(dev))
	   return dev->buffersize;
	else if(isFull(dev))
	   return 0;

	if(dev->rear >= dev->front)
	{
	   return dev->buffersize - dev->rear - 1;
	}
	return dev->front - dev->rear - 1;
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
		return result; 

	count = min(count, (size_t) spacefree(dev));
	if(dev->front == -1)
	{
		dev->front = 0;
	}
	if (copy_from_user(&dev->buffer[dev->rear+1], buf, count)) 
	{
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}
	DBG("Going to write %li bytes to buffer[%d] from %p\n", (long)count, dev->rear+1, buf);
	dev->rear = (dev->rear + count) % dev->buffersize;
	mutex_unlock(&dev->lock);

	/* finally, awake any reader */
	wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */

	/* and signal asynchronous readers, explained late in chapter 5 */
	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	DBG("\"%s\" did write %li bytes\n",current->comm, (long)count);
	return count;
}


static unsigned int char_p_poll(struct file *filp, poll_table *wait)
{
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
	if (isEmpty(dev) == 0)
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

static long int ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	struct char_pipe *dev = file->private_data;
	switch(cmd)
	{
		case WR_VALUE:
			memset(dev->buffer, 0, strlen(dev->buffer));
			if(copy_from_user(dev->buffer, (char *) arg, strlen((char *)arg))) 
			{
				DBG("Error copying data from user!");
			}
			else
			{
				DBG("Update the data to : %s", dev->buffer);
			}
			break;
		case RD_VALUE:		
			if(copy_to_user((char *) arg, dev->buffer, strlen(dev->buffer))) 
				DBG("Error copying data to user!");
			else
				DBG("The data was copied!");
			break;
		default:
			break;
	}
	return 0;
}

struct file_operations char_pipe_fops = 
{
	.owner = THIS_MODULE,
	.read = char_p_read,
	.write = char_p_write,
	.open = char_p_open,
	.release = char_p_release,
	.fasync = char_p_fasync,
	.poll = char_p_poll,
	.llseek = no_llseek,   // disable llseek
	.unlocked_ioctl = ioctl,
};

/*
 * Set up a cdev entry.
 */
static void char_p_setup_cdev(struct char_pipe *dev)
{
	int err, devno = char_p_dev;
    
	cdev_init(&dev->cdev, &char_pipe_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
	  DBG("Error %d adding char_pipe %d", devno);
}

 
/*
 * Initialize the pipe devs; return how many we did.
 */
int char_p_init(dev_t dev)
{
	char_p_device = kmalloc(char_p_nr_devs * sizeof(struct char_pipe), GFP_KERNEL);
	memset(char_p_device, 0, char_p_nr_devs * sizeof(struct char_pipe));
	init_waitqueue_head(&(char_p_device->inq));
	init_waitqueue_head(&(char_p_device->outq));
	mutex_init(&char_p_device->lock);
	char_p_setup_cdev(char_p_device);
	init_waitqueue_head(&open_wait_queue);
	return 0;
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
		char_p_device = NULL; 
	}
	unregister_chrdev_region(char_p_dev, char_p_nr_devs);
}

static __init int char_pipe_init(void)
{
	int ret = alloc_chrdev_region(&char_p_dev, char_p_minor_num, char_p_nr_devs, CHAR_PIPE);
	if(ret < 0)
	{
	   DBG("Fail to allocate major number");
	   return ret;
	}
	char_p_major_num = MAJOR(char_p_dev);
	DBG("char pipe device major number = %d\n", char_p_major_num);
	ret = char_p_init(char_p_dev);
	if(ret < 0)
		return ret;

	return 0;
}

static __exit void char_pipe_exit(void)
{
	char_p_cleanup();
	DBG("Unload Device Complete");
}

module_init(char_pipe_init);
module_exit(char_pipe_exit);
MODULE_LICENSE("Dual BSD/GPL");
