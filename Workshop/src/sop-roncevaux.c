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


#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_KNIGHT_NAME_LENGTH 20


typedef struct player
{
    char name[MAX_KNIGHT_NAME_LENGTH];
    int hp;
    int attack;
    int type;
}player_t;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
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

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void child_work(player_t*player,int(*fw)[2],int fr,int n)
{
    srand(getpid());
    fcntl(fr, F_SETFL, O_NONBLOCK);
    int p = n-1;
    int alive = n;
    while(alive>0)
    {
        int res;
        char hit[20]={0};
        if((res=TEMP_FAILURE_RETRY(read(fr,&hit,sizeof(hit))))==-1)
        {
            if(errno==EPIPE) break;
            else if(errno!=EAGAIN)  ERR("read");
        }

        if(res==0) break;
        player->hp-=atoi(hit);
        if(player->hp<=0) 
        {
            printf("%s dies\n",player->name);
            break;
        }
        //printf("%d\n",atoi(hit));
        int oponent = rand()%p;
        int dmg = 1+rand()%player->attack;
        if(dmg==0)
            printf("%s attacks his enemy, however he deflected\n",player->name);
        else if(dmg<5)
            printf("%s goes to strike, he hit right and well\n",player->name);
        else
            printf("%s strikes powerful blow, the shield he breaks and inflicts a big wound\n",player->name);
        char buf[20];
       
        snprintf(buf,sizeof(buf),"%d",dmg);
        if(TEMP_FAILURE_RETRY(write(fw[oponent][1],buf,sizeof(buf)))<0)
        {
            if(errno==EPIPE) 
            {
                int f = fw[oponent][1];
                fw[oponent][1] = fw[p][1];
                fw[p][1] = f;
                p--;
                alive--; 

            }
            else ERR("write");
        }
        int t = 1+rand()%10;
        msleep(t);
    }
    if(player->type==0) printf("FRANCI WON!!!\n");
    else printf("SARCENI WON!!!\n");
}

void create_children(player_t*franci,player_t*sarceni,int(*fr)[2],int(*fs)[2],int nf, int ns)
{   
    for(int k=0;k<nf;k++)
    {
        franci[k].type = 0;
        switch (fork())
        {
            case 0:
                for(int i=0;i<ns;i++)
                    close(fs[i][0]);
                for(int i=0;i<nf;i++)
                {
                    if(i!=k) close(fr[i][0]);
                    close(fr[i][1]);
                }
                printf("I am Frankish knight %s. I will serve my king with my %d HP and %d attack.\n",franci[k].name,franci[k].hp,franci[k].attack);
                child_work(&franci[k],fs,fr[k][0],ns);

                for(int i=0;i<ns;i++)
                    close(fs[i][1]);
                close(fr[k][0]);
                free(fr);
                free(fs);
                free(franci);
                free(sarceni);
                exit(0);
            case -1: ERR("fork");
            default:
                break;
        }
    }
    for(int k=0;k<ns;k++)
    {
        sarceni[k].type = 0;
        switch (fork())
        {
            case 0:
                for(int i=0;i<nf;i++)
                    close(fr[i][0]);
                for(int i=0;i<ns;i++)
                {
                    if(i!=k) close(fs[i][0]);
                    close(fs[i][1]);
                }
                printf("I am Spanish knight %s. I will serve my king with my %d HP and %d attack.\n",sarceni[k].name,sarceni[k].hp,sarceni[k].attack);
                child_work(&sarceni[k],fr,fs[k][0],nf);

                for(int i=0;i<nf;i++)
                    close(fr[i][1]);
                close(fs[k][0]);
                free(fr);
                free(fs);
                free(franci);
                free(sarceni);
                exit(0);
            case -1: ERR("fork");
            default:
                break;
        }
    }
}

int main(int argc, char* argv[])
{
    printf("Opened descriptors: %d\n", count_descriptors());
    srand(time(NULL));
    FILE* fr =fopen("franci.txt", "r"); 
    FILE* fs =fopen("saraceni.txt", "r"); 
    int(*fds)[2],(*fdr)[2];
    int nf,ns;

    fscanf(fr,"%d",&nf);
    fscanf(fs,"%d",&ns);
    printf("%d %d\n",nf,ns);
    if((fds = malloc(sizeof(int)*ns))==NULL) ERR("malloc");
    if((fdr = malloc(sizeof(int)*nf))==NULL) ERR("malloc");

    set_handler(SIG_IGN,SIGPIPE);

    player_t* franci = calloc(sizeof(player_t),nf); //
    player_t* sarceni = calloc(sizeof(player_t),ns);//
    
    for(int i=0;i<nf;i++)
    {
        if(pipe(fdr[i])<0) ERR("pipe");
        int hp,attack;
        fscanf(fr,"%s %d %d",franci[i].name,&hp,&attack);
        franci[i].attack=attack;
        franci[i].hp=hp;
    }

    for(int i=0;i<ns;i++)
    {
        if(pipe(fds[i])<0) ERR("pipe");
        int hp,attack;
        fscanf(fs,"%s %d %d",sarceni[i].name,&hp,&attack);
        sarceni[i].attack=attack;
        sarceni[i].hp=hp;
    }
    fclose(fs);
    fclose(fr);

    create_children(franci,sarceni,fdr,fds,nf,ns);
    for(int i=0;i<nf;i++)
    {
        close(fdr[i][0]);
        close(fdr[i][1]);
    }
    for(int i=0;i<ns;i++)
    {
        close(fds[i][0]);
        close(fds[i][1]);
    }


    while(wait(NULL)>0);
    printf("Opened descriptors: %d\n", count_descriptors());
    free(fds);
    free(fdr);
    free(franci);
    free(sarceni);
    return 0;
}
