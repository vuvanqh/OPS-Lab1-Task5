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

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s fifo_file file\n", name);
    exit(EXIT_FAILURE);
}

typedef struct
{
    pid_t id;
    int money;
}player_t;

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void child_work(player_t* player,int fdr,int fdw)
{
    srand(getpid());
    int money = 1+rand()%player->money;
    printf("[%d]: I have [%d] and I'm going to play roulette.\n",getpid(),player->money);
    char buf[MAX_MESSAGE];
    while(player->money>0)
    {
        int money = 1+rand()%(player->money);
        snprintf(buf,sizeof(buf),"%2d%d",rand()%37,money);
        //printf("%s\n",buf);
        if(TEMP_FAILURE_RETRY(write(fdw,buf,sizeof(buf)))<0) ERR("write");
        int wins;
        int res;
        if((res=TEMP_FAILURE_RETRY(read(fdr,&wins,sizeof(wins))))<0) ERR("write");
        if(res==0) break;
        if(wins>0) printf("[%d]: I won [%d]\n",getpid(),wins);
        //else printf("[%d]: I lost\n",getpid());
        player->money+=wins;
        if(player->money<=0)
        {
            printf("[%d]: I'm broke\n",getpid());
            break;
        }
    }
}

void create_players(player_t*players,int n,int m,int**fds)
{
    for(int i=0;i<n;i++)
    {
        fds[i] = calloc(sizeof(int),2);
        if(pipe(fds[i])<0) ERR("pipe");

        players[i].money=m;
        int x=fork();
        switch (x)
        {
            case -1: ERR("fork");
            case 0:
                for(int k=0;k<i;k++)
                {
                    close(fds[k][0]);
                    close(fds[k][1]);
                    free(fds[k]);
                }
                players[i].id = getpid();
                child_work(&players[i],fds[i][0],fds[i][1]);
                
                close(fds[i][1]);
                close(fds[i][0]);
                free(fds[i]);
                free(fds);
                free(players);
                exit(0);
            default: players[i].id = x; break;
        }
        //fcntl( fds[i][0], F_SETFL, fcntl(fds[i][0], F_GETFL) | O_NONBLOCK);
    }
}

void parent_work(player_t*players,int n, int**fds)
{
    srand(getpid());
    int count=0;
    int inGame=n;
    int*bets=calloc(sizeof(int),n);
    int*moneys = calloc(sizeof(int),n);
    while(inGame>0)
    {
        //printf("waiting for read\n");
        for(int i=0;i<inGame;i++)
        {
            int res;
            char buf[MAX_MESSAGE];
            if((res=TEMP_FAILURE_RETRY(read(fds[i][0],buf,sizeof(buf))))<0)
            {
                if(errno==EAGAIN)
                {
                    errno=0;
                    continue;
                }
                if(errno==EPIPE) 
                {
                    player_t*temp = &players[inGame-1];
                    players[inGame-1] = players[i];
                    players[i] = *temp;
                    inGame--;
                    break;
                }
                ERR("read");
            }
            if(res == 0) {
                printf("Dealer: [%d] is out of money and left the game.\n", players[i].id);
                close(fds[i][0]);
                close(fds[i][1]);
                free(fds[i]);
            
                players[i] = players[inGame - 1];
                fds[i] = fds[inGame - 1];
                inGame--;
                i--; 
                continue;
            }
            char bet[3]={0};
            char money[MAX_MESSAGE];
            snprintf(bet,sizeof(bet),"%s",buf);
            snprintf(money,sizeof(money),"%s",buf+2);
            moneys[i] = atoi(money);
            bets[i] = atoi(bet);
            if(moneys[i]==0)
            {
                inGame--;
                player_t*temp = &players[inGame-1];
                    players[inGame-1] = players[i];
                    players[i] = *temp;
                    inGame--;
                    continue;
                    i--;
            }
            printf("Dealer: [%d] placed [%d] on [%d]\n",players[i].id,atoi(money),atoi(bet));
            count++;
        }
        if(count==inGame)
        {
            int nombre = rand()%37;
            printf("Dealer: [%d] is the lucky number\n",nombre);
            for(int i=0;i<inGame;i++)
            {
                printf("%d\n",inGame);
                int wins = 0;
                if(bets[i]==nombre)
                    wins = 35*moneys[i];
                else
                    wins = (-35)* moneys[i];
                
                if(TEMP_FAILURE_RETRY(write(fds[i][1],&wins,sizeof(wins)))<0) ERR("write");
            }
            count=0;
            printf("\n");
        }
    }   
    printf("Dealer: Casino always wins\n");
    free(bets);
    free(moneys);
}

//n = players
//m = starting money
// [0, 36] with a 35:1
int main(int argc, char** argv)
{
    if(argc!=3) usage(argv[0]);
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if(n<1 || m<36) usage(argv[0]);
    sethandler(SIG_IGN,SIGPIPE);
    player_t* players = calloc(sizeof(player_t),n);
    int** fds = calloc(sizeof(int*),n);
    create_players(players,n,m,fds);

    parent_work(players,n,fds);
    while (wait(NULL) > 0);
    free(players);
    for(int i=0;i<n;i++)
    {
        free(fds[i]);
    }
    free(fds);
    return 0;
}