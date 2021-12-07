#ifndef CHAR_DEVICE
#define CHAR_DEVICE

#define DEVICE_NAME "char_device"

#define DATA_NUM  30   // 30 chunks
#define DATA_SIZE 4096 // chunk length

// fake char device structure
struct char_device 
{
    char **data;	   // device data
    int size;		   // ammount of data
    struct semaphore sem; // semaphore, for locking the device
    struct cdev mcdev;    // cdev

    char* ioctrl_data;
};
struct char_device *fake_device = NULL;

// device registraion datas
int minior_num = 0;
int major_num; // will store our major number, extracted from dev_t using macro - mknod /dev/device_file c major minor  
dev_t dev_num; // device number


// device prototypes
int open(struct inode *, struct file *);
ssize_t read(struct file *, char __user *, size_t, loff_t *);
ssize_t write(struct file *, const char __user *, size_t, loff_t *);
int release(struct inode *, struct file *);
loff_t seek(struct file *, loff_t, int);
long int ioctl_test(struct file *file, unsigned cmd, unsigned long arg);

#endif
