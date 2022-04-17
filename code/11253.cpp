//编译代码指令 gcc -std=gnu99 -g -o multithread_webserver multithread_webserver.c -lphtread
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/mman.h>
#include <semaphore.h>

#define VERSION 23
#define BUFSIZE 8096
#define ERROR        42
#define LOG         44
#define FORBIDDEN 403 
#define NOTFOUND 404
#ifndef SIGCLD
# define SIGCLD SIGCHLD 
#define SEM_NAME "thserver.c"
#endif

struct {
    char *ext; 
    char *filetype;
}extensions [] = {
{"gif", "image/gif" },
{"jpg", "image/jpg" },
{"jpeg","image/jpeg"},
{"png", "image/png" },
{"ico", "image/ico" },
{"zip", "image/zip" },
{"gz",  "image/gz"  },
{"tar", "image/tar" },
{"htm", "text/html" },
{"html","text/html" }, 
{0,0} 
};
typedef struct { 
    int hit;
    int fd; 
} webparam;
long sumrt=0,sumst=0,sumlt=0,sumwt=0;
float avgrt,avgst,avglt,avgwt;
long numr=0,nums=0,numl=0,numw=0;
pthread_mutex_t lockid1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;  
pthread_mutex_t lockid2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER; 
pthread_mutex_t lockid3 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond3 = PTHREAD_COND_INITIALIZER; 
pthread_mutex_t lockid4 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond4 = PTHREAD_COND_INITIALIZER; 

unsigned long get_file_size(const char *path) {
    unsigned long filesize = -1; struct stat statbuff;
    if(stat(path, &statbuff) < 0)
        return filesize;
    else
        filesize = statbuff.st_size; 
    return filesize; 
}


