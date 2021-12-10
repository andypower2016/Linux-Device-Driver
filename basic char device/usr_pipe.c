// A user program interacts with char_pipe 
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "ioctrl_test.h"

#define DEVICE_NAME "/dev/char_pipe"
#define buff_size 256

int main(int argc, char** argv)
{
    int mode;
    mode = strcmp(argv[1],"r") == 0 ? O_RDONLY : O_WRONLY; 
    int fd = open(DEVICE_NAME, mode);
    if(fd == -1) 
    {
       printf("Opening device fail !\n");
       return -1;
    }
    if(mode == O_RDONLY)
      goto readonly;

    char ch;
    char buffer[buff_size];
    int ret;
    do
    {
      memset(buffer, 0, sizeof(buffer));
      if(ch != '\n')
        printf("enter r/w/e\n");
      scanf("%c", &ch);
      switch(ch)
      {
           case 'r':
            while(1) // read infinitly
            {
              ret = read(fd, buffer, buff_size);
              if(ret == -1)
              {
                //printf("read fail or no data is available\n");
                continue;
              }
              printf("read from device = %s\n", buffer);
            }
            break; 
           case 'w':   
             printf("enter data to write (data)\n");
             scanf("%s", buffer);      
             ret = write(fd, buffer, strlen(buffer));
             if(ret == -1)
             {
               printf("write fail\n");
             }
             printf("write %s to device\n", buffer);  
             break;
           case 'e':
            close(fd);
            return 0; 
           default:
             break; 
      } 
      
    }while(1);
    close(fd);   
    return 0;

readonly:
    while(1) // read infinitly
    {
      ret = read(fd, buffer, buff_size);
      if(ret == -1)
      {
        //printf("read fail or no data is available\n");
        continue;
      }
      else
      {
        printf("read from device = %s\n", buffer);
        memset(buffer, 0, sizeof(buffer));
      }
    }        

    close(fd);   
    return 0;
}
