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
#include<arpa/inet.h>
#include <time.h>

#define DRIVER_PATH "/dev/tscomdrv"
// pause ps_gtl for a number of seconds 
#define WAIT_TIME 8

#include <sys/ioctl.h>

#define PORT 4444
time_t tp_last;
struct timespec tp_current;

int pid_to_hook;
int fd;
char counter;
struct pollfd sfds[1];

struct sockaddr_in si_me, si_other;
int s, i, slen = sizeof(si_other) , recv_len;

struct sockaddr_in si_broadcast;
struct in_addr broadcast_address;

// function to read off ps_gtl ioctl
int get_responses(){
    int x,y ;
    struct pt_regs regs;
    unsigned char data[6*4];

    if (waitpid(pid_to_hook, NULL, WNOHANG))
    {
        ptrace(PTRACE_GETREGS, pid_to_hook, NULL, &regs);
        if (regs.regs[3] == 0x36 && regs.regs[4] == 4)
        {


            for (y = 0; y < 6; y++)
            {
                unsigned int from_ioctl;
                from_ioctl = ptrace(PTRACE_PEEKDATA, pid_to_hook, regs.regs[11] + (y * 4), NULL);
                memcpy(&data[4*y], &from_ioctl, 4);
            }
            printf("rx: ");
            for (y=0;y<sizeof(data); y++){
                printf("%02x", data[y]);
            }
            printf("\n");

            // send to udp
            int err;
            err = sendto(s, &data, sizeof(data), 0, (struct sockaddr*) &si_broadcast, slen);
            if (err == -1)
            {
                printf("failed to send to udp: %d\n", errno);
            }
        }
       
    }
    if (tp_current.tv_sec > tp_last + WAIT_TIME ) { 
        ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
    } else {

        struct pollfd io_sfds[1];
        int poll_s;
        io_sfds[0].fd = fd;
        io_sfds[0].events = POLLIN;
        poll_s = poll(io_sfds,1,1);
        int err;

        if (io_sfds[0].revents & POLLIN){
            err = ioctl(fd,4,&data);
        
            if (err != 0){
                printf("for %d from ioctl rx \n");
                return;
            }
            printf("rx2: ");
            for (y=0;y<sizeof(data); y++){
                printf("%02x", data[y]);
            }
            printf("\n");

            err = sendto(s, &data, sizeof(data), 0, (struct sockaddr*) &si_broadcast, slen);
            if (err == -1)
            {
                printf("failed to send to udp: %d\n", errno);
            }
        }
    }
}


void send_bus(char *data, int len){
    int err;
    int print_counter;
    printf("trying to send: %d bytes\n", len);

    if (len < 5){ // offset by 1 as first byte is always 0x00
        printf("Packet to short\n");
        return;
    }
    if (len > 9){ // leave one data byte for checksum
        printf("Packet to long\n");
        return;
    }

    char msg[6*4] = {
            // send or rx?
            0x01,0x00,0x00,0x00, // 0-3
            // always blank
            0x00,0x00,0x00,0x00, // 4-7
            // packet count
            0x13,0x00,0x00,0x00, // 8-11

                  // data + checksum (13-21 - 10 bytes)
            0x00,0x00,0x00,0x00, // 12-15
            0x00,0x00,0x00,0x00, // 16-19
                      // not sure, maybe resend count? or take bus? somewhere between 1 and 5?
            0x00,0x00,0x05,0x00  // 20-23
    };

    memcpy(&msg[13], data,len);
   


    msg[8] = counter; // counter
    counter += 1;

    char chk = 0xff;


    int chk_counter;
    for (chk_counter=0; chk_counter<len; chk_counter++){
        chk = chk - msg[13+chk_counter] & 0xff;
    };
    chk = chk + 1;
    msg[13+chk_counter] = chk;

 
    printf("sending: ");
    for (print_counter=0;print_counter<sizeof(msg); print_counter++){
                printf("%02x", msg[print_counter] & 0xff);
    }
    printf("\n");   

    tp_last = tp_current.tv_sec;

    err = ioctl(fd, 3, &msg);
    printf("sent - driver replied with (0 is good): %d\n", err);

}


int main(int argc, char *argv[])
{
    int poll_result;
    char buf[11];
    int err;
    int x;
    int t_result;
    
    clockid_t clk_id;

    

    if (argc != 2)
    {
        printf("Not enough args. Usage ./busserver {PID}");
        return 1;
    }

    pid_to_hook = atoi(argv[1]);


    fd = open(DRIVER_PATH, 2);

    if (fd < 0)
    {
        printf("Cannot open device file...\n");
        return;
    }

    if (-1 == (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)))
	{
		printf("Could not create socket\n");
        exit(1);
	}

    
    
    broadcast_address.s_addr = 0xffffffff;
    memset((char *) &si_broadcast, 0, sizeof(si_broadcast));
    si_broadcast.sin_family = AF_INET;
    si_broadcast.sin_addr = broadcast_address;
    si_broadcast.sin_port = PORT;
     // broadcast address

    int broadcastEnable=1;
    int ret=setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (ret == -1) {
        printf("could not enable broadcast\n");
    }

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    if (-1 == bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ))
	{
		printf("could not bind to port - maybe in use?\n");
        exit(1);
	}
    



    char data[11] = { 

    };

    ptrace(PTRACE_ATTACH, pid_to_hook, NULL, NULL);
    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACECLONE);
    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACESYSGOOD);
    
    printf("starting worker loop\n");

    sfds[0].fd = s;
    sfds[0].events = POLLIN;
    while (1){
        t_result = clock_gettime(CLOCK_MONOTONIC, &tp_current);
        poll_result = poll(sfds,1,1);
        if (sfds[0].revents & POLLIN){
            if ((recv_len = recvfrom(s, &data, sizeof(data), 0, (struct sockaddr *) &si_other, &slen)) == -1)
            {
                printf("udp rx issue? maybe too long?\n");
            } else {
                send_bus(data, recv_len);
            }
        }
        get_responses();
    }

    //send_bus(test, sizeof(test));


    // ptrace(PTRACE_ATTACH, pid_to_hook, NULL, NULL);
    // ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACECLONE);
    

    // ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACESYSGOOD);
    // ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
    // waitpid(pid_to_hook, NULL, 0); // stop ps_gtl to stop interference
    // printf("Attached to pid\n");




   // ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);

 // TODO ACCEPT MSG HERE
 // TODO RX 
 // TODO SEND BUS
 // TODO adjust get response to send to sock get_response();    
    usleep(100);

}