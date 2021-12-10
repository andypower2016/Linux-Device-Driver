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
#include <linux/wait.h> 	  // wait_queue
#include <linux/ioctl.h>

#include "ioctrl_test.h"
#include "char_device.h"

// char device structure
struct char_device 
{
    char **data;	      // device data for file operation (read/write)
    int size;		      // ammount of data
    struct semaphore sem; // semaphore, for locking the device
    struct cdev mcdev;    // cdev
    char* ioctrl_data;    // for ioctl test
};

struct char_device *char_dev = NULL;
// device registraion datas
int minior_num = 0; // minor number of the device, we set to zero here
int major_num; 		// will store our major number, extracted from dev_t using MAJOR macro
dev_t dev_num; 		// device number
int dev_count = 1;  // number of devices

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
    .unlocked_ioctl = ioctl_test,
 };

static void free_dev_data(struct char_device *dev)
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
	if(dev->ioctrl_data)
		kfree(dev->ioctrl_data);
}

static void char_cleanup(void)
{
	if(char_dev)
	{
	   cdev_del(&char_dev->mcdev);
	   free_dev_data(char_dev);
	   kfree(char_dev);
	}
	unregister_chrdev_region(dev_num, dev_count);
}

static int char_init(void)
{
    ret = 0; // 0 for success
	
	char_dev = kmalloc(sizeof(struct char_device), GFP_KERNEL);
	if(!char_dev)
	{
	   DBG("Fail to kalloc device");
	   ret = -ENOMEM;
	   goto err;
	}
	memset(char_dev, 0, sizeof(struct char_device));	

	// kmalloc ctrl data
	char_dev->ioctrl_data = (char*) kmalloc(buffer_size, GFP_KERNEL);
	memset(char_dev->ioctrl_data, 0, buffer_size);
	char init_data[] = "ctl init data";
	memcpy(char_dev->ioctrl_data, init_data, strlen(init_data));

	// init semaphore
	sema_init(&char_dev->sem, 1); // initial value one, means nothing is locked
	
	// init cdev
	cdev_init(&char_dev->mcdev, &fileop);
	char_dev->mcdev.owner = THIS_MODULE;
	ret = cdev_add(&char_dev->mcdev, dev_num, 1);
	if(ret < 0)
	{
	   DBG("Fail to add cdev to kernel");
	   goto err;
	}
	return ret;
	
err:
	char_cleanup();
	return ret;
}

int open(struct inode *pinode, struct file *pfile)
{ 
	//if(down_trylock(&char_dev->sem)) 
	if(down_interruptible(&char_dev->sem))  // Only one process is allowed when accessing this device
	    return -ERESTARTSYS;
	
	struct char_device *dev = container_of(pinode->i_cdev, struct char_device, mcdev);
	pfile->private_data = dev;
	DBG("Opened device ! device datasize=%d\n", dev->size);
	return 0;
}
int release(struct inode *pinode, struct file *pfile)
{
	DBG("Closed device");
	up(&char_dev->sem);
	return 0;
}

// Called when user wants to get information from the device
ssize_t read(struct file *pfile, char __user *buffer, size_t length, loff_t *f_pos)
{
	struct char_device *dev = pfile->private_data;
	DBG("fpos=%lld, offset=%lld\n", pfile->f_pos, *f_pos);
	
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
	return ret;
}

// Called when user wants to send data to device
ssize_t write(struct file *pfile, const char __user *buffer, size_t length, loff_t *f_pos)
{
	struct char_device *dev = pfile->private_data;
	DBG("fpos=%lld, offset=%lld\n", pfile->f_pos, *f_pos);
	
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
	DBG("write length=%ld, dev->size=%d", length, dev->size);
end:
	return ret;
}

loff_t seek(struct file* pfile, loff_t offset, int option)
{
	DBG("fpos=%lld, offset=%lld", pfile->f_pos, offset);
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
	DBG("new file pos=%lld", newpos);
	return newpos;
}

long int ioctl_test(struct file *file, unsigned cmd, unsigned long arg)
{
	struct char_device *dev = file->private_data;
	switch(cmd)
	{
		case WR_VALUE:
			memset(dev->ioctrl_data, 0, strlen(dev->ioctrl_data));
			if(copy_from_user(dev->ioctrl_data, (char *) arg, strlen((char *)arg))) 
			{
				DBG("Error copying data from user!");
			}
			else
			{
				DBG("Update the data to : %s", dev->ioctrl_data);
			}
			break;
		case RD_VALUE:		
			if(copy_to_user((char *) arg, dev->ioctrl_data, strlen(dev->ioctrl_data))) 
				DBG("Error copying data to user!");
			else
				DBG("The data was copied!");
			break;
		default:
			break;
	}
	return 0;
}

static __init int char_device_init(void)
{
	// use dynamic allocation to assign our device
	ret = alloc_chrdev_region(&dev_num, minior_num, dev_count, DEVICE_NAME);
	if(ret < 0)
	{
	   DBG("Fail to allocate major number");
	   return ret;
	}
	major_num = MAJOR(dev_num); // extracts the major number and store in variable
	DBG("Load Device, Major number=%d", major_num);

	// char device init
	ret = char_init();
	if(ret < 0)
		return ret;

	// char pipe init
	dev_t dev = MKDEV(major_num, minior_num+1); // makes a device number(major, minior+1)
	ret = char_p_init(dev);
	if(ret < 0)
		return ret;

	return 0;
}

static __exit void char_device_exit(void)
{
	char_cleanup();
	char_p_cleanup();
	DBG("Unload Device Complete");
}

module_init(char_device_init);
module_exit(char_device_exit);
MODULE_LICENSE("Dual BSD/GPL");
//MODULE_LICENSE("GPL");

