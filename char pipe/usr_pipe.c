// A user program interacts with char_pipe 
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
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
    {
      goto readonly;
    }

    char ch;
    char buffer[buff_size];
    int ret;


    struct pollfd pfds_write = {
      .fd = fd,
      .events = POLLOUT,
    };

    struct pollfd pfds_read = {
      .fd = fd,
      .events = POLLIN,
    };
    int rc;
    int nfds = 1; // number of pfds, I use one in this example.

    do
    {
      memset(buffer, 0, sizeof(buffer));
      if(ch != '\n')
        printf("enter r/w/p/e\n");
      scanf("%c", &ch);
      switch(ch)
      {
           /*case 'r': // read
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
            break; */
           case 'w': // write
             printf("enter data to write (data)\n");
             scanf("%s", buffer);      
             ret = write(fd, buffer, strlen(buffer));
             if(ret == -1)
             {
               printf("write fail\n");
             }
             printf("write %s to device, size=%ld\n", buffer, strlen(buffer));  
             break;
           case 'p': // poll
             // polling to check writable
             rc = poll(&pfds_write, nfds, 0); 
             if(rc)
             {
               printf("writable\n");
             }  
             else
             {
               printf("currently not writable\n");
             }
             // polling to check readable
             rc = poll(&pfds_read, nfds, 0); 
             if(rc)
             {
               printf("readable\n");
             }  
             else
             {
               printf("currently not readable\n");
             }
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

    char input[256];

readonly:
    do
    {
      printf("enter how many bytes to read\n");
      fgets(input, sizeof(input), stdin);
      int len = strlen(input);
      if(len > 0)
      {
        input[len-1] = 0;
      }
      ret = read(fd, buffer, atoi(input));
      if(ret == -1)
      {
        //printf("read fail or no data is available\n");
        continue;
      }
      else
      {
        printf("read from device = %s, size = %ld\n", buffer, strlen(buffer));
        memset(buffer, 0, strlen(buffer));
      }
    } while(1); // read infinitly       
 
    close(fd);   
    return 0;
}
