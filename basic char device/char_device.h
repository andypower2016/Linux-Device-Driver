#ifndef CHAR_DEVICE
#define CHAR_DEVICE

#define DEVICE_NAME "char_device"

// total 120 kb
#define DATA_NUM  30   // 30 chunks
#define DATA_SIZE 4096 // chunk length (4kb)

#define DBG(format, arg...) printk("[%s]:%d => " format "\n",__FUNCTION__,__LINE__,##arg)

// device driver prototypes
int open(struct inode *, struct file *);
ssize_t read(struct file *, char __user *, size_t, loff_t *);
ssize_t write(struct file *, const char __user *, size_t, loff_t *);
int release(struct inode *, struct file *);
loff_t seek(struct file *, loff_t, int);
long int ioctl_test(struct file *file, unsigned cmd, unsigned long arg);

// char pipe
int char_p_init(dev_t dev);
void char_p_cleanup(void);

#endif