void logger(int type, char *s1, char *s2, int socket_fd) 
{ 
  int fd ; 
  struct timeval t1,t2;
  double timeuse;
  char logbuffer[BUFSIZE*2]; 
  time_t timep;
  struct tm *p;
  time(&timep);
  p = gmtime(&timep);
  /*根据消息类型，将消息放入logbuffer缓存，或直接将消息通过socket通道返回给客户端*/ 
  gettimeofday(&t1,NULL);
  switch (type) { 
  case ERROR: 
    (void)sprintf(logbuffer,"date:%d\\%d\\%d %d:%d:%d\nERROR: %s:%s Errno=%d exiting pid=%d",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour+8,p->tm_min,p->tm_sec,s1, s2, errno,getpid()); 
    break; 
  case FORBIDDEN: 
    (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\n The requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271); 
    (void)sprintf(logbuffer,"date:%d\\%d\\%d %d:%d:%d\nFORBIDDEN: %s:%s",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour+8,p->tm_min,p->tm_sec,s1, s2); 
    break; 
  case NOTFOUND: 
    (void)write(socket_fd,  "HTTP/1.1  404  Not  Found\nContent-Length:  136\nConnection: close\nContent-Type:  text/html\n\n<html><head>\n<title>404  Not Found</title>\n</head><body>\n<h1>Not  Found</h1>\nThe  requested  URL  was  not  found  on  this server.\n</body></html>\n",224); 
    (void)sprintf(logbuffer,"date:%d\\%d\\%d %d:%d:%d\nNOT FOUND: %s:%s",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour+8,p->tm_min,p->tm_sec,s1, s2); 
    break; 
  case LOG:
    (void)sprintf(logbuffer,"date:%d\\%d\\%d %d:%d:%d\nINFO: %s:%s:%d",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour+8,p->tm_min,p->tm_sec,s1, s2,socket_fd); 
    break; 
  } 
  gettimeofday(&t2,NULL);
  timeuse = (t2.tv_sec-t1.tv_sec)*1000000.0+t2.tv_usec-t1.tv_usec;
  /*printf("Time spent on putting in loggbuffer cache：%lf\n",timeuse);*/
  /* 将logbuffer缓存中的消息存入webserver.log文件*/ 
  gettimeofday(&t1,NULL);
  if((fd = open("nweb.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) { 
    (void)write(fd,logbuffer,strlen(logbuffer)); 
    (void)write(fd,"\n",1); 
    (void)close(fd); 
  } 
  gettimeofday(&t2,NULL);
  timeuse = (t2.tv_sec-t1.tv_sec)*1000000.0+t2.tv_usec-t1.tv_usec;
  /*printf("The time of loggbuffer sending to webserver.log：%lf\n",timeuse);*/
} 

    /* this is a web thread, so we can exit on errors */ 
void * web(void * data)
{
    int fd;
    int hit;
    int j, file_fd, buflen; 
    long i, ret, len; 
    char * fstr;
    struct timeval tss,tse,tws,twe,tls,tle;
    char buffer[BUFSIZE+1]; /* static so zero filled */ 
    webparam *param=(webparam*) data;
    fd=param->fd; hit=param->hit;
    struct timeval trs,tre;
    long readtime;
    gettimeofday(&trs,NULL);
    ret =read(fd,buffer,BUFSIZE);    /* read web request in one go */ 
    gettimeofday(&tre,NULL);
    pthread_mutex_lock(&lockid1);
    numr++;
    readtime = (tre.tv_sec-trs.tv_sec)*1000000+tre.tv_usec-trs.tv_usec;
    sumrt = sumrt+readtime;
    pthread_mutex_unlock(&lockid1);

    if(ret == 0 || ret == -1) {  /* read failure stop now */
        logger(FORBIDDEN,"failed to read browser request","",fd); 
    }
    else{
        long sendtime;
        gettimeofday(&tss,NULL);
        if(ret > 0 && ret < BUFSIZE)  /* return code is valid chars */ 
            buffer[ret]=0;     /* terminate the buffer */
        else 
            buffer[0]=0;
        for(i=0;i<ret;i++)  /* remove cf and lf characters */ 
            if(buffer[i] == '\r' || buffer[i] == '\n')
                buffer[i]='*';
        logger(LOG,"request",buffer,hit);
        gettimeofday(&tse,NULL);
        pthread_mutex_lock(&lockid2);
        nums++;
        sendtime = (tse.tv_sec-tss.tv_sec)*1000000+tse.tv_usec-tss.tv_usec;
        sumst = sumst + sendtime;
        pthread_mutex_unlock(&lockid2);

        if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) { 
            logger(FORBIDDEN,"only simple get operation supported",buffer,fd);
        }
        for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */ 
            if(buffer[i] == ' ') { /* string is "get url " +lots of other stuff */
                buffer[i] = 0; 
                break;
            } 
        }
        for(j=0;j<i-1;j++)    /* check for illegal parent directory use .. */ 
            if(buffer[j] == '.' && buffer[j+1] == '.') {
                logger(FORBIDDEN,"parent directory (..) path names not supported",buffer,fd); 
            }
        if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file*/
            (void)strcpy(buffer,"GET /index.html");
        /* work out the file type and check we support it */ 
        buflen=strlen(buffer);
        fstr = (char *)0;
        for(i=0;extensions[i].ext != 0;i++){ 
            len = strlen(extensions[i].ext);
            if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) { 
                fstr =extensions[i].filetype;
                break; 
            }
        }
        if(fstr == 0) 
            logger(FORBIDDEN,"file extension type not supported",buffer,fd); 
        if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
            logger(NOTFOUND, "failed to open file",&buffer[5],fd); 
        }

        long logtime;
        gettimeofday(&tls,NULL);
        logger(LOG,"send",&buffer[5],hit);
        len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* 使⽤ lseek 来获得⽂件⻓度，⽐较低效*/ (void)lseek(file_fd, (off_t)0, SEEK_SET);           /* 想想还有什么⽅法来获取*/
        (void)sprintf(buffer,"HTTP/1.1  200  ok\nserver:  nweb/%d.0\ncontent-length:  %ld\nconnection: close\ncontent-type: %s\n\n", VERSION, len, fstr); /* header + a blank line */
        logger(LOG,"header",buffer,hit); 
        (void)write(fd,buffer,strlen(buffer));
        gettimeofday(&tle,NULL);
        pthread_mutex_lock(&lockid3);
        numl++;
        logtime = (tle.tv_sec-tls.tv_sec)*1000000+tle.tv_usec-tls.tv_usec;
        sumlt = sumlt + logtime;
        pthread_mutex_unlock(&lockid3);

        /* send file in 8kb block - last block may be smaller */ 
        long webreadtime;
        gettimeofday(&tws,NULL);
        while ((ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
            (void)write(fd,buffer,ret); 
        }
        gettimeofday(&twe,NULL);
        pthread_mutex_lock(&lockid4);
        numw++;
        webreadtime = (twe.tv_sec-tws.tv_sec)*1000000+twe.tv_usec-tws.tv_usec;
        sumwt = sumwt + webreadtime;
        pthread_mutex_unlock(&lockid4);
        
        usleep(10000);/*在 socket 通道关闭前，留出⼀段信息发送的时间*/ 
        close(file_fd);
    }
    close(fd); //释放内存 
    free(param);
    printf("The thread%d number is:%lu\n",hit,pthread_self());
    //pthread_mutex_unlock(&lockid);    
}


