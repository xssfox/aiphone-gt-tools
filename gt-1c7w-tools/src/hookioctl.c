#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

int main(int argc, char *argv[])
{
    struct pt_regs regs;
    unsigned int atdata;
    int pid_to_hook, y;

    if (argc != 2)
    {
        printf("Not enough args. Usage ./hookioctl {PID}");
        return 1;
    }

    pid_to_hook = atoi(argv[1]);
    // kill(pid_to_hook, SIGSTOP);
    // printf("Sent stop to pid\n");

    printf("Pid stopped\n");

    ptrace(PTRACE_ATTACH, pid_to_hook, NULL, NULL);
    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACECLONE);
    printf("Attached to pid\n");

    ptrace(PTRACE_SETOPTIONS, pid_to_hook, NULL, (void *)PTRACE_O_TRACESYSGOOD);

    // ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);

    int status;
    ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
    printf("starting trace\n");
    while (1)
    {

        if (waitpid(pid_to_hook, NULL, WNOHANG))
        {

            // if (WIFEXITED(status)){
            //     printf("Monitored process terminated\n");
            //     exit(1);
            // }

            // if( WIFSTOPPED(status) && WSTOPSIG(status) & 0x80 ){
            //     printf("process stopped\n");
            //     //continue;
            // }

            ptrace(PTRACE_GETREGS, pid_to_hook, NULL, &regs);
            // printf("%x %x\n", regs.regs[3], regs.regs[4]);
            if (regs.regs[3] == 0x36 && regs.regs[4] == 4)
            {

                // //int syscall = regs.orig_rax;
                int data;

                for (y = 0; y < 32; y++)
                {
                    atdata = ptrace(PTRACE_PEEKDATA, pid_to_hook, regs.regs[11] + (y * 4), NULL);
                    printf(" %x", atdata);
                }
                printf("\n");
            }
            // ptrace(PTRACE_CONT, pid_to_hook, NULL, NULL);

            // return 0;
            //  ptrace(PTRACE_CONT, pid_to_hook, NULL, NULL);

            // printf("continued\n");
            ptrace(PTRACE_SYSCALL, pid_to_hook, NULL, NULL);
        }
    }
}