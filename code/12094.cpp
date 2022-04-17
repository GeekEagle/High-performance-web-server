#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <stdbool.h>

#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

#ifndef _Z_MEMORYPOOL_H_
#define _Z_MEMORYPOOL_H_

#define _Z_MEMORYPOOL_THREAD_

#define mem_size_t unsigned long long
#define KB (mem_size_t)1024


typedef struct _mp_chunk {  //内存块
    mem_size_t alloc_mem;
    struct _mp_chunk* prev, * next;
    int is_free;
} _MP_Chunk;

typedef struct _mp_mem_pool_list {  //内存链表
    char* start;
    unsigned int id;
    mem_size_t mem_pool_size;
    mem_size_t alloc_mem;
    mem_size_t alloc_prog_mem;
    _MP_Chunk* free_list, * alloc_list;  //分配起始终止节点
    struct _mp_mem_pool_list* next;
} _MP_Memory;

typedef struct _mp_mem_pool {//内存池
    unsigned int last_id;
    int auto_extend;
    mem_size_t mem_pool_size, total_mem_pool_size, max_mem_pool_size;
    //内存池大小  总内存池大小 最大大小
    struct _mp_mem_pool_list* mlist;
#ifdef _Z_MEMORYPOOL_THREAD_
    pthread_mutex_t lock;
#endif
} MemoryPool;