int main(int argc, char **argv) 
{
    int i, port, pid, listenfd, socketfd, hit; 
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */ 
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
    sem_t* psem;
    int show;
    /*if((show=sem_init(psem,0, 1))!=0){ 
        perror("create semaphore error");
        exit(1); 
    }*/
    if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
        (void)printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
                     "\tnweb is a small and very safe mini web server\n"
                    "\tnweb only servers out file/web pages with extensions named below\n" 
                    "\t and only from the named directory or its sub-directories.\n"
                    "\tThere is no fancy features = safe and secure.\n\n" 
                    "\tExample: nweb 8181 /home/nwebdir &\n\n"
                    "\tOnly Supports:", VERSION);
        for(i=0;extensions[i].ext != 0;i++) 
            (void)printf(" %s",extensions[i].ext);
        (void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                    "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
                    "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0); 
    }
    if( !strncmp(argv[2],"/"    ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
        !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) || 
        !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) || 
        !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
          (void)printf("ERROR: Bad top directory %s, see nweb -?\n",argv[2]); exit(3);
    }
    if(chdir(argv[2]) == -1){
        (void)printf("ERROR: Can't Change to directory %s\n",argv[2]); 
        exit(4);
    }
    /* Become deamon + unstopable and no zombies children (= no wait()) */ 
    pid_t pro = fork();
    /*if(fork() != 0)
        return 0; /* parent returns OK to shell */
    if (pro==0)
    {
    /*(void)signal(SIGCLD, SIG_IGN); /* ignore child death */ 
    /*(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */ 
    /*for(i=0;i<32;i++)
        (void)close(i);     /* close open files */
    /*(void)setpgrp();     /* break away from process group */ 
    logger(LOG,"nweb starting",argv[1],getpid());
    /* setup the network socket */
    if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0) 
        logger(ERROR, "system call","socket",0);
    port = atoi(argv[1]); 
    if(port < 0 || port >60000)
        logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0); 
    
    //初始化线程属性，为分离状态
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED); 
    pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setschedpolicy(&attr,SCHED_FIFO);
    //
    pthread_t pth; 
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0) 
        logger(ERROR,"system call","bind",0);
    if( listen(listenfd,64) <0)
        logger(ERROR,"system call","listen",0); 
    printf("The child process number is:%d,its father is%d\n",getpid(),getppid());
    for(hit=1; ;hit++) {
        length = sizeof(cli_addr);
        if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0){
            logger(ERROR,"system call","accept",0);
            break;
            }
        webparam *param=malloc(sizeof(webparam)); 
        param->hit=hit;
        param->fd=socketfd;
        /*printf("aaaaaaaa\n");*/
        if(pthread_create(&pth, &attr, &web, (void*)param)<0){ 
            logger(ERROR,"system call","pthread_create",0);
        } 
        pthread_join(pth,NULL);
        avgrt = (float)sumrt/(float)numr;
        avgst = (float)sumst/(float)nums;
        avgwt = (float)sumwt/(float)numw;
        avglt = (float)sumlt/(float)numl;
        printf("Average reading time is %f\nAverage sending time is %f\nAverage webreading time is %f\nAverage logging time is %f\n",avgrt,avgst,avgwt,avglt);
    }
    
    pthread_mutex_destroy(&lockid1);
    pthread_mutex_destroy(&lockid2);
    pthread_mutex_destroy(&lockid3);
    pthread_mutex_destroy(&lockid4);
    /*pthread_cond_destroy(&cond1);
    pthread_cond_destroy(&cond2);
    pthread_cond_destroy(&cond3);
    pthread_cond_destroy(&cond4);*/
    }
    else if (pro>0){
        close(socketfd);
        printf("Father number is %d\n",getpid());
         wait(NULL);
    }
    /*sem_destroy(psem);*/
}