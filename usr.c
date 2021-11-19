#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_NAME "/dev/char_device"
#define buff_size 4096

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
    
    do
    {
    	memset(buffer, '\0', sizeof(buffer));
    	if(ch != '\n')
    	  printf("enter r/w/e\n");
    	scanf("%c", &ch);
    	switch(ch)
    	{
           case 'r':
             lseek(fd, 0, SEEK_SET);	// seek fd to begin, read out the entire device buffer
             ret = read(fd, buffer, sizeof(buffer));
             if(ret == -1)
             {
             	printf("read fail\n");
             }
             printf("read from device = %s\n", buffer);
             break;	
           case 'w':        
             lseek(fd, 0, SEEK_END); // seek fd to end, append data to device buffer     
             printf("enter data to write\n");
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
}
