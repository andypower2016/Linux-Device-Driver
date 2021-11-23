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

static void cleanup_device(void)
{
	if(fake_device)
	{
	   cdev_del(&fake_device->mcdev);
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
	
	// init data
	char str[20] = "Init data for device";
	memcpy(fake_device->buffer, str, sizeof(str));
	fake_device->size = sizeof(str);
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
	printk(KERN_ALERT "Unload Device Complete\n");
}


// Called when device is called by kernel
int open(struct inode *pinode, struct file *pfile)
{ 
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	struct char_device *dev = container_of(pinode->i_cdev, struct char_device, mcdev);
	pfile->private_data = dev;
	printk(KERN_INFO "opened device ! device datasize=%d\n", dev->size);
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

	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	printk(KERN_INFO "Reading from device, data=%s\n", fake_device->buffer);
	printk("pfile->f_pos=%lld loff_t=%lld\n", pfile->f_pos,  *f_pos);
	// Take data from kernel space to user space
	// copy_from_user(destination, source, sizeToTransfer)	
	struct char_device *dev = pfile->private_data;
	ret = copy_to_user(buffer, dev->buffer+*f_pos, length);
	if(ret)
	{
	   ret = -EFAULT;
	   goto end;
	}
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
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	printk(KERN_INFO "Writing to device , data=%s\n", buffer);
	printk("pfile->f_pos=%lld loff_t=%lld\n", pfile->f_pos,  *f_pos);
	// Take data from user space to kernel space
	// copy_from_user(destination, source, sizeToTransfer)
	struct char_device *dev = pfile->private_data;
	ret = copy_from_user(dev->buffer+*f_pos, buffer, length);
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
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	printk(KERN_INFO "Closed device\n");
	return 0;
}


loff_t seek(struct file* pfile, loff_t offset, int option)
{
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
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
	printk("new file pos=%lld\n", newpos);
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

