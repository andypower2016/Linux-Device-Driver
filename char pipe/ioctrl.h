#ifndef IOCTRL_H
#define IOCTRL_H

#define CHAR_IOC_MAGIC 'c'

#define buffer_size 256
#define WR_VALUE _IOW(CHAR_IOC_MAGIC, 1, char*)
#define RD_VALUE _IOR(CHAR_IOC_MAGIC, 2, char*) 

#endif