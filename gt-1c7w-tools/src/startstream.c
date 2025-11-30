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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 5555

#define H264ENC_PORT "/tmp/.socket/h264enc_port"
#define RECSET_PORT "/tmp/.socket/recset_port"
// #define APPLI_PORT "/tmp/.socket/appli_port"

struct sockaddr_in si_me, si_other;
int s, i, slen = sizeof(si_other), recv_len;

struct sockaddr_in si_broadcast;
struct in_addr broadcast_address;

char newName[255];
struct sockaddr_un svaddr_h264, svaddr_recset, claddr, rxaddr, svaddr_appli;

#define WAIT_TIME 1 // in seconds
int t_result;
time_t tp_last;
struct timespec tp_current;

// char cmd_call[103] = {
//  //   0    1     2      3     4     5    6      7    8    9       a     b   c      d     e    f
//    0x2, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
// };

// record
char cmd_call[103] = {
    //   0    1     2      3     4     5    6      7    8    9       a     b   c      d     e    f
    0x2,
    0xfd,
    0x6,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

char cmd_rec_set[103] = {
  0x05, 0x00, 0x14
// 0x00, 0x04, 0x06, 0x00
};

char cmd_encode[103] = {
    0x05, 0x05, 0x01, 0x01 
};


//05 05 01 01 

// char cmd_appli[103] = {
//  //   0  1       2      3     4     5    6      7    8    9       a     b   c      d     e    f
//     0x4,  0x02,  0x1b,  0x01, 0x01, 0x01,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
// };
// char cmd_appli_2[103] = {
//  //   0  1       2      3     4     5    6      7    8    9       a     b   c      d     e    f
//     0x03, 0x01,  0x1b,  0xc1, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
// };
// char cmd_appli_3[103] = {
//  //   0  1       2      3     4     5    6      7    8    9       a     b   c      d     e    f
//     0x0, 0x03,  0x1b,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
// };

/* assumes ram set already
0000 0000 0000 0000 0000 0000 0000 0000
0000 0000 0000 0000 3331 322e 3933 362e
2e32 3234 0000 0000 0000 0000 0000 0000
0000 0000 0000 0000 0000 0000 0000 0000
330f 0000 7fde 540c 15ce 798e 92ef a6fe
5491 e1e2 9dfa 1273 4873 1073 ccd0 c14a
98fb dc19 1518 8d86 6b4d 3731 2e32 3631
302e 312e 0000 0000 0000 0000 0000 0000
0000 0000 0000 0000 0000 0000 0000 0000
0000 9151 0000 0000 0000 0000 0000 0000
0000 0000 0000 0000 0000 0000 0000 0000
*/

int pid_to_hook;

void singalHandler(int sig){
    ptrace(PTRACE_CONT, pid_to_hook, NULL, NULL);
    ptrace(PTRACE_DETACH, pid_to_hook, NULL, NULL);
    exit(1);
}



int main(int argc, char *argv[])
{
    int fd, shm;
    char *shmp;
    struct pt_regs regs;

    fd = socket(AF_UNIX, SOCK_DGRAM, 1);

    // shm = open("/tmp/snd_media_info_table.ram", 0x42, 0600);
    // if (shm == -1)
    // {
    //     printf("shm failed  %d\n", errno);
    //     return 1;
    // }

    // shmp = mmap(NULL, 176, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    // if (shmp == MAP_FAILED)
    // {
    //     printf("mmap failed : %d %d\n", errno, shm);
    //     return 1;
    // }
    snprintf(newName, sizeof(newName), "/tmp/h264enc-tmp.%ld", (long)getpid());

    // // in theory we should wait for semophores or something but meh
    // memset(shmp + 0x18, 0, 176);
    // snprintf(shmp + 0x18, 40, "172.16.0.47");
    // shmp[64] = 0x12;
    // shmp[65] = 0x34;

    // snprintf(shmp + 0x6A, 40, "172.16.0.1");
    // shmp[0x92] = 0x34;

    svaddr_h264.sun_family = AF_UNIX;
    strncpy(svaddr_h264.sun_path, H264ENC_PORT, sizeof(svaddr_h264.sun_path) - 1);

    svaddr_recset.sun_family = AF_UNIX;
    strncpy(svaddr_recset.sun_path, RECSET_PORT, sizeof(svaddr_recset.sun_path)-1);

    // svaddr_appli.sun_family = AF_UNIX;
    // strncpy(svaddr_appli.sun_path, APPLI_PORT, sizeof(svaddr_appli.sun_path)-1);

    claddr.sun_family = AF_UNIX;
    snprintf(claddr.sun_path, sizeof(claddr.sun_path), "%s", newName);

    if (bind(fd, (struct sockaddr *)&claddr, sizeof(struct sockaddr_un)) == -1)
    {
        printf("bind error\n");
        exit(1);
    }



    // sendto(fd, cmd_appli, sizeof(cmd_appli),0,(struct sockaddr  *) &svaddr_appli, sizeof(struct sockaddr_un));
    // sendto(fd, cmd_appli_2, sizeof(cmd_appli),0,(struct sockaddr  *) &svaddr_appli, sizeof(struct sockaddr_un));
    // sendto(fd, cmd_appli_3, sizeof(cmd_appli),0,(struct sockaddr  *) &svaddr_appli, sizeof(struct sockaddr_un));
    sendto(fd, cmd_rec_set, sizeof(cmd_rec_set),0,(struct sockaddr  *) &svaddr_recset, sizeof(struct sockaddr_un));
    sendto(fd, cmd_call, sizeof(cmd_call), 0, (struct sockaddr *)&svaddr_h264, sizeof(struct sockaddr_un));
    sendto(fd, cmd_encode, sizeof(cmd_encode), 0, (struct sockaddr *)&svaddr_recset, sizeof(struct sockaddr_un));


    

    if (argc != 2)
    {
        printf("Not enough args. Usage ./hookioctl {PID}");
        return 1;
    }

    if (-1 == (s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)))
    {
        printf("Could not create socket\n");
        exit(1);
    }

    memset((char *)&si_me, 0, sizeof(si_me));
    si_me.sin_addr.s_addr = inet_addr("172.16.0.47");
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);

    // if (-1 == bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ))
    // {
    // 	printf("could not bind to port - maybe in use?\n");
    //     exit(1);
    // }

    if (connect(s, (struct sockaddr *)&si_me, sizeof(si_me)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(1);
    }

    pid_to_hook = atoi(argv[1]);

    ptrace(PTRACE_ATTACH, pid_to_hook, NULL, NULL);
    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACECLONE);
    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACESYSGOOD);
    signal(SIGINT, singalHandler);
    ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
    int last_pointer;
    char fpfilePath[255];
    char fpfilename[255];
    char outdata[40000];
    int write_ptr;
    int r;
    outdata[0] = 0x00;
    outdata[1] = 0x00;
    outdata[2] = 0x00;
    outdata[3] = 0x01;
    write_ptr=4;
    while (1)
    {
        //printf("loop\n");
        ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
        if (waitpid(pid_to_hook, NULL,   __WALL | __WNOTHREAD )) // WNOHANG |
        {
            ptrace(PTRACE_GETREGS, pid_to_hook, NULL, &regs);
            //printf("pid\n");
            // write
            if (regs.regs[3] == 4) // && regs.regs[4] == 8
            {

                // printf("regs\n");
                // printf("regs.regs[4]: %x \n", regs.regs[4]);
                // printf("regs.regs[5]: %x \n--\n", regs.regs[5]);
                // printf("regs.regs[6]: %x \n--\n", regs.regs[6]);
                sprintf(fpfilePath, "/proc/%d/fd/%d", pid_to_hook,regs.regs[4]);
                r = readlink(fpfilePath, fpfilename, sizeof(fpfilename));
                if (r < 0){
                    printf("fail to read link");
                    continue; // unable to name so assume its not us
                }
                if (!strcmp(fpfilePath, "/tmp/movie_enc.mov")){
                    continue;
                }
                //printf("file: %s \n--\n", fpfilename);

                // we assume that zthe buf location is going to change and if we receive the same location twice its likely a duplicate write
                
                if (last_pointer == regs.regs[5]){
                    continue;
                    
                }
                last_pointer = regs.regs[5];

                

                int data[20000];

                int y;
                if (regs.regs[6] > sizeof(data)){
                    continue;
                }
                for (y = 0; y < regs.regs[6] / 4; y++)
                {
                    data[y] = ptrace(PTRACE_PEEKDATA, pid_to_hook, regs.regs[5] + (y * 4), NULL);
                }

                char *byteArray = (char *)data;
                for(y=0;y<regs.regs[6] ; y++){
                    outdata[write_ptr] = byteArray[y];
                    //printf("%d : %x \n", write_ptr, outdata[write_ptr] &0xff);
                    if (write_ptr > 4){
                        if (outdata[write_ptr] == 0x01 && 
                            outdata[write_ptr-1] == 0x00 &&
                            outdata[write_ptr-2] == 0x00 &&
                            outdata[write_ptr-3] == 0x00 
                        ){
                            printf("writing out to socket\n");
                            int err;
                            err = sendto(s, outdata, write_ptr+1, 0, (struct sockaddr *)&si_me, slen);
                            if (err == -1)
                            {
                                printf("failed to send to udp: %d\n", errno);
                            }
                            outdata[0] = 0x00;
                            outdata[1] = 0x00;
                            outdata[2] = 0x00;
                            outdata[3] = 0x01;
                            write_ptr=4;
                            continue;
                        } 
                    };
                    write_ptr=write_ptr+1;
                      
                    if (write_ptr>=sizeof(outdata)){
                        printf("didn't find header in time :/\n");
                        int err;
                        err = sendto(s, &outdata, write_ptr, 0, (struct sockaddr *)&si_me, slen);
                        if (err == -1)
                        {
                            printf("failed to send to udp: %d\n", errno);
                        }
                        outdata[0] = 0x00;
                        outdata[1] = 0x00;
                        outdata[2] = 0x00;
                        outdata[3] = 0x01;
                        write_ptr=4;
                        write_ptr=0;
                    }

                }
                printf("\n");
                


                

                    // sendto(fd, cmd_rec_set, sizeof(cmd_rec_set),0,(struct sockaddr  *) &svaddr_recset, sizeof(struct sockaddr_un));
                    // // sendto(fd, cmd_call, sizeof(cmd_call), 0, (struct sockaddr *)&svaddr_h264, sizeof(struct sockaddr_un));
                    // sendto(fd, cmd_encode, sizeof(cmd_encode), 0, (struct sockaddr *)&svaddr_recset, sizeof(struct sockaddr_un));
                                
            }
        }
        //usleep(10);
        t_result = clock_gettime(CLOCK_MONOTONIC, &tp_current);
        if (tp_current.tv_sec > tp_last + WAIT_TIME ) { 
            tp_last = tp_current.tv_sec;
         //  printf("test\n");
         //   sendto(fd, cmd_rec_set, sizeof(cmd_rec_set),0,(struct sockaddr  *) &svaddr_recset, sizeof(struct sockaddr_un));
         //   sendto(fd, cmd_call, sizeof(cmd_call), 0, (struct sockaddr *)&svaddr_h264, sizeof(struct sockaddr_un));
         //   sendto(fd, cmd_encode, sizeof(cmd_encode), 0, (struct sockaddr *)&svaddr_recset, sizeof(struct sockaddr_un));
        }

        
    }
}


//todo add a ctrl+c sig handler which detactes ptrace