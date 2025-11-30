#define _POSIX_C_SOURCE 200112L
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <inttypes.h>
#ifdef X128
#include <emmintrin.h>
#endif
#include <time.h>


/*

Will need a restart to resume intercom operation


this no longer required
kill $(pidof ps_wdt)
kill $(pidof ps_update)
kill $(pidof prm_log)
kill $(pidof ps_hardware)
rmmod wdtdrv
kill -9 $(pidof ps_gtl)
rmmod tscom
/mnt/sdcard0/scanserial
*/

uint64_t target = 0xffe20000;

#define PORT 4444

struct sockaddr_in si_me, si_other;

int s, i, slen = sizeof(si_other) , recv_len;
struct sockaddr_in si_broadcast;
struct in_addr broadcast_address;


char one_byte;
char read_already=0;
void *map_base;

struct pollfd sfds[1];

void main(){
    int poll_result;
    char buf[11];
    int err;
    int x;
    int fd;
    unsigned int pagesize = (unsigned)sysconf(_SC_PAGESIZE);
    fd = open("/dev/mem", O_RDWR | O_SYNC);

    printf("%x\n" ,target & ~((typeof(target))pagesize - 1));

    if (fd == -1)
    {
        printf("Error opening /dev/mem (%d) : %s\n", errno, strerror(errno));
        exit(1);
    }
    map_base = mmap(0, 0x90000, PROT_READ | PROT_WRITE, MAP_SHARED,  fd, target & ~((typeof(target))pagesize - 1));
    if (map_base == (void *)-1)
    {
        printf("Error mapping (%d) : %s\n", errno, strerror(errno));
        exit(1);
    }
    unsigned int batch = 0;

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
    

    sfds[0].fd = s;
    sfds[0].events = POLLIN;


    // disable receive interupt
    *(volatile short *)(map_base+0x08) = (*(volatile short *)(map_base+0x08))  & 0x303f; // & 0xFFBF;
    //timers disable
    // *(volatile short *)(map_base+0x8fff0) = (*(volatile short *)(map_base+0x8fff0)) & 0xFFDF;
    // *(volatile short *)(map_base+0x8ffe4) = (*(volatile short *)(map_base+0x8ffe4)) & 0xFFDF;
    char data[11*5]; 
    while(1){
        poll_result = poll(sfds,1,0);
        if (sfds[0].revents & POLLIN){
            printf("\n");
            if ((recv_len = recvfrom(s, &data, sizeof(data)-1, 0, (struct sockaddr *) &si_other, &slen)) == -1)
            {
                printf("udp rx issue? maybe too long?\n");
            } else {
                if (recv_len < 5){ // offset by 1 as first byte is always 0x00
                    printf("Packet to short\n");
                    return;
                }
                if (recv_len > 9){ // leave one data byte for checksum
                    printf("Packet to long\n");
                    return;
                }

                char chk = 0xff;
                int chk_counter;
                for (chk_counter=0; chk_counter<recv_len; chk_counter++){
                    chk = chk - data[chk_counter] & 0xff;
                };
                chk = chk + 1;
                data[chk_counter] = chk;
                recv_len= recv_len+1; // make room for checksum
                printf("sending: ");
                int print_counter;
                for (print_counter=0;print_counter<recv_len; print_counter++){
                            printf("%02x", data[print_counter] & 0xff);
                }
                printf("\n");
                int pos = 0 ;
                for ( pos=0;pos<recv_len;pos++){
                    while ((*(volatile short *)(map_base+0x10) & 0x20) != 0x20){
                        usleep(10);
                    }
                    *(volatile unsigned char *)(map_base+0x0c) = data[pos];
                    *(volatile short *)(map_base+0x10) = *(volatile short *)(map_base+0x10) & 0xff9f; // clear tdfe and tend
                }
                printf("\n");
                while ((*(volatile short *)(map_base+0x10) & 0x40) != 0x40){
                    usleep(10);
                }
                printf("TEND detected - serial transmission should be done\n");
               // *(volatile short *)(map_base+0x08) = *(volatile short *)(map_base+0x08) & 0xffdf;  // lcear te
            }
        }
        if ((*(volatile short *)(map_base+0x10) & 0x2) == 0x2){
            if (batch > 10000) {
                printf("\n");
            }
            one_byte = *(volatile unsigned char *)(map_base +0x14) ;
            //*(volatile char *)(map_base + 0x14) = 0x00;
            *(volatile short *)(map_base+0x10) = (*(volatile short *)(map_base+0x10)) & 0xff6c;
            *(volatile short *)(map_base+0x24) = (*(volatile short *)(map_base+0x24)) & 0xfffe;

            printf("%02x", one_byte & 0xff);
            fflush(stdout);
            batch = 0;
        } else {
            batch += 1;
        }
    }
    printf("\n");
}