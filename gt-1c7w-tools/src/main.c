#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

//#define gtl_sock "/tmp/.socket/gtl_port"

// hijack this.
//#define appli_sock "/tmp/.socket/appli_port"
//#define old_sock "/tmp/.socket/appli_port.old"

char newName[255];
char oldName[255];

// Handler for SIGINT, triggered by 
// Ctrl-C at the keyboard 
void singalHandler(int sig)  { 
    rename(newName, oldName);
    printf("exiting. renamed %s to %s\n", newName, oldName);
    exit(sig);
} 

#define BUF_SIZE 150
int main(int argc, char *argv[]) {
    if (argc != 2){
        printf("Not enough args. Usage ./blah dfgd.socket");
        return 1;
    }

    
    snprintf(newName, sizeof(newName), "%s.%ld", argv[1], (long) getpid());
    snprintf(oldName, sizeof(oldName), "%s", argv[1]);
    signal(SIGINT, singalHandler); 
    rename(argv[1], newName);
    

    printf("renamed %s to %s\n", argv[1], newName);
    
    struct sockaddr_un svaddr, claddr, rxaddr;
    ssize_t numBytes;
    char resp[BUF_SIZE];
    int x;
    int sockaddr_size;
    sockaddr_size = sizeof(struct sockaddr_un);

    
    int sfd, j;

    sfd = socket(AF_UNIX, SOCK_DGRAM, 1);
    if (sfd == -1){
        printf("socket error\n");
        singalHandler(errno);
    }

    claddr.sun_family = AF_UNIX;
    // snprintf(claddr.sun_path, sizeof(claddr.sun_path), "/tmp/gw.sock.%ld", (long) getpid());
    snprintf(claddr.sun_path, sizeof(claddr.sun_path), argv[1]);
    
    if (bind(sfd, (struct sockaddr *) &claddr, sizeof(struct sockaddr_un)) == -1){
        printf("bind error");
        singalHandler(errno);
    }
    printf("started monitoring\n");
    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, newName, sizeof(svaddr.sun_path)-1);
    
    //sendto(sfd, '\x00', 1,0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    while (1){
        numBytes = recvfrom(sfd, resp, BUF_SIZE, 0, (struct sockaddr  *) &rxaddr, &sockaddr_size);
        if (numBytes == -1){
            printf("recvfrom exit");
            singalHandler(errno);
        }
        sendto(sfd, resp, numBytes,0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
        printf("Response %s %d:", rxaddr.sun_path,(int) numBytes);
        for (x=0;x<numBytes; x++){
            printf("%02x ", resp[x]);
        }
        printf("\n");
        
    }

}
