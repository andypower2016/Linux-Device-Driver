// An user program interacts with char_device using ioctl
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
    char ch;
    char buffer[buff_size];
    do
    {
      memset(buffer, 0, sizeof(buffer));
      if(ch != '\n')
        printf("enter r/w/e\n");
      scanf("%c", &ch);
      switch(ch)
      {
           case 'r':
             ioctl(dev, RD_VALUE, buffer);
             printf("The data after read is : %s\n", buffer);
             break; 
           case 'w':   
             printf("enter data to write\n");
             scanf("%s", buffer);      
             ioctl(dev, WR_VALUE, buffer);
             break;
           case 'e':
            close(dev);
            return 0; 
           default:
             break; 
      } 
      
    }while(1);
    close(dev);   
    return 0;
}
