#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>			// file operations structure, allows user to open/read/write/close
#include <linux/cdev.h>			// this is a char driver, makes cdev available
#include <linux/semaphore.h>    // used to access semaphores for syncronization
#include <asm/uaccess.h>		// copy_to_user, copy_from_user (used to map data from user space to kernel space)

#include <linux/slab.h>       // kmalloc/kfree
#include <linux/seq_file.h>   // seq_printf
#include <linux/completion.h> // completion 
#include <linux/spinlock.h>   // spinlock

#include "char_device.h"

int ret;

// completion not used here
//struct completion rw_completion; // device read will wait until the completion of device write
//DECLARE_COMPLETION(completion);	

spinlock_t g_spin_lock;
unsigned long flags;

struct file_operations fileop = 
{
    .owner = THIS_MODULE,
    .open = open,
    .read = read,
    .write = write,
    .release = release,
    .llseek = seek,
};

void free_device_data(struct char_device *dev)
{
	if(dev->data)
	{
		int i = 0;
		for(; i < DATA_SIZE ; ++i)
		{
			if(dev->data[i])
			{
			   kfree(dev->data[i]);
			}
		}
		kfree(dev->data);
	}
}

static void cleanup_device(void)
{
	if(fake_device)
	{
	   cdev_del(&fake_device->mcdev);
	   free_device_data(fake_device);
	   kfree(fake_device);
	}
	unregister_chrdev_region(dev_num, 1);
}

static int setup_device(void)
{
    ret = 0; // 0 for success
	fake_device = kmalloc(sizeof(struct char_device), GFP_KERNEL);
	if(!fake_device)
	{
	   printk(KERN_ALERT "Fail to kalloc device\n");
	   ret = -ENOMEM;
	   cleanup_device();
	   return ret;
	}
	memset(fake_device, 0, sizeof(struct char_device));	

	// init semaphore
	sema_init(&fake_device->sem, 1); // initial value one, means nothing is locked
	
	// init cdev
	cdev_init(&fake_device->mcdev, &fileop);
	fake_device->mcdev.owner = THIS_MODULE;
	ret = cdev_add(&fake_device->mcdev, dev_num, 1);
	if(ret < 0)
	{
	   printk(KERN_ALERT "Fail to add cdev to kernel\n");
	}
	return ret;
}

static __init int chrdev_init(void)
{
	// (1) use dynamic allocation to assign our device
	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if(ret < 0)
	{
	   printk(KERN_ALERT "Fail to allocate major number\n");
	   return ret;
	}
	major_num = MAJOR(dev_num); // extracts the major number and store in variable
	printk(KERN_INFO "Major number=%d\n", major_num);
	printk(KERN_INFO "Use mknod /dev/%s c %d 0\" for device file\n", DEVICE_NAME, major_num);

	// setup device dynamically
	ret = setup_device();

	// init spinlock
	spin_lock_init(&g_spin_lock);
	return ret;
}

static __exit void chrdev_exit(void)
{
	cleanup_device();
	printk(KERN_ALERT "%s Unload Device Complete\n", __FUNCTION__);
}


// Called when device is called by kernel
int open(struct inode *pinode, struct file *pfile)
{ 
	struct char_device *dev = container_of(pinode->i_cdev, struct char_device, mcdev);
	pfile->private_data = dev;
	printk(KERN_INFO "%s opened device ! device datasize=%d\n", __FUNCTION__, dev->size);
	return ret;
}

