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
sig_atomic_t volatile last_sig=0;
typedef struct
{
    int* cards;
    int suits[4];
    int id;
    int m;
}player_t;

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s fifo_file file\n", name);
    exit(EXIT_FAILURE);
}


int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sig_handler(int sig)
{
    last_sig=SIGINT;
}

void child_work(player_t*player,int fdr,int fdw,int fdn)
{
    srand(getpid());
    player->cards = calloc(sizeof(int),player->m);
    for(int i=0;i<player->m;i++)
    {
        if(last_sig==SIGINT) break;
        int card;
        int res;
        if((res=TEMP_FAILURE_RETRY(read(fdr,&card,sizeof(card))))<0)
        {
            if(errno==EPIPE) break;
            ERR("read");
        }
        if(res==0) 
        {
            printf("lol\n");
            break;
        }
        player->cards[i] = card;
        player->suits[card%4]++;
        //printf("ok");
        //printf("%d %d\n",getpid(),card);
    }
    while (1)
    {
        if(last_sig==SIGINT) break;
        int idx = rand()%player->m;
        player->suits[player->cards[idx]%4]--;
        if(TEMP_FAILURE_RETRY(write(fdn,&player->cards[idx],sizeof(int)))<0) 
        {
            if(errno==EPIPE) break;
            ERR("pipe");
        }
        int res;
        //printf("waiting...\n");
        if((res=TEMP_FAILURE_RETRY(read(fdr,&player->cards[idx],sizeof(int))))<0)
        {
            if(errno==EPIPE) break;
            ERR("pipe");
        }
        player->suits[player->cards[idx]%4]++;
        if(res==0) break;
        char buf[256];
        snprintf(buf,sizeof(buf),"Player[%d]: ",getpid());
        for(int i=0;i<player->m;i++)
        {
            snprintf(buf+strlen(buf),5,"%d, ",player->cards[i]);
            //printf("%s\n",buf);
        }
        printf("%s\n",buf);
        if(player->suits[player->cards[idx]%4]==player->m) 
        {
            int x = getpid();
            printf("[%d|%d]: My ship sails!\n",getpid(),player->cards[idx]%4);
            if(TEMP_FAILURE_RETRY(write(fdw,&x,sizeof(int)))<0) ERR("write");
            break;
        }
    }
    free(player->cards);
    close(fdr);
    close(fdw);
    close(fdn);
}

void parent_work(player_t* players,int(*fd)[2],int R,int n,int m)
{
    srand(getpid());
    int cards[52] = {0};
    for (int i=0;i<n;i++)
    {
        if(last_sig==SIGINT) break;
        for(int k=0;k<m;k++)
        {
            int idx;
            do
            {
                idx = rand()%52;
            } while (cards[idx]==1);
            cards[idx]=1;
            if(TEMP_FAILURE_RETRY(write(fd[i][1],&idx,sizeof(idx)))<0)
            {
                if(errno==EPIPE)
                {
                    break;
                } 
                ERR("write");
            }
        }
    }
    int res;
    int winner;
    fcntl(R, F_SETFL, O_NONBLOCK);
    while(1)
    {
        if(last_sig==SIGINT) break;
        if((res=TEMP_FAILURE_RETRY(read(R,&winner,sizeof(int))))<0)
        {
        // if(errno==EPIPE) break;
            if(errno==EAGAIN) continue;
            ERR("pipe");
        }
        else break;
    }
    if(last_sig!=SIGINT)printf("Server: [%d] won!\n",winner);
    for(int i=0;i<n;i++)
        close(fd[i][1]);
    close(R);
}

void create_players(player_t*players, int(* fd)[2], int R, int n, int m)
{
    for(int i=0;i<n;i++)
    {
        int x=fork();
        players[i].m = m;
        switch (x)
        {
            case 0:
                for(int k=0;k<n;k++)
                {
                    if(k!=i)close(fd[k][0]);
                    if(k!=(i+1)%n)close(fd[k][1]);
                }

                child_work(&players[i],fd[i][0],R,fd[(i+1)%n][1]);
                free(fd);
                free(players);
                exit(0);
            case -1: ERR("fork");
            default: players[i].id = x;
        }
        close(fd[i][0]);
    }
}

//n-players
//m-cards per player
int main(int argc, char**argv)
{
    if(argc!=3) usage(argv[0]);
    int n=atoi(argv[1]);
    int m=atoi(argv[2]);
    if(n<4 || n>7 || m<4 || m*n>52) usage(argv[0]);
    sethandler(SIG_IGN,SIGPIPE);
    sethandler(sig_handler,SIGINT);
    int(* fds)[2], R[2];
    fds = malloc(sizeof(int)*n);
    if(pipe(R)<0) ERR("pipe");
    for(int i=0;i<n;i++)
        if(pipe(fds[i])<0) ERR("pipe");

    player_t* players = calloc(sizeof(player_t),n);

    create_players(players,fds,R[1],n,m);
    close(R[1]);
    parent_work(players,fds,R[0],n,m);

    while(wait(NULL)>0);
    free(fds);
    free(players);
    return 0;
}