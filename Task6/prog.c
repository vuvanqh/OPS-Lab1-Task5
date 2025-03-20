#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_MESSAGE 50
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct 
{
    int hp;
    int attack;
    int id;
    int next;
    int read;
}brigade_t;

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void child_work(brigade_t*brigade, int fdw, int fdr)
{
    char buf[50];
    snprintf(buf,sizeof(buf),"Brigade[%d]: %d %d ANGARD!!!\n",getpid(),brigade->hp, brigade->attack);
    if(write(fdw,buf,sizeof(buf))<0) ERR("write");
}
void create_brigade(brigade_t* brigade,int fd[3][2],int B[2][2])
{
    int hp = 10;
    int attack = 10;
    brigade[0].next = B[0][1];
    brigade[0].read = 0;
    brigade[1].next = B[1][1];
    brigade[1].read = B[0][0];
    brigade[2].next = 0;
    brigade[2].read = B[1][0];
    for(int k=0;k<3;k++)
    {
        int x=fork();
        switch(x)
        {
            case 0:
                srand(getpid());
                for(int i=0;i<3;i++)
                {
                    if(i<2 && B[i][0]!=brigade[k].read) close(B[i][0]);
                    if(i<2 && B[i][1]!=brigade[k].next) close(B[i][1]);
                    if(i!=k) 
                    {
                        close(fd[i][0]);
                        close(fd[i][1]);
                    }
                }
                brigade[k].hp = 10 + rand()%hp;
                brigade[k].attack = 10 + rand()%attack;

                child_work(&brigade[k],fd[k][1],fd[k][0]);
                close(fd[k][1]);
                close(fd[k][0]);
                close(brigade[k].read);
                close(brigade[k].next);
                exit(0);
            case -1: ERR("pipe");
            default: brigade[k].id = x;
        }
        //close(fd[k][1]);
    }
}

void parent_work(brigade_t*brigade,int fd[3][2])
{
    for(int i=0;i<3;i++)
    {
        int res;
        char buf[50];
        if((res=TEMP_FAILURE_RETRY(read(fd[i][0],buf,sizeof(buf))))<0)
        {
            ERR("read");
        }
        printf("%s\n",buf);
    }
    for(int i=0;i<3;i++)
    {
        close(fd[i][0]);
        close(fd[i][1]);
    }
}

int main(int argc,char**argv)
{
    int fd[3][2],B[2][2];

    pipe(B[1]);
    pipe(B[0]);
    for(int i=0;i<3;i++)
        pipe(fd[i]);
    brigade_t brigade[3];
    sethandler(SIG_IGN,SIGPIPE);
    create_brigade(brigade,fd,B);
    
    parent_work(brigade,fd);
    for(int i=0;i<3;i++)
    {
        close(B[i][0]);
        close(B[i][1]);
    }
    while(wait(NULL)>0);
    return 0;
}