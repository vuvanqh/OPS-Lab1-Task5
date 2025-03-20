#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s fifo_file file\n", name);
    exit(EXIT_FAILURE);
}
volatile sig_atomic_t last_signal = 0;

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sigchld_handler(int sig)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (0 == pid)
            return;
        if (0 >= pid)
        {
            if (ECHILD == errno)
                return;
            ERR("waitpid:");
        }
    }
}

void child_work(int fdr,int fdw)
{
    fputs("WORK\n",stdout);
    int num;
    char buf[256]={};
    srand(getpid());
    for(;;)
    {
        if(errno==EPIPE)
            break;
        int res = 0;
        if((res = TEMP_FAILURE_RETRY(read(fdr,&buf,sizeof(buf))))<=0)
        {
            if(res<0) ERR("read");
            if(res==0 || errno==EPIPE) 
            {
                printf("naura\n");
                break;
            }
        }
        num =atoi(buf);
        printf("%2d %-5d \n",num,getpid());
        if(num==0)
        {
            break;
        }
        int x = rand()%20-10;
        num+=x;
        snprintf(buf,sizeof(buf),"%d",num);
        if(TEMP_FAILURE_RETRY(write(fdw,&buf,sizeof(buf)))<0) ERR("write");
    }
    close(fdr);
    close(fdw);
    exit(0);
}

void parent_work(int fdr,int fdw)
{
    int num = 1;
    int i=1;
    char buf[256];
    for(;;)
    {
        if(errno==EPIPE)
            break;
        if(i++%10==0) num=0; //zeby nie bylo w nieskonczonosc
        snprintf(buf,sizeof(buf),"%d",num);
        if(TEMP_FAILURE_RETRY(write(fdw,&buf,sizeof(buf)))<0) ERR("write");
        num = atoi(buf);
        int res = 0;
        if((res = TEMP_FAILURE_RETRY(read(fdr,&buf,sizeof(buf))))<=0)
        {
            if(res<0) ERR("read");
            if(res==0) 
            {
                printf("naura\n");
                break;
            }
        }
        printf("%2d %-5d [PARENT]\n",num,getpid());
        if(num==0)
        {
            break;
        }
        int x = rand()%21-10;
        num+=x;
        sleep(1);
    }
    close(fdr);
    close(fdw);
    exit(0);
}
void create_children(int**p,int n)
{
    for(int i=1;i<n;i++)
    {
        switch(fork())
        {
            case 0:
            for (int j = 0; j < n; j++)
                {
                    if (j != i) close(p[j][0]);  
                    if (j != (i + 1) % n) close(p[j][1]); 
                }
                child_work(p[i][0],p[(i+1)%n][1]);
                exit(0);
            case -1: ERR("fork");
        }
        close(p[i][0]);
        if(i!=1) close(p[i][1]);
    }
    close(p[0][1]);
}

/*
0-parent read
1-parent write = (i+1)%n

*/
int main(int argc,char**argv)
{
    if(argc!=2) usage(argv[0]);
    int n = atoi(argv[1]);
    int **p;
    if((p=(int**)malloc(sizeof(int*)*n))==NULL) ERR("malloc");

    for(int i=0;i<n;i++)
    {
        if((p[i]=(int*)malloc(sizeof(int)*2))==NULL) ERR("malloc");
        if(pipe(p[i])<0) ERR("pipe");
    }

    sethandler(sigchld_handler,SIGCHLD);
    sethandler(SIG_IGN,SIGPIPE);

    create_children(p,n);
    parent_work(p[0][0],p[1][1]);

    for(int i=0;i<n;i++)
        free(p[i]);
    free(p);
    printf("end\n");
    return 0;
}