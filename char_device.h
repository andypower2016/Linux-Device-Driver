#ifndef CHAR_DEVICE
#define CHAR_DEVICE

#define DEVICE_NAME "char_device"

// fake char device structure
struct char_device 
{
    char buffer[4096];	   // device data
    int size;		   // ammount of data
    struct semaphore sem; // semaphore, for locking the device
    struct cdev mcdev;    // cdev
};
struct char_device *fake_device = NULL;

// device registraion datas
int major_num; // will store our major number, extracted from dev_t using macro - mknod /dev/device_file c major minor  
dev_t dev_num; // device number


// device prototypes
int open(struct inode *, struct file *);
ssize_t read(struct file *, char __user *, size_t, loff_t *);
ssize_t write(struct file *, const char __user *, size_t, loff_t *);
int release(struct inode *, struct file *);
loff_t seek(struct file *, loff_t, int);

#endif