// Called when user wants to get information from the device
ssize_t read(struct file *pfile, char __user *buffer, size_t length, loff_t *f_pos)
{
	/*if(down_interruptible(&fake_device->sem)) 
	{
	    return -ERESTARTSYS;
	}*/
	//spin_lock(&g_spin_lock);
	spin_lock_irqsave(&g_spin_lock, flags); // disable interrupts before taking the lock

	struct char_device *dev = pfile->private_data;
	printk(KERN_INFO "%s Reading from device\n", __FUNCTION__);
	printk(KERN_INFO "%s fpos=%lld, offset=%lld\n", __FUNCTION__, pfile->f_pos, *f_pos);
	
	int idx_chunk = *f_pos/(DATA_NUM*DATA_SIZE);
	int idx_data  = *f_pos%DATA_SIZE;

	if(!dev->data)
	{
		dev->data = (char**) kmalloc(DATA_NUM * sizeof(char*), GFP_KERNEL);
		memset(dev->data, 0, DATA_NUM * sizeof(char*));
	}

	if(!dev->data[idx_chunk])
	{
		dev->data[idx_chunk] = (char*) kmalloc(DATA_SIZE * sizeof(char), GFP_KERNEL);
		memset(dev->data[idx_chunk], 0, DATA_SIZE * sizeof(char));
	}

	if( (idx_data+length) > DATA_SIZE)
	{
		length = DATA_SIZE-idx_data;
	}

	// Take data from kernel space to user space
	// copy_from_user(destination, source, sizeToTransfer)	
	
	ret = copy_to_user(buffer, dev->data[idx_chunk]+idx_data, length);
	if(ret)
	{
	   ret = -EFAULT;
	   goto end;
	}
	// update file offset 
	*f_pos += length;
end:
	//up(&fake_device->sem);
	//spin_unlock(&g_spin_lock);
	spin_unlock_irqrestore(&g_spin_lock, flags);
	return ret;
}

// Called when user wants to send data to device
ssize_t write(struct file *pfile, const char __user *buffer, size_t length, loff_t *f_pos)
{
	/*if(down_interruptible(&fake_device->sem)) 
	{
	    return -ERESTARTSYS;
	}*/
	//spin_lock(&g_spin_lock);
	spin_lock_irqsave(&g_spin_lock, flags);

	struct char_device *dev = pfile->private_data;
	printk(KERN_INFO "%s Writing to device\n", __FUNCTION__);
	printk(KERN_INFO "%s fpos=%lld, offset=%lld\n", __FUNCTION__, pfile->f_pos, *f_pos);
	
	int idx_chunk = *f_pos/(DATA_NUM*DATA_SIZE);
	int idx_data  = *f_pos%DATA_SIZE;

	if(!dev->data)
	{
		dev->data = (char**) kmalloc(DATA_NUM * sizeof(char*), GFP_KERNEL);
		memset(dev->data, 0, DATA_NUM * sizeof(char*));
	}

	if(!dev->data[idx_chunk])
	{
		dev->data[idx_chunk] = (char*) kmalloc(DATA_SIZE * sizeof(char), GFP_KERNEL);
		memset(dev->data[idx_chunk], 0, DATA_SIZE * sizeof(char));
	}

	if( (idx_data+length) > DATA_SIZE)
	{
		length = DATA_SIZE-idx_data;
	}

	// Take data from user space to kernel space
	// copy_from_user(destination, source, sizeToTransfer)
	
	ret = copy_from_user(dev->data[idx_chunk]+idx_data, buffer, length);
	if(ret)
	{
	   ret = -EFAULT;
	   goto end;
	}
	*f_pos += length;
	if(*f_pos > dev->size)
	{
	   dev->size = *f_pos;
	}	
	printk("write length=%ld, dev->size=%d\n", length, dev->size);
end:
	//up(&fake_device->sem);
	//spin_unlock(&g_spin_lock);
	spin_unlock_irqrestore(&g_spin_lock, flags);
	return ret;
}


int release(struct inode *pinode, struct file *pfile)
{
	printk(KERN_INFO "%s Closed device\n", __FUNCTION__);
	return 0;
}


loff_t seek(struct file* pfile, loff_t offset, int option)
{
	printk(KERN_INFO "%s fpos=%lld, offset=%lld\n", __FUNCTION__, pfile->f_pos, offset);
	struct char_device *dev = pfile->private_data;
	loff_t newpos;
	switch(option)
	{
	  case 0: // SEEK_SET
	     newpos = offset;
	     break;
	  case 1: // SEEK_CUR
	     newpos = pfile->f_pos + offset;
	     break;
	  case 2: // SEEK_END
	     newpos = dev->size + offset;
	     break;
	  default:
	      return -EINVAL;
	}
	if(newpos < 0)
	{
	    return -EINVAL;
	}
	pfile->f_pos = newpos;
	printk(KERN_INFO "%s new file pos=%lld\n", __FUNCTION__, newpos);
	return newpos;
}

/*
int ioctl(struct inode* pinode, struct file* pfile, unsigned int f_flags, unsigned long )
{
	
}*/

module_init(chrdev_init);
module_exit(chrdev_exit);
MODULE_LICENSE("Dual BSD/GPL");
//MODULE_LICENSE("GPL");

