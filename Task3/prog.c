#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
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

#define MAX_MESSAGE_SIZE 30

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

sig_atomic_t volatile last_sig=0;
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
void sig_alrm(int sig)
{
    last_sig = SIGALRM;
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
typedef struct
{
    int id;
    int skill;
}student_t;

/*
n- [3,20] students
4 stages - 3,6,7,5
k - [3,9] skill lv
*/

void child_work(student_t* student,int fr,int R)
{
    srand(getpid());
    printf("student%d [%d]: %d skill\n",student->id,getpid(),student->skill);
    //sleep(1);
    char buf[MAX_MESSAGE_SIZE];
    int res;
    //sleep(1);
    if((res=TEMP_FAILURE_RETRY(read(fr,buf,sizeof(buf))))<0)
    {
        printf("naura\n");
        if(errno==EPIPE) return;
        ERR("read");
    }
    
    snprintf(buf,sizeof(buf),"Student [%d]: HERE",getpid());
    if(TEMP_FAILURE_RETRY(write(R,buf,sizeof(buf)))<0)
    {
        if(errno==EPIPE) return;
        ERR("write");
    }
    int toDo=4;
    while(toDo>0)
    {
        int t = 100 + rand()%401;
        msleep(t);
        int q = 1 + rand()%20;
        snprintf(buf,sizeof(buf),"%d%d",getpid(),q+student->skill);
        //printf("%s\n",buf);
        if((res =TEMP_FAILURE_RETRY(write(R,buf,sizeof(buf))))<0)
        {
            if(errno==EPIPE) {
                printf("Student %d: Oh no, I haven't finished stage %d! I need more time\n", getpid(), 5-toDo);
                break;
            }
            ERR("read");
        }
        if(res==0)
        {
            printf("Student %d: Oh no, I haven't finished stage %d! I need more time\n", getpid(), 5-toDo);
            break;
        }
        int num;
       // printf("Student [%d] waiting to read\n", getpid());
        if((res=TEMP_FAILURE_RETRY(read(fr,&num,sizeof(num))))<0)
        {
            printf("naura\n");
            if(errno==EPIPE) break;
            ERR("read");
        }
        if(res==0)
        {
            printf("Student %d: Oh no, I haven't finished stage %d! I need more time\n", getpid(), 5-toDo);
            break;
        }
        if(num==1) toDo--;
    }
    if(toDo==0)printf("Student [%d]: I NAILED IT!\n",getpid());
}

void create_child(student_t* students,int**fds,int R,int n)
{
    for(int i=0;i<n;i++)
    {
        if((fds[i] = calloc(sizeof(int),2))==NULL) ERR("calloc");
        if(pipe(fds[i])<0) ERR("pipe");
        int x = fork();
        switch (x)
        {
            case -1: ERR("fork");
            case 0: 
                //cleanup
                for(int k=0;k<i;k++) //clean up others pipes
                {
                    close(fds[k][0]);
                    close(fds[k][1]);
                    free(fds[k]);
                }
                close(fds[i][1]);

                //child logic
                srand(getpid());
                students[i].skill=3+rand()%7;
                students[i].id=i+1;
                child_work(&students[i],fds[i][0],R);
                close(R);
                //cleanup2
                close(fds[i][0]);
                free(fds[i]);
                free(fds);
                exit(0);
            default: students[i].id = x; break;
        }
       // close(fds[i][0]);
    }
}

void parent_work(student_t*students,int**fds,int R,int n)
{
    srand(getpid());
    //init
    int pidLen =1;
    int dummy = students[0].id;
    while(dummy>0)
    {
        pidLen++;
        dummy/=10;
    }
    char buf[MAX_MESSAGE_SIZE];
    int* progress = calloc(sizeof(int),n);
    int done=0;
    int pts[4] = {3,6,7,4};
    int studPts[4] = {0};
    for(int i=0;i<n;i++)
    { 
        progress[i] = 0;
        snprintf(buf,sizeof(buf),"Teacher: Is [%d] here?",students[i].id);
        printf("%s\n",buf);
        int res;
        
        if(TEMP_FAILURE_RETRY(write(fds[i][1],buf,sizeof(buf)))<0)
        {
            if(errno==EPIPE) break;
            ERR("write");
        }
        if((res=TEMP_FAILURE_RETRY(read(R,buf,sizeof(buf))))<0)
        {
            if(errno==EPIPE) break;
            ERR("write");
        }
        printf("%s\n",buf);
        if(res==0) break;
    }
    int points[4] = {4+rand()%20, 7+rand()%20, 8+rand()%20, 5+rand()%20};
    char skill[4];
    alarm(2);
    while(done<n)
    {
        if(last_sig==SIGALRM)
        {
            printf("Teacher: END OF TIME!\n");
            break;
        }
        int res;
        char buff[MAX_MESSAGE_SIZE];
        if((res=TEMP_FAILURE_RETRY(read(R,buff,sizeof(buff))))<0)
        {
            if(errno==EPIPE) break;
            ERR("write");
        }
        if(res==0) break;
        snprintf(buf, pidLen, "%s", buff);
        snprintf(skill, 4, "%s", buff+pidLen-1);
        for(int i=0;i<n;i++)
        {
           // printf("%d %d\n",atoi(buf),atoi(skill));
            if(atoi(buf)==students[i].id)
            {
              //  printf("ok\n");
                int num=0;
                if(atoi(skill)>=points[progress[i]])
                {
                    if(++progress[i]==4) done++;
                    printf("Teacher: Student [%d] finished stage [%d]\n",atoi(buf),progress[i]);
                    num=1;
                    studPts[i]+=pts[progress[i]-1];
                }
                else{
                    printf("Teacher: Student [%d] needs to fix stage [%d]\n",atoi(buf),progress[i]);
                }
                
                if(TEMP_FAILURE_RETRY(write(fds[i][1],&num,sizeof(num)))<0);
            }
        }
    }
    printf("Teacher: IT'S FINALLY OVER!\n");
    //cleanup
    for(int i=0;i<n;i++) 
    {
        close(fds[i][1]);
        close(fds[i][0]);
        free(fds[i]);
    }
    close(R);
    free(fds);
    free(progress);
}

int main(int argc,char**argv)
{
    if(argc!=2) usage(argv[0]);
    int n=atoi(argv[1]);
    if(n<3 || n>20) usage(argv[0]);
    student_t* students = calloc(sizeof(student_t),n);
    if(students==NULL) ERR("calloc");
    sethandler(SIG_IGN, SIGPIPE);
    sethandler(sig_alrm, SIGALRM);
    int** fds, R[2];
    if((fds=calloc(sizeof(int*),n))==NULL) ERR("malloc");
    if(pipe(R)<0) ERR("pipe");

    create_child(students,fds,R[1],n);
    close(R[1]);
    parent_work(students,fds,R[0],n);
    while (wait(NULL) > 0);

    free(students);
    return 0;
}