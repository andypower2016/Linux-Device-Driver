#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>		// file operations structure, allows user to open/read/write/close
#include <linux/cdev.h>	// this is a char driver, makes cdev available
#include <linux/semaphore.h>   // used to access semaphores for syncronization
#include <asm/uaccess.h>	// copy_to_user, copy_from_user (used to map data from user space to kernel space)

#include <linux/slab.h> // kmalloc/kfree
#include "char_device.h"

int ret;

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
	   cdev_del(fake_device->cdev);
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
	sprintf(fake_device->buffer, "Init data for device");
	fake_device->size = 0;
	// init semaphore
	sema_init(&fake_device->sem, 1); // initial value one, means nothing is locked
	
	// init cdev
	fake_device->cdev = cdev_alloc(); 
	fake_device->cdev->ops = &fileop;
	fake_device->cdev->owner = THIS_MODULE;
	ret = cdev_add(fake_device->cdev, dev_num, 1);
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
	
	/*
	// (2) create cdev structure, initialize cdev
	mcdev = cdev_alloc(); 
	mcdev->ops = &fileop;
	mcdev->owner = THIS_MODULE;
	// add cdev to kernel
	// int cdev_add(struct cdev* dev, dev_t num, unsigned int count)
	ret = cdev_add(mcdev, dev_num, 1);
	if(ret < 0)
	{
	   printk(KERN_ALERT "Fail to add cdev to kernel\n");
	   return ret;
	}
	
	//(4) Initialize semaphore
	sema_init(&device.sem, 1); // initial value one, means nothing is locked
	
	// init device data
	sprintf(device.buffer, "Init data for device");
	
	*/
	
	
	// setup device dynamically
	ret = setup_device();
	
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
	printk(KERN_INFO "opened device ! \n");
	return ret;
}

// Called when user wants to get information from the device
ssize_t read(struct file *pfile, char __user *buffer, size_t length, loff_t *f_pos)
{
	if(down_interruptible(&fake_device->sem)) 
	{
	    return -ERESTARTSYS;
	}
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	printk(KERN_INFO "Reading from device, data=%s\n", fake_device->buffer);
	// Take data from kernel space to user space
	// copy_from_user(destination, source, sizeToTransfer)
	ret = copy_to_user(buffer, fake_device->buffer, length);
	if(ret)
	{
	   ret = -EFAULT;
	   goto end;
	}
	*f_pos += length;
end:
	up(&fake_device->sem);
	return ret;
}

// Called when user wants to send data to device
ssize_t write(struct file *pfile, const char __user *buffer, size_t length, loff_t *f_pos)
{
	if(down_interruptible(&fake_device->sem)) 
	{
	    return -ERESTARTSYS;
	}
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	printk(KERN_INFO "Writing to device , data=%s\n", buffer);
	// Take data from user space to kernel space
	// copy_from_user(destination, source, sizeToTransfer)
	ret = copy_from_user(fake_device->buffer, buffer, length);
	if(ret)
	{
	   ret = -EFAULT;
	   goto end;
	}
	*f_pos += length;
	fake_device->size = *f_pos;	
end:
	up(&fake_device->sem);
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

