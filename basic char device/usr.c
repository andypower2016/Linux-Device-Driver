// A user program that interacts with char_device
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_NAME "/dev/char_device"
#define buff_size 256

int main(int argc, char** argv)
{
    int ret;
    int fd;
    char ch, buffer[buff_size];
    int row, col, size;
    
    fd = open(DEVICE_NAME, O_RDWR);	
    if(fd == -1)
    {
    	printf("fail to open %s\n", DEVICE_NAME);
    	exit(-1); 	
    } 
    
    do
    {
    	memset(buffer, '\0', sizeof(buffer));
    	if(ch != '\n')
    	  printf("enter r/w/e\n");
    	scanf("%c", &ch);
    	switch(ch)
    	{
           case 'r':
             printf("enter data pos to read (rol, col, size)\n");
             scanf("%d %d %d", &row, &col, &size);   
             lseek(fd, row*col, SEEK_SET);	
             ret = read(fd, buffer, size);
             if(ret == -1)
             {
             	 printf("read fail\n");
             }
             printf("read from device = %s\n", buffer);
             break;	
           case 'w':   
             printf("enter data to write (rol, col, data)\n");
             scanf("%d %d %s", &row, &col, buffer);      
             lseek(fd, row*col, SEEK_SET); 
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
}
