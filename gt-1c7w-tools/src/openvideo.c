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

#define BROWSER_PORT "/tmp/.socket/browser_port"
char newName[255];
struct sockaddr_un svaddr, claddr, rxaddr;

char cmd_call[103] = {
    0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x05
};

char cmd_call2[103] = {
    0x01, 0x0d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x05
};

void main(){
    int fd;
    fd = socket(AF_UNIX, SOCK_DGRAM, 1);

    snprintf(newName, sizeof(newName), "/tmp/browser-video.%ld", (long) getpid());

    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, BROWSER_PORT, sizeof(svaddr.sun_path)-1);

    claddr.sun_family = AF_UNIX;
    snprintf(claddr.sun_path, sizeof(claddr.sun_path), "%s", newName);
    
    if (bind(fd, (struct sockaddr *) &claddr, sizeof(struct sockaddr_un)) == -1){
        printf("bind error\n");
        exit(1);
    }

    sendto(fd, cmd_call, sizeof(cmd_call),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    sendto(fd, cmd_call2, sizeof(cmd_call2),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));


}
