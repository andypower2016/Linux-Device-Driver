#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>		// file operations structure, allows user to open/read/write/close
#include <linux/cdev.h>	// this is a char driver, makes cdev available
#include <linux/semaphore.h>   // used to access semaphores for syncronization
#include <asm/uaccess.h>	// copy_to_user, copy_from_user (used to map data from user space to kernel space)


// fake char device structure
struct char_device 
{
    char buffer[256];
    struct semaphore sem;
} device ;

// To register a fake char device, we need cdev and other variables
struct cdev *mcdev; // m stands 'my'
int major_num; // will store our major number, extracted from dev_t using macro - mknod /dev/device_file c major minor
int ret;       // holds the return value of each kernel function
dev_t dev_num; // device number

#define DEVICE_NAME "char_device"

int open(struct inode *, struct file *);
ssize_t read(struct file *, char __user *, size_t, loff_t *);
ssize_t write(struct file *, const char __user *, size_t, loff_t *);
int release(struct inode *, struct file *);

struct file_operations fileop = 
{
    .owner = THIS_MODULE,
    .open = open,
    .read = read,
    .write = write,
    .release = release,
};

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
	
	// Initialize semaphore
	sema_init(&device.sem, 1); // initial value one, means nothing is locked
	
	return 0;
}

static __exit void chrdev_exit(void)
{
	cdev_del(mcdev);
	unregister_chrdev_region(dev_num, 1);
	printk(KERN_ALERT "Unload Device Complete\n");
}

// Called when device is called by kernel
int open(struct inode *pinode, struct file *pfile)
{
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	// Only allow one process to open this device by using a semaphore as a mutex
	if(down_interruptible(&device.sem) != 0) // sem = sem - 1
	{
	   printk(KERN_ALERT "can't lock device when open\n");
	   return ret;	
	}
	
	printk(KERN_INFO "opened device ! \n");
	return 0;
}

// Called when user wants to get information from the device
ssize_t read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	printk(KERN_INFO "Reading from device\n");
	// Take data from kernel space to user space
	// copy_from_user(destination, source, sizeToTransfer)
	ret = copy_to_user(buffer, device.buffer, length);
}

// Called when user wants to send data to device
ssize_t write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	printk(KERN_INFO "Writing to device\n");
	// Take data from user space to kernel space
	// copy_from_user(destination, source, sizeToTransfer)
	ret = copy_from_user(device.buffer, buffer, length);
	return length;
}


int release(struct inode *pinode, struct file *pfile)
{
	printk(KERN_ALERT "Inside %s Function\n", __FUNCTION__);
	up(&device.sem);  // sem = sem + 1
	printk(KERN_INFO "Closed device\n", __FUNCTION__);
	return 0;
}

module_init(chrdev_init);
module_exit(chrdev_exit);
MODULE_LICENSE("GPL");

