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

// Guard 2 - 1 is taken by gateway
// #define DESIRED 0x0e, 0x02


#include <sys/ioctl.h>


unsigned long blah;
#define NUM_MSG 10

int pid_to_hook;
int fd;
struct pollfd sfds[1];


// from ioctl fd
void get_response_direct(){

    unsigned long out[150];
    int s,x;
    
    for(x=0;x<20;x++){
        s = poll(sfds,1,60);
        if (sfds[0].revents & POLLIN){
            ioctl(fd,4,&out);
            for (x=0;x<6; x++){
                printf("%08x ", out[x]);
            }
            printf("\n");            
        }
        
    }
}


// function to read off ps_gtl ioctl
int get_responses(){
    int x,y ;
    struct pt_regs regs;
    unsigned int atdata;
    for(x=0;x<70;x++){
        if (waitpid(pid_to_hook, NULL, WNOHANG))
        {
            ptrace(PTRACE_GETREGS, pid_to_hook, NULL, &regs);
            // printf("%x %x\n", regs.regs[3], regs.regs[4]);
            if (regs.regs[3] == 0x36 && regs.regs[4] == 4)
            {

                // //int syscall = regs.orig_rax;
                int data;

                for (y = 0; y < 6; y++)
                {
                    atdata = ptrace(PTRACE_PEEKDATA, pid_to_hook, regs.regs[11] + (y * 4), NULL);
                    printf(" %08x", atdata);
                }
                printf("\n");
            }
            ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
            continue;
        }
        usleep(60000);
    }
}



int main(int argc, char *argv[])
{
    
    int err;
    int x;
    

    if (argc != 2)
    {
        printf("Not enough args. Usage ./hookioctl {PID}");
        return 1;
    }

    pid_to_hook = atoi(argv[1]);

    ptrace(PTRACE_ATTACH, pid_to_hook, NULL, NULL);
    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACECLONE);
    printf("Attached to pid\n");

    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACESYSGOOD);


    char msg[NUM_MSG][6*4] = {
   
  // 0    1   2    3      4    5   6     7     8    9      10  11     12  13    14    15    16    17   18     19    20   21   22   23
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00, 0x38, 0x0f,0x0e, 0x02, 0x26, 0xff, 0x00, 0x00,0x00,0x05,0x00 },       
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00, 0x0f, 0x01,0x0e, 0x02, 0x40, 0xff, 0x00, 0x00,0x00,0x05,0x00 },      
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00, 0x0f, 0x01,0x0e, 0x02, 0x29, 0xff, 0x00, 0x00,0x00,0x05,0x00 },      
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00, 0x0c, 0x03,0x0e, 0x02, 0x9e, 0xff, 0x00, 0x00,0x00,0x05,0x00 },      
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00, 0x0c, 0x03,0x0e, 0x02, 0x83, 0xff, 0x00, 0x00,0x00,0x05,0x00 },       
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00, 0x0c, 0x03,0x0e, 0x02, 0x84, 0xff, 0x00, 0x00,0x00,0x05,0x00 },       
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00, 0x00,0x00,0x00, 0x00, 0x0c, 0x03,0x0e, 0x02, 0x81, 0xff, 0x00, 0x00,0x00,0x05,0x00 },
  // lift control
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x13, 0x00,0x00,0x00, 0x00, 0x8f, 0x01,0x0c, 0x01, 0xb0, 0x00, 0x59, 0x56, 0xc,0x01,0x00},
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x13, 0x00,0x00,0x00, 0x00, 0x8f, 0x01,0x0c, 0x01, 0xb0, 0x00, 0x59, 0x56, 0xc,0x01,0x00},
  { 0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x13, 0x00,0x00,0x00, 0x00, 0x8f, 0x01,0x0c, 0x01, 0xb0, 0x00, 0x59, 0x56, 0xc,0x01,0x00},

    };

    

    fd = open(DRIVER_PATH, 2);



    if (fd < 0)
    {
        printf("Cannot open device file...\n");
        return;
    }

    sfds[0].fd = fd;
    sfds[0].events = POLLIN;


    ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
    waitpid(pid_to_hook, NULL, 0); // stop ps_gtl to stop interference

    err = ioctl(fd, 6, &msg[x]);
    if (err < 0)
    {
        printf("ioctl returned less than 0 but original code didn't handle this.\n");
    }
    if (err < 0)
    {
        printf("ioctl returned less than 1 which is probably an error\n");
        // return;
    }
    printf("id %x\n", err);

    for(x=0;x<NUM_MSG;x++){
        msg[x][8] = x; // counter

        //override address
        // msg[x][15] = 0xe;
        // msg[x][16] = 0x02;

        //update chk sum
        char chk = 0xff;
        if (msg[x][19]){
            chk = chk - msg[x][12] - msg[x][13]- msg[x][14] - msg[x][15] - msg[x][16] - msg[x][17] -  msg[x][18] - msg[x][19]+ 1;
            msg[x][20] = chk;
        } else {
            chk = chk - msg[x][12] - msg[x][13]- msg[x][14] - msg[x][15] - msg[x][16] - msg[x][17] + 1;
            msg[x][18] = chk;
        }
        //chk = chk - msg[x][12] - msg[x][13]- msg[x][14] - msg[x][15] - msg[x][16] - msg[x][17] -  msg[x][18] - msg[x][19]+ 1;
        

        //printf("%x\n",chk);

        ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
        err = ioctl(fd, 3, &msg[x]);
        if (err < 0)
        {
            printf("ioctl returned less than 0 but original code didn't handle this.\n");
        }
        if (err < 0)
        {
            printf("ioctl returned less than 1 which is probably an error\n");
            // return;
        }
        printf("sent%d: %x\n", x, err);
        //get_responses();
        get_response_direct();
        
        usleep(150000);
    }
    printf("done\n");
    while(1){
        
        get_responses();
    }
}