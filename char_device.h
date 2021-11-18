#ifndef CHAR_DEVICE
#define CHAR_DEVICE

#define DEVICE_NAME "char_device"

// fake char device structure
struct char_device 
{
    char buffer[256];	   // device data
    struct semaphore sem; // semaphore, for locking the device
    struct cdev *cdev;    // cdev
};

struct char_device *fake_device = NULL;
int major_num; // will store our major number, extracted from dev_t using macro - mknod /dev/device_file c major minor
int ret;       // holds the return value of each kernel function
dev_t dev_num; // device number


// device prototypes
int open(struct inode *, struct file *);
ssize_t read(struct file *, char __user *, size_t, loff_t *);
ssize_t write(struct file *, const char __user *, size_t, loff_t *);
int release(struct inode *, struct file *);


#endif
