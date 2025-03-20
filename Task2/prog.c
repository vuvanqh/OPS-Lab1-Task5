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
#define PIPE_SIZE 16

int max(int*arr,int* winner,int n)
{
    int maxi=-1;
    for(int i=0;i<n;i++)
        if(maxi<arr[i] && winner[i]!=-1) maxi=arr[i];
    return maxi;
}

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

void child_work(int fdr,int fdw,int M,int id)
{
    srand(getpid());

    int* cards = calloc(sizeof(int),M);
    for(int i=0;i<M;i++)
        cards[i] = i+1;
    char buf[PIPE_SIZE];
    int points=0;
    //printf("%d %d %d\n",fdr,fdw,getpid());
    for(int i=0;i<M;i++)
    {
        if((rand()%20)==0) 
        {
            printf("Player%d left the chat...\n",id);
            break;
        }
        if(errno==EPIPE) break;

        int res;

        if((res=TEMP_FAILURE_RETRY(read(fdr,buf,sizeof(buf))))<0) ERR("read");
    
        if(res==0) break;

        int x = rand()%(M-i);
        if(TEMP_FAILURE_RETRY(write(fdw,&cards[x],sizeof(int)))<0) 
        {
            if(errno==EPIPE) break;
            ERR("write");
        }
        for(int k=x+1;k<M-i;k++)
            cards[k-1] = cards[k];
        if(M-i-1 == 0) break;
        if((cards=realloc(cards,sizeof(int)*(M-i-1)))==NULL) ERR("realloc");

        res=0;
        if((res=TEMP_FAILURE_RETRY(read(fdr,buf,sizeof(buf))))<0) 
        {
            if(errno==EPIPE) break;
            ERR("read");
        }
        if(res==0) break;
    }
    close(fdr);
    close(fdw);
    free(cards);
    exit(0);
}

void parent_work(int* fdw, int* fdr, int n, int M)
{
    char buf[PIPE_SIZE];
    int* cards = calloc(sizeof(int),n);
    int* winner = calloc(sizeof(int),n);
    int* points = calloc(sizeof(int),n);
    int inGame=n;
    for(int k=0;k<M;k++)
    {
        if(inGame==1) break;
        int sum=0;
        printf("NEW ROUND: %d\n",k+1);
        snprintf(buf,sizeof(buf),"new_round");
        for(int i=0;i<n;i++)
        {
            if (winner[i] == -1) continue;
            if(TEMP_FAILURE_RETRY(write(fdw[i],buf,sizeof(buf)))<0) 
            {
                if(errno==EPIPE)
                {
                    inGame--;
                    winner[i]=-1;
                    continue;
                }
                else ERR("write");
            }
            int res;
            int num;
            if((res = TEMP_FAILURE_RETRY(read(fdr[i],&num,sizeof(num))))<0)
            {
                if(errno==EPIPE)
                {
                    winner[i]=-1;
                    inGame--;
                    continue;
                }
                else ERR("read");
            }
            if(res==0)
                break;
            printf("Got number %d from player %d\n",num,i+1);
            cards[i] = num;
        }
        int maxi = max(cards,winner,n);
        for(int i=0;i<n;i++)
            if(cards[i]==maxi && winner[i]!=-1)
            {
                sum++;
                winner[i]=1;
            }
        printf("MAX NUM IN THIS ROUND: %d\n",maxi);
        for(int i=0;i<n;i++)
        {
            if(winner[i]==1)
            {
                snprintf(buf,sizeof(buf),"%d",n/sum);
                if(TEMP_FAILURE_RETRY(write(fdw[i],buf,sizeof(buf)))<0) 
                {
                    if(errno==EPIPE) continue;
                    ERR("write");
                }
                winner[i]=0;
                points[i]+=n/sum;
            }
            else if(winner[i]!=-1)
            {
                snprintf(buf,sizeof(buf),"%d",0);
                if(TEMP_FAILURE_RETRY(write(fdw[i],buf,sizeof(buf)))<0) 
                {
                    if(errno==EPIPE) continue;
                    ERR("write");
                }
            }
        }
        puts("\n");
    }
    for(int i=0;i<n;i++)
    {
        printf("Player%d: %d points\n",i+1,points[i]);
    }
    for(int i=0;i<n;i++)
    {
        close(fdw[i]);
        close(fdr[i]);
    }
    free(cards);
    free(winner);
    free(points);
}
void create_children(int*fdw,int*fdr,int n,int M)
{
    int t_client[2];
    int t_server[2]; //for server to write
    for(int i=0;i<n;i++)
    {
        if(pipe(t_client)) ERR("pipe");
        if(pipe(t_server)) ERR("pipe");
        switch(fork())
        {
            case -1: ERR("fork");
            case 0:
                for(int k=0;k<i;k++)
                {
                    close(fdw[k]);
                    close(fdr[k]);
                }
                free(fdw);
                free(fdr);
                close(t_server[1]);
                close(t_client[0]);
                child_work(t_server[0],t_client[1],M,i+1);
                close(t_client[1]);
                close(t_server[0]);
                exit(0);
        }
        close(t_server[0]);
        close(t_client[1]);
        fdw[i] = t_server[1];
        fdr[i] = t_client[0];
    }
}

//n = #child processes/players
//M = #cards numbered from 1 to M
//0 if loss
//n points if win / distribute the points if needed
int main(int argc,char**argv)
{
    if(argc!=3) usage(argv[0]);
    int n = atoi(argv[1]);
    int M = atoi(argv[2]);
    if(n<2 || n>5 || M<5 || M>10) usage(argv[0]);
    sethandler(sigchld_handler,SIGCHLD);
    sethandler(SIG_IGN,SIGPIPE);

    int *fdw,*fdr;
    if((fdw = (int*)malloc(sizeof(int)*n))==NULL) ERR("malloc");
    if((fdr = (int*)malloc(sizeof(int)*n))==NULL) ERR("malloc");

    create_children(fdw,fdr,n,M);
    parent_work(fdw,fdr,n,M);
    free(fdw);
    free(fdr);
    return 0;
}