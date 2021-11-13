#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>

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
	register_chrdev(260, "char_device", &fileop);
	return 0;
}

static __exit void chrdev_exit(void)
{
	unregister_chrdev(260, "char_device");
}

int open(struct inode *pinode, struct file *pfile)
{
	printk(KERN_ALERT "Inside %s Funtion\n", __FUNCTION__);
	return 0;
}
ssize_t read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_ALERT "Inside %s Funtion\n", __FUNCTION__);
	return 0;
}
ssize_t write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_ALERT "Inside %s Funtion\n", __FUNCTION__);
	return length;
}
int release(struct inode *pinode, struct file *pfile)
{
	printk(KERN_ALERT "Inside %s Funtion\n", __FUNCTION__);
	return 0;
}

module_init(chrdev_init);
module_exit(chrdev_exit);
MODULE_LICENSE("GPL");

