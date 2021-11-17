#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define DEVICE_NAME "/dev/char_device"
#define buff_size 256

int main(int argc, char** argv)
{
    int ret;
    int fd;
    char ch, buffer[buff_size];
    
    fd = open(DEVICE_NAME, O_RDWR);	
    if(fd == -1)
    {
    	printf("fail to open %s\n", DEVICE_NAME);
    	exit(-1); 	
    } 
    
    while(1)
    {
    	memset(buffer, '\0', sizeof(buffer));
    	printf("enter r/w/e\n");
    	scanf("%c", &ch);
    	switch(ch)
    	{
           case 'r':
             ret = read(fd, buffer, sizeof(buffer));
             if(ret == -1)
             {
             	printf("read fail\n");
             }
             printf("read from device = %s\n", buffer);
             break;	
           case 'w':
             printf("enter data to write\n");
             scanf("%s", buffer);        
             ret = write(fd, buffer, sizeof(buffer));
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
    }
    close(fd);   
    return 0;
}