struct {
    char* ext;
    char* filetype;
}extensions[] = {
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

pthread_mutex_t lockid1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lockid2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lockid3 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond3 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lockid4 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond4 = PTHREAD_COND_INITIALIZER;

typedef struct {
    int fd;
    int hit;
    int file_fd;
    long len;
    char* file_name;
    char* file_msg;
}webparam;

/* queue status and conditional variable*/
typedef struct staconv {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;  /*⽤于阻塞和唤醒线程池中线程*/
    bool status;            /*表示任务队列状态：false 为⽆任务；true 为有任务*/
} staconv;

/*Task*/
typedef struct task {
    struct task* next;            /* 指向下⼀任务   */
    void  (*function)(void* arg);  /*  函数指针   */
    void* arg;                    /*  函数参数指针  */
} task;
/*Task Queue*/
typedef struct taskqueue {
    pthread_mutex_t mutex;/* ⽤于互斥读写任务队列 */
    task* front;     /* 指向队⾸*/
    task* rear;	     /* 指向队尾*/
    staconv* has_jobs;    /* 根据状态，阻塞线程*/
    int  len;     /* 队列中任务个数*/
} taskqueue;
/* Thread */
typedef struct thread {
    int id;	/* 线程 id*/
    pthread_t pthread;  /* 封装的 POSIX 线程*/
    struct threadpool* pool;    /* 与线程池绑定*/
} thread;
/*Thread Pool*/
typedef struct threadpool {
    thread** threads;    /* 线程指针数组*/
    volatile int num_threads;    /*线程池中线程数量*/
    volatile int num_working;  /*⽬前正在⼯作的线程个数*/
    pthread_mutex_t  thcount_lock;          /*线程池锁⽤于修改上⾯两个变量 */
    pthread_cond_t  threads_all_idle;     /*⽤于销毁线程的条件变量*/
    taskqueue  queue; /*任务队列*/
    volatile bool is_alive;   /* 表示线程池是否还存活*/
}threadpool;

MemoryPool* MemoryPoolInit(mem_size_t maxmempoolsize, mem_size_t mempoolsize);  //初始化
void* MemoryPoolAlloc(MemoryPool* mp, mem_size_t want_size);  //分配
int MemoryPoolFree(MemoryPool* mp, void* p);
MemoryPool* MemoryPoolClear(MemoryPool* mp);
int MemoryPoolDestroy(MemoryPool* mp);
int MemoryPoolSetThreadSafe(MemoryPool* mp, int thread_safe);

mem_size_t GetTotalMemory(MemoryPool* mp);
mem_size_t GetUsedMemory(MemoryPool* mp);
float MemoryPoolGetUsage(MemoryPool* mp);
mem_size_t GetProgMemory(MemoryPool* mp);
float MemoryPoolGetProgUsage(MemoryPool* mp);

#endif  // !_Z_MEMORYPOOL_H_

#define MP_CHUNKHEADER sizeof(struct _mp_chunk)
#define MP_CHUNKEND sizeof(struct _mp_chunk*)


#define MP_ALIGN_SIZE(_n) (_n + sizeof(long) - ((sizeof(long) - 1) & _n))

#define MP_INIT_MEMORY_STRUCT(mm, mem_sz)       \
  do{                                          \
       mm->mem_pool_size = mem_sz;             \
       mm->alloc_mem = 0;                      \
       mm->alloc_prog_mem = 0;                 \
       mm->free_list = (_MP_Chunk*) mm->start; \
       mm->free_list->is_free = 1;             \
       mm->free_list->alloc_mem = mem_sz;      \
       mm->free_list->prev = NULL;             \
       mm->free_list->next = NULL;             \
       mm->alloc_list = NULL;                  \
   }while (0)                            

#define MP_DLINKLIST_INS_FRT(head, x) \
    do {                              \
        x->prev = NULL;               \
        x->next = head;               \
        if (head) head->prev = x;     \
        head = x;                     \
    } while (0)

#define MP_DLINKLIST_DEL(head, x)                 \
    do {                                          \
        if (!x->prev) {                           \
            head = x->next;                       \
            if (x->next) x->next->prev = NULL;    \
        } else {                                  \
            x->prev->next = x->next;              \
            if (x->next) x->next->prev = x->prev; \
        }                                         \
    } while (0)

static _MP_Memory* extend_memory_list(MemoryPool* mp, mem_size_t new_mem_sz) {
    char* s = (char*)malloc(sizeof(_MP_Memory) + new_mem_sz * sizeof(char));
    if (!s) return NULL;
    _MP_Memory* mm = (_MP_Memory*)s;
    mm->start = s + sizeof(_MP_Memory);
    MP_INIT_MEMORY_STRUCT(mm, new_mem_sz);
    mm->id = mp->last_id++;
    mm->next = mp->mlist;
    mp->mlist = mm;
    return mm;
}
static _MP_Memory* find_memory_list(MemoryPool* mp, void* p) {
    _MP_Memory* tmp = mp->mlist;
    while (tmp) {
        if (tmp->start <= (char*)p &&
            tmp->start + mp->mem_pool_size > (char*)p)
            break;
        tmp = tmp->next;
    }
    return tmp;
}
static int merge_free_chunk(MemoryPool* mp, _MP_Memory* mm, _MP_Chunk* c) {
    _MP_Chunk* p0 = c, * p1 = c;
    while (p0->is_free) {
        p1 = p0;
        if ((char*)p0 - MP_CHUNKEND - MP_CHUNKHEADER <= mm->start) break;
        p0 = *(_MP_Chunk**)((char*)p0 - MP_CHUNKEND);
    }
    p0 = (_MP_Chunk*)((char*)p1 + p1->alloc_mem);
    while ((char*)p0 < mm->start + mp->mem_pool_size && p0->is_free) {
        MP_DLINKLIST_DEL(mm->free_list, p0);
        p1->alloc_mem += p0->alloc_mem;
        p0 = (_MP_Chunk*)((char*)p0 + p0->alloc_mem);
    }
    *(_MP_Chunk**)((char*)p1 + p1->alloc_mem - MP_CHUNKEND) = p1;
    pthread_mutex_unlock(&mp->lock);
    return 0;
}
MemoryPool* MemoryPoolInit(mem_size_t maxmempoolsize, mem_size_t mempoolsize) {  //初始化
    if (mempoolsize > maxmempoolsize)return NULL;
    MemoryPool* mp = (MemoryPool*)malloc(sizeof(MemoryPool));  //内存池
    if (!mp) return NULL;
    mp->last_id = 0;
    if (mempoolsize < maxmempoolsize) mp->auto_extend = 1;
    mp->max_mem_pool_size = maxmempoolsize;
    mp->total_mem_pool_size = mp->mem_pool_size = mempoolsize;
    pthread_mutex_init(&mp->lock, NULL);
    char* s = (char*)malloc(sizeof(_MP_Memory) + sizeof(char) * mp->mem_pool_size);  //内存链表
    if (!s) return NULL;
    mp->mlist = (_MP_Memory*)s;
    mp->mlist->start = s + sizeof(_MP_Memory);  //内存链表开始节点
    MP_INIT_MEMORY_STRUCT(mp->mlist, mp->mem_pool_size);  //初始化这个节点，赋值
    mp->mlist->next = NULL;
    mp->mlist->id = mp->last_id++;
    return mp;
}

void* MemoryPoolAlloc(MemoryPool* mp, mem_size_t wantsize) {
    if (wantsize <= 0) return NULL;
    mem_size_t total_needed_size = MP_ALIGN_SIZE(wantsize + MP_CHUNKHEADER + MP_CHUNKEND);
    //总共需要的，是分配大小，加上块的头尾
    if (total_needed_size > mp->mem_pool_size) return NULL;
    _MP_Memory* mm = NULL, * mm1 = NULL;
    _MP_Chunk* _free = NULL, * _not_free = NULL;
    pthread_mutex_lock(&mp->lock);
FIND_FREE_CHUNK:
    mm = mp->mlist;
    while (mm) {
        if (mp->mem_pool_size - mm->alloc_mem < total_needed_size) {
            //剩余空间小于需要空间，不给，下一个
            mm = mm->next;
            continue;
        }
        _free = mm->free_list;  //未分配块
        while (_free) {
            if (_free->alloc_mem >= total_needed_size) { //剩的比要的多，给
                if (_free->alloc_mem - total_needed_size >
                    MP_CHUNKHEADER + MP_CHUNKEND) {  //剩下的地方还能放个头尾
                    _not_free = _free;
                    _free = (_MP_Chunk*)((task*)_not_free + total_needed_size);
                    *_free = *_not_free;
                    _free->alloc_mem -= total_needed_size;  //可分配空间，减去需要的空间
                    *(_MP_Chunk**)((task*)_free + _free->alloc_mem - MP_CHUNKEND) = _free;
                    //剩余可分配空间有多大
                    if (!_free->prev) mm->free_list = _free;
                    else _free->prev->next = _free;
                    if (_free->next) _free->next->prev = _free;
                    _not_free->is_free = 0;   //已分配空间，不可释放
                    _not_free->alloc_mem = total_needed_size;
                    *(_MP_Chunk**)((task*)_not_free + total_needed_size - MP_CHUNKEND) = _not_free;
                    //已分配空间多大
                }
                else {//剩的不够，踢了
                    _not_free = _free;
                    MP_DLINKLIST_DEL(mm->free_list, _not_free);
                    _not_free->is_free = 0;
                }
                MP_DLINKLIST_INS_FRT(mm->alloc_list, _not_free); //分好的加到已分配块里
                mm->alloc_mem += _not_free->alloc_mem;// 已分配总大小
                mm->alloc_prog_mem += (_not_free->alloc_mem - MP_CHUNKHEADER - MP_CHUNKEND); //纯空间大小
                pthread_mutex_unlock(&mp->lock);
                return (void*)((task*)_not_free + MP_CHUNKHEADER);
            }
            _free = _free->next;
        }
        mm = mm->next;
    }
    //下面是分大内存区域的
    if (mp->auto_extend) {
        if (mp->total_mem_pool_size >= mp->max_mem_pool_size) {
            pthread_mutex_unlock(&mp->lock);
            return NULL;
        }
        mem_size_t add_mem_sz = mp->max_mem_pool_size - mp->mem_pool_size;
        add_mem_sz = add_mem_sz >= mp->mem_pool_size ? mp->mem_pool_size : add_mem_sz;
        mm1 = extend_memory_list(mp, add_mem_sz);
        if (!mm1) {
            pthread_mutex_unlock(&mp->lock);
            return NULL;
        }
        mp->total_mem_pool_size += add_mem_sz;
        goto FIND_FREE_CHUNK;
    }
    pthread_mutex_unlock(&mp->lock);
    return NULL;
}
int MemoryPoolFree(MemoryPool* mp, void* p) {
    if (p == NULL || mp == NULL) return 1;
    pthread_mutex_lock(&mp->lock);
    _MP_Memory* mm = mp->mlist;
    if (mp->auto_extend) mm = find_memory_list(mp, p);
    _MP_Chunk* ck = (_MP_Chunk*)((char*)p - MP_CHUNKHEADER);
    MP_DLINKLIST_DEL(mm->alloc_list, ck);  //已分配的，释放
    MP_DLINKLIST_INS_FRT(mm->free_list, ck); //未分配的，头插
    ck->is_free = 1;
    mm->alloc_mem -= ck->alloc_mem;
    mm->alloc_prog_mem -= (ck->alloc_mem - MP_CHUNKHEADER - MP_CHUNKEND);
    return merge_free_chunk(mp, mm, ck);
}
MemoryPool* MemoryPoolClear(MemoryPool* mp) {
    if (!mp) return NULL;
    pthread_mutex_lock(&mp->lock);
    _MP_Memory* mm = mp->mlist;
    while (mm) {
        MP_INIT_MEMORY_STRUCT(mm, mm->mem_pool_size);
        mm = mm->next;
    }
    pthread_mutex_unlock(&mp->lock);
    return mp;
}
int MemoryPoolDestroy(MemoryPool* mp) {
    if (mp == NULL) return 1;
    pthread_mutex_lock(&mp->lock);
    _MP_Memory* mm = mp->mlist, * mm1 = NULL;
    while (mm) {
        mm1 = mm;
        mm = mm->next;
        free(mm1);
    }
    pthread_mutex_unlock(&mp->lock);
    pthread_mutex_destroy(&mp->lock);
    free(mp);
    return 0;
}
mem_size_t GetTotalMemory(MemoryPool* mp) {
    return mp->total_mem_pool_size;
}
mem_size_t GetUsedMemory(MemoryPool* mp) {
    pthread_mutex_lock(&mp->lock);
    mem_size_t total_alloc = 0;
    _MP_Memory* mm = mp->mlist;
    while (mm) {
        total_alloc += mm->alloc_mem;
        mm = mm->next;
    }
    pthread_mutex_unlock(&mp->lock);
    return total_alloc;
}
mem_size_t GetProgMemory(MemoryPool* mp) {
    pthread_mutex_lock(&mp->lock);
    mem_size_t total_alloc_prog = 0;
    _MP_Memory* mm = mp->mlist;
    while (mm) {
        total_alloc_prog += mm->alloc_prog_mem;
        mm = mm->next;
    }
    pthread_mutex_unlock(&mp->lock);
    return total_alloc_prog;
}
float MemoryPoolGetUsage(MemoryPool* mp) {
    return (float)GetUsedMemory(mp) / GetTotalMemory(mp);
}
float MemoryPoolGetProgUsage(MemoryPool* mp) {
    return (float)GetProgMemory(mp) / GetTotalMemory(mp);
}

#undef MP_CHUNKHEADER
#undef MP_CHUNKEND
#undef MP_ALIGN_SIZE
#undef MP_INIT_MEMORY_STRUCT
#undef MP_DLINKLIST_INS_FRT
#undef MP_DLINKLIST_DEL

void staconv_wait(staconv* s);
void staconv_signal(staconv* s);
void init_taskqueue(taskqueue* queue);
task* take_taskqueue(taskqueue* queue);
void push_taskqueue(taskqueue* queue, task* curtask);
void destroy_taskqueue(taskqueue* queue);
struct threadpool* initTheadPool(int num_threads);
void addTask2ThreadPool(threadpool* pool, task* curtask);
void waitThreadPool(threadpool* pool);
void destoryThreadPool(threadpool* pool);
int getNumofThreadWorking(threadpool* pool);
int create_thread(struct threadpool* pool, struct thread* pthread, int id);
void* thread_do(struct thread* pthread);

double sbt = 0, abt, slt = 0, alt, numb = 0, numl = 0, num = 0, suml = 0, avgl;
long maxl = 0, minl = 10000000;

void staconv_wait(staconv* s)//p操作
{
    pthread_mutex_lock(&(s->mutex));
    struct timeval ts, te;
    double timeuse;
    gettimeofday(&ts, NULL);
    while (s->status <= 0)
    {
        pthread_cond_wait(&(s->cond), &(s->mutex));
    }
    s->status = false;
    gettimeofday(&te, NULL);
    timeuse = (te.tv_sec - ts.tv_sec) * 1000000.0 + te.tv_usec - ts.tv_usec;
    sbt += timeuse;           //阻塞时间
    numb++;
    pthread_mutex_unlock(&(s->mutex));
}
void staconv_signal(staconv* s)//v操作
{
    pthread_mutex_lock(&(s->mutex));
    s->status = true;
    pthread_cond_signal(&(s->cond));
    pthread_mutex_unlock(&(s->mutex));
}

void init_taskqueue(taskqueue* queue)
{
    queue->len = 0;
    pthread_mutex_init(&(queue->mutex), NULL);
    queue->front = NULL;
    queue->rear = NULL;
    queue->has_jobs = (staconv*)malloc(sizeof(staconv));
    staconv* s = queue->has_jobs;
    pthread_mutex_init(&(s->mutex), NULL);
    pthread_cond_init(&(s->cond), NULL);
    s->status = false;
}

task* take_taskqueue(taskqueue* queue)
{
    pthread_mutex_lock(&(queue->mutex));
    task* t = queue->front;
    if (queue->len == 1)
    {
        queue->front = NULL;
        queue->rear = NULL;
        queue->len = 0;
    }
    else if (queue->len > 1)
    {
        queue->front = t->next;
        queue->len -= 1;
        staconv_signal(queue->has_jobs);
    }
    pthread_mutex_unlock(&(queue->mutex));
    return t;
}

void push_taskqueue(taskqueue* queue, task* curtask)
{
    pthread_mutex_lock(&(queue->mutex));
    curtask->next = NULL;
    if (queue->len == 0)
    {
        queue->rear = curtask;
        queue->front = curtask;
    }
    else
    {
        queue->rear->next = curtask;
        queue->rear = curtask;
    }
    queue->len++;
    staconv_signal(queue->has_jobs);
    pthread_mutex_unlock(&(queue->mutex));
}
void destory_taskqueue(taskqueue* queue)
{
    pthread_mutex_destroy(&(queue->mutex));
    free(queue->front);
    pthread_mutex_destroy(&(queue->has_jobs->mutex));
    pthread_cond_destroy(&(queue->has_jobs->cond));
}

/*线程池初始化函数*/
struct threadpool* initThreadPool(int num_threads)
{
    //创建线程池空间
    threadpool* pool;
    pool = (threadpool*)malloc(sizeof(struct threadpool));
    pool->num_threads = 0;
    pool->num_working = 0;
    pool->is_alive = true;
    //初始化互斥量和条件变量
    pthread_mutex_init(&(pool->thcount_lock), NULL);
    pthread_cond_init(&pool->threads_all_idle, NULL);
    //初始化任务队列
    //****需实现*****
    init_taskqueue(&pool->queue);
    //创建线程数组
    pool->threads = (struct thread*)malloc(num_threads * sizeof(struct thread*));
    //创建线程
    for (int i = 0; i < num_threads; i++) {
        create_thread(pool, &(pool->threads[i]), i);  //i 为线程 id, 
    }
    //等等所有的线程创建完毕,在每个线程运⾏函数中将进⾏ pool->num_threads++ 操作 
    //因此，此处为忙等待，直到所有的线程创建完毕，并⻢上运⾏阻塞代码时才返回。 
    while (pool->num_threads != num_threads) {}
    return pool;
}

/*向线程池中添加任务*/
void addTask2ThreadPool(threadpool* pool, task* curtask)
{
    //将任务加⼊队列
    push_taskqueue(&pool->queue, curtask);
}
/*等待当前任务全部运⾏完*/
void waitThreadPool(threadpool* pool) {
    pthread_mutex_lock(&pool->thcount_lock);
    while (pool->queue.len || pool->num_working) {
        pthread_cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    }
    pthread_mutex_unlock(&pool->thcount_lock);
}
/*销毁线程池*/
void destoryThreadPool(threadpool* pool) {
    //如果当前任务队列中有任务，需等待任务队列为空，并且运⾏线程执⾏完任务后
    if ((pool->queue).has_jobs->status == true)
        waitThreadPool(pool);
    //销毁任务队列 
    //****需实现*****
    destory_taskqueue(&pool->queue);
    //销毁线程指针数组,并释放所有为线程池分配的内存
    free(pool->threads);
    pthread_mutex_destroy(&(pool->thcount_lock));
    pthread_cond_destroy(&(pool->threads_all_idle));
}
/*获得当前线程池中正在运⾏线程的数量*/
int getNumofThreadWorking(threadpool* pool)
{
    return pool->num_working;
}
double qlen = 0, qnum = 0, aqlen;
/*线程运⾏的逻辑函数*/
void* thread_do(struct thread* pthread)
{
    /* 设置线程名字 */
    struct timeval ts, te;
    double timeuse;
    char thread_name[128] = { 0 };
    sprintf(thread_name, "thread-pool-%d", pthread->id);
    prctl(PR_SET_NAME, thread_name);
    /* 获得线程池*/
    threadpool* pool = pthread->pool;
    /* 在线程池初始化时，⽤于已经创建线程的计数，执⾏ pool->num_threads++ */
    pthread_mutex_lock(&(pool->thcount_lock));
    pool->num_threads++;
    pthread_mutex_unlock(&(pool->thcount_lock));
    /*线程⼀直循环往复运⾏，直到 pool->is_alive 变为 false*/
    while (pool->is_alive)
    {
        /*如果任务队列中还要任务，则继续运⾏，否则阻塞*/
        staconv* s = (pool->queue).has_jobs;
        staconv_wait(s);
        if (pool->is_alive) {
            /*执⾏到此位置，表明线程在⼯作，需要对⼯作线程数量进⾏计数*/
            pthread_mutex_lock(&(pool->thcount_lock));
            pool->num_working++;
            pthread_mutex_unlock(&(pool->thcount_lock));
            /* 从任务队列的队⾸提取任务，并执⾏*/
            gettimeofday(&ts, NULL);
            void (*func)(void*);
            void* arg;
            //take_taskqueue 从任务队列头部提取任务，并在队列中删除此任务 
            //****需实现 take_taskqueue*****
            task* curtask = take_taskqueue(&pool->queue);
            qlen += pool->queue.len; //每次取消息队列长度，累加
            qnum++;//取了多少次
            if (curtask) {
                func = curtask->function;
                arg = curtask->arg;
                //执⾏任务
                func(arg);
                if (maxl < pool->num_working)
                    maxl = pool->num_working;
                if (minl > pool->num_working)
                    minl = pool->num_working;
                suml += pool->num_working;
                num++;
                //释放任务 
                free(curtask);
            }
            /*gettimeofday(&te, NULL);
            timeuse = (te.tv_sec - ts.tv_sec) * 1000000.0 + te.tv_usec - ts.tv_usec;
            slt += timeuse;           //活跃时间
            numl++;*/
            /*执⾏到此位置，表明线程已经将任务执⾏完成，需更改⼯作线程数量*/
           //此处还需注意，当⼯作线程数量为 0，表 示 任 务 全 部 完 成 ，要 让 阻 塞 在 waitThreadPool 函数上的线程继续运⾏
            pthread_mutex_lock(&(pool->thcount_lock));
            pool->num_working--;
            if (pool->num_working == 0)
                pthread_cond_signal(&(pool->threads_all_idle));
            pthread_mutex_unlock(&(pool->thcount_lock));
        }
    }
    /*运⾏到此位置表明，线程将要退出，需更改当前线程池中的线程数量*/
    //pool->num_threads--
    pthread_mutex_lock(&(pool->thcount_lock));
    pool->num_threads--;
    pthread_mutex_unlock(&(pool->thcount_lock));
    return NULL;
}

/*创建线程*/
int create_thread(struct threadpool* pool, struct thread* pthread, int id)
{
    //为 thread 分配内存空间
    pthread = (struct thread*)malloc(sizeof(struct thread));
    if (pthread == NULL) {
        perror("creat_thread(): Could not allocate memory for thread\n");
        return -1;
    }
    //设置这个 thread 的属性
    (pthread)->pool = pool;
    (pthread)->id = id;
    //创建线程
    pthread_create(&(pthread)->pthread, NULL, (void*)thread_do, (pthread));
    pthread_detach((pthread)->pthread);
    return 0;
}

unsigned long get_file_size(const char* path) {
    unsigned long filesize = -1; struct stat statbuff;
    if (stat(path, &statbuff) < 0)
        return filesize;
    else
        filesize = statbuff.st_size;
    return filesize;
}

void logger(int type, char* s1, char* s2, int socket_fd)
{
    int fd;
    struct timeval t1, t2;
    double timeuse;
    char logbuffer[BUFSIZE * 2];
    time_t timep;
    struct tm* p;
    time(&timep);
    p = gmtime(&timep);
    /*根据消息类型，将消息放入logbuffer缓存，或直接将消息通过socket通道返回给客户端*/
    gettimeofday(&t1, NULL);
    switch (type) {
    case ERROR:
        (void)sprintf(logbuffer, "date:%d\\%d\\%d %d:%d:%d\nERROR: %s:%s Errno=%d exiting pid=%d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour + 8, p->tm_min, p->tm_sec, s1, s2, errno, getpid());
        break;
    case FORBIDDEN:
        (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\n The requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
        (void)sprintf(logbuffer, "date:%d\\%d\\%d %d:%d:%d\nFORBIDDEN: %s:%s", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour + 8, p->tm_min, p->tm_sec, s1, s2);
        break;
    case NOTFOUND:
        (void)write(socket_fd, "HTTP/1.1  404  Not  Found\nContent-Length:  136\nConnection: close\nContent-Type:  text/html\n\n<html><head>\n<title>404  Not Found</title>\n</head><body>\n<h1>Not  Found</h1>\nThe  requested  URL  was  not  found  on  this server.\n</body></html>\n", 224);
        (void)sprintf(logbuffer, "date:%d\\%d\\%d %d:%d:%d\nNOT FOUND: %s:%s", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour + 8, p->tm_min, p->tm_sec, s1, s2);
        break;
    case LOG:
        (void)sprintf(logbuffer, "date:%d\\%d\\%d %d:%d:%d\nINFO: %s:%s:%d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour + 8, p->tm_min, p->tm_sec, s1, s2, socket_fd);
        break;
    }
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000.0 + t2.tv_usec - t1.tv_usec;
    /*printf("Time spent on putting in loggbuffer cache：%lf\n",timeuse);*/
    /* 将logbuffer缓存中的消息存入webserver.log文件*/
    gettimeofday(&t1, NULL);
    if ((fd = open("nweb.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        (void)write(fd, logbuffer, strlen(logbuffer));
        (void)write(fd, "\n", 1);
        (void)close(fd);
    }
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000.0 + t2.tv_usec - t1.tv_usec;
    /*printf("The time of loggbuffer sending to webserver.log：%lf\n",timeuse);*/
}

threadpool* read_msg_pool; //初始化线程池300个线程容量，且300个线程开始等待任务队列的进来
threadpool* readfile_pool;
threadpool* send_pool;
MemoryPool* rpool;
MemoryPool* rfpool;
MemoryPool* spool;
pthread_mutex_t lockid = PTHREAD_MUTEX_INITIALIZER;
struct timeval ts, te;
double timeuse;

void* web_sendMsg(void* data)
{

    webparam* param = data;

    char* buffer = param->file_msg;
    (void)write(param->fd, buffer, param->len);
    //printf("send\n");
    gettimeofday(&te, NULL);
    timeuse = (te.tv_sec - ts.tv_sec) * 1000000 + te.tv_usec - ts.tv_usec;
    printf("Nginx malloc time use:%lf us\n", timeuse);
    pthread_mutex_unlock(&lockid);

    close(param->fd);
    free(param->file_msg);
    free(param);
}

void readmsg(void* data)
{
    webparam* param = data;
    int fd;
    int hit, file_fd, buflen;
    long ret, len, i;
    char* fstr;
    char* buffer = param->file_name;
    buflen = strlen(buffer);
    fd = param->fd;
    hit = param->hit;
    fstr = (char*)0;
    //printf("readfile1111111\n");
    for (i = 0; extensions[i].ext != 0; i++) {
        len = strlen(extensions[i].ext);
        if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
            fstr = extensions[i].filetype;
            break;
        }
    }
    if (fstr == 0) logger(FORBIDDEN, "file extension type not supported", buffer, fd);


    if ((file_fd = open(&(buffer[5]), O_RDONLY)) == -1) {  /* open the file for reading */
        logger(NOTFOUND, "failed to open file", &(buffer[5]), fd);
    }
    logger(LOG, "SEND", buffer, hit);
    len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
    (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
    (void)sprintf(buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
    logger(LOG, "Header", buffer, hit);

    (void)write(fd, buffer, strlen(buffer));
    webparam* pa = malloc(sizeof(webparam));
    char* newbuffer = (char*)malloc(sizeof(char) * (len + 1));
    //printf("readfile4444444444\n");
    if ((ret = read(file_fd, newbuffer, (len + 1))) > 0)
    {
        pa->file_msg = newbuffer;
        pa->fd = fd;
        pa->len = len + 1;
        //task* t = (task*)malloc(sizeof(task));
        t = (task*)MemoryPoolAlloc(spool, sizeof(task));
         printf("readfile\n");
        t->arg = (void*)pa;
        t->function = (void*)web_sendMsg;
        addTask2ThreadPool(send_pool, t);
    }
    free(param);
    close(file_fd);

}

void* web(void* data)
{
    webparam* par = (webparam*)malloc(sizeof(webparam));
    par->file_name = (char*)malloc(sizeof(char) * (BUFSIZE + 1));
    int fd;
    int hit;
    int j, file_fd, buflen;
    long i, ret, len;
    char* fstr;
    struct timeval tss, tse, tws, twe, tls, tle;
    char* buffer = par->file_name; /* static so zero filled */
    webparam* param = (webparam*)data;
    fd = param->fd; hit = param->hit;
    struct timeval trs, tre;
    long readtime;
    pthread_mutex_lock(&lockid);
    gettimeofday(&ts, NULL);
    ret = read(fd, buffer, BUFSIZE);    /* read web request in one go */

    if (ret == 0 || ret == -1) {  /* read failure stop now */
        logger(FORBIDDEN, "failed to read browser request", "", fd);
    }
    else {
        if (ret > 0 && ret < BUFSIZE)  /* return code is valid chars */
            buffer[ret] = 0;     /* terminate the buffer */
        else
            buffer[0] = 0;
        for (i = 0; i < ret; i++)  /* remove cf and lf characters */
            if (buffer[i] == '\r' || buffer[i] == '\n')
                buffer[i] = '*';
        logger(LOG, "request", buffer, hit);

        if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
            logger(FORBIDDEN, "only simple get operation supported", buffer, fd);
        }
        for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
            if (buffer[i] == ' ') { /* string is "get url " +lots of other stuff */
                buffer[i] = 0;
                break;
            }
        }
        for (j = 0; j < i - 1; j++)    /* check for illegal parent directory use .. */
            if (buffer[j] == '.' && buffer[j + 1] == '.') {
                logger(FORBIDDEN, "parent directory (..) path names not supported", buffer, fd);
            }
        if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) /* convert no filename to index file*/
            (void)strcpy(buffer, "GET /index.html");
        /* work out the file type and check we support it */

        par->fd = fd;
        par->hit = hit;
        task* ta = (task*)malloc(sizeof(task));
        //ta = (task*)MemoryPoolAlloc(rfpool, sizeof(task));
        printf("readreq\n");
        ta->function = (void*)readmsg;
        ta->arg = (void*)par;
        addTask2ThreadPool(readfile_pool, ta);//将任务加入解析线程池中
        /* send file in 8kb block - last block may be smaller */
    }
    free(param);
    /*printf("The thread%d number is:%lu\n",hit,pthread_self());*/
}

int main(int argc, char** argv)
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
    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
        (void)printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
            "\tnweb is a small and very safe mini web server\n"
            "\tnweb only servers out file/web pages with extensions named below\n"
            "\t and only from the named directory or its sub-directories.\n"
            "\tThere is no fancy features = safe and secure.\n\n"
            "\tExample: nweb 8181 /home/nwebdir &\n\n"
            "\tOnly Supports:", VERSION);
        for (i = 0; extensions[i].ext != 0; i++)
            (void)printf(" %s", extensions[i].ext);
        (void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
            "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
            "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0);
    }
    if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
        !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
        !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
        !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)) {
        (void)printf("ERROR: Bad top directory %s, see nweb -?\n", argv[2]); exit(3);
    }
    if (chdir(argv[2]) == -1) {
        (void)printf("ERROR: Can't Change to directory %s\n", argv[2]);
        exit(4);
    }
    /* Become deamon + unstopable and no zombies children (= no wait()) */
    //pid_t pro = fork();
    (void)signal(SIGCLD, SIG_IGN); /* ignore child death */
    (void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */

    logger(LOG, "nweb starting", argv[1], getpid());
    /* setup the network socket */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        logger(ERROR, "system call", "socket", 0);
    port = atoi(argv[1]);
    if (port < 0 || port >60000)
        logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        logger(ERROR, "system call", "bind", 0);
    if (listen(listenfd, 64) < 0)
        logger(ERROR, "system call", "listen", 0);
    /*printf("The child process number is:%d,its father is%d\n",getpid(),getppid());*/
    read_msg_pool = initThreadPool(16);
    readfile_pool = initThreadPool(16);
    send_pool = initThreadPool(10);
    rpool = MemoryPoolInit(300 * KB, 150 * KB);
    rfpool = MemoryPoolInit(300 * KB, 150 * KB);
    spool = MemoryPoolInit(300 * KB, 150 * KB);

    for (hit = 1; ; hit++) {
        length = sizeof(cli_addr);

        if ((socketfd = accept(listenfd, (struct sockaddr*)&cli_addr, &length)) < 0)
            logger(ERROR, "system call", "accept", 0);
        gettimeofday(&ts, NULL);
        webparam* param = malloc(sizeof(webparam));
        param->hit = hit;
        param->fd = socketfd;
        task* t = (task*)malloc(sizeof(task));
        //t = (task*)MemoryPoolAlloc(rpool, sizeof(task));
        if (!t) {
            printf("error malloc\n");
            exit(1);
        }
        t->function = (void*)web;
        t->arg = (void*)param;
        addTask2ThreadPool(read_msg_pool, t);//将任务加入解析线程池中

       /* avgl = suml / num;  //平均活跃数量
        abt = sbt / numb;  //平均阻塞时间
        alt = slt / numl; //平均活跃时间
        aqlen = qlen / qnum; //消息队列平均长度线程数
        printf("Average live number:%lf\n", avgl);
        printf("Average block time:%lf\n", abt);
        printf("Average live time:%lf\n", alt);
        printf("max live:%ld,min live:%ld\n", maxl, minl);
        printf("Average length on taskqueue:%lf\n", aqlen);*/
    }
    MemoryPoolClear(rpool);
    MemoryPoolDestroy(rpool);
    MemoryPoolClear(rfpool);
    MemoryPoolDestroy(rfpool);
    MemoryPoolClear(spool);
    MemoryPoolDestroy(spool);
}