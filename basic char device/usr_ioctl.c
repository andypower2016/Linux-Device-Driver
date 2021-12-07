#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "ioctrl_test.h"

#define DEVICE_NAME "/dev/char_device"
#define buff_size 256

int main(int argc, char** argv)
{
    int dev = open(DEVICE_NAME, O_WRONLY);
    if(dev == -1) 
    {
      printf("Opening was not possible!\n");
      return -1;
    }

    char buffer[buff_size];
    ioctl(dev, RD_VALUE, buffer);
    printf("The data after read is : %s\n", buffer);

    memset(buffer,0,sizeof(buffer));
    char user_data[] = "data in user";
    memcpy(buffer, user_data, strlen(user_data));
    ioctl(dev, WR_VALUE, buffer);

    sleep(2);
    memset(buffer,0,sizeof(buffer));
    ioctl(dev, RD_VALUE, buffer);
    printf("The data after update is : %s\n", buffer);

    close(dev);
    return 0;
}
