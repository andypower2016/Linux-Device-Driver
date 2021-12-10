// signal process with stdin
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#define print(format, args...) printf("[%s:%d]" format "\n", __FUNCTION__, __LINE__, ##args)

void handler(int sig)
{
    print("process pid=%d is signaled", getpid());
}

int main(int argc, char** argv)
{
    int fd, oflags;
    signal(SIGIO, &handler);
    fcntl(STDIN_FILENO, F_SETOWN, getpid());
    oflags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, oflags | FASYNC);
    while(1);
    return 0;
}
