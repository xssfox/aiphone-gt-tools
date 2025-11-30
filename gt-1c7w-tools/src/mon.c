#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/ioctl.h>

#define DRIVER_PATH "/dev/tscomdrv"


void main(){
     int fd, s, x;
     struct pollfd sfds[1];
     fd = open(DRIVER_PATH, 2);
     sfds[0].fd = fd;
     sfds[0].events = POLLIN;
     unsigned long out[150];

     while(1){
        s = poll(sfds,1,100);
        if (sfds[0].revents & POLLIN){
            ioctl(fd,4,&out);
            for (x=0;x<10; x++){
                printf("%08x ", out[x]);
            }
            printf("\n");
        }
     }
     
}