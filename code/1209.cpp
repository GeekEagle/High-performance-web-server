#ifndef _Z_MEMORYPOOL_H_
#define _Z_MEMORYPOOL_H_

#define _Z_MEMORYPOOL_THREAD_
#ifdef _Z_MEMORYPOOL_THREAD_
#include <pthread.h>
#endif

#include <stdlib.h>
#include <string.h>

#define mem_size_t unsigned long long
#define KB (mem_size_t)(1 << 10)
#define MB (mem_size_t)(1 << 20)
#define GB (mem_size_t)(1 << 30)

typedef struct _mp_chunk {  
    //�ڴ��(����used��û�õ�free���ڴ�������ʾ������
    mem_size_t alloc_mem;  //ÿ�����С �������used��Ҳ������free��
    struct _mp_chunk* prev, * next; //���ǰ��ڵ�
    int is_free;  //�Ƿ���
} _MP_Chunk;

typedef struct _mp_mem_pool_list {  //�ڴ�����
    char* start;
    unsigned int id;
    mem_size_t mem_pool_size;  
    mem_size_t alloc_mem;     //�õĻ����õĿռ��С
    mem_size_t alloc_prog_mem;  //�ų�ͷβ�ڵ�Ŀ��С
    _MP_Chunk* free_list, * alloc_list;  //������ʼ��ֹ�ڵ�
    struct _mp_mem_pool_list* next;
} _MP_Memory;

typedef struct _mp_mem_pool {//�ڴ��
    unsigned int last_id;
    int auto_extend;
    mem_size_t mem_pool_size, total_mem_pool_size, max_mem_pool_size;  
    //�ڴ�ش�С  ���ڴ�ش�С ����С
    struct _mp_mem_pool_list* mlist;
#ifdef _Z_MEMORYPOOL_THREAD_ 
    pthread_mutex_t lock;
#endif
} MemoryPool;

/*
 *  �ڴ��API
 */
MemoryPool* MemoryPoolInit(mem_size_t maxmempoolsize, mem_size_t mempoolsize);  //��ʼ��
void* MemoryPoolAlloc(MemoryPool* mp, mem_size_t want_size);  //����
int MemoryPoolFree(MemoryPool* mp, void* p);
MemoryPool* MemoryPoolClear(MemoryPool* mp);
int MemoryPoolDestroy(MemoryPool* mp);
int MemoryPoolSetThreadSafe(MemoryPool* mp, int thread_safe);

/*
 *  �ڴ����ϢAPI
 */
mem_size_t GetTotalMemory(MemoryPool* mp);
mem_size_t GetUsedMemory(MemoryPool* mp);
float MemoryPoolGetUsage(MemoryPool* mp);
mem_size_t GetProgMemory(MemoryPool* mp);
float MemoryPoolGetProgUsage(MemoryPool* mp);

#endif  // !_Z_MEMORYPOOL_H_

#define MP_CHUNKHEADER sizeof(struct _mp_chunk)
#define MP_CHUNKEND sizeof(struct _mp_chunk*)


#define MP_ALIGN_SIZE(_n) (_n + sizeof(long) - ((sizeof(long) - 1) & _n))

#define MP_INIT_MEMORY_STRUCT(mm, mem_sz)       \    //��ʼ���ڴ�����
    do {                                        \
        mm->mem_pool_size = mem_sz;             \
        mm->alloc_mem = 0;                      \
        mm->alloc_prog_mem = 0;                 \
        mm->free_list = (_MP_Chunk*) mm->start; \
        mm->free_list->is_free = 1;             \
        mm->free_list->alloc_mem = mem_sz;      \
        mm->free_list->prev = NULL;             \
        mm->free_list->next = NULL;             \
        mm->alloc_list = NULL;                  \
    } while (0)

#define MP_DLINKLIST_INS_FRT(head, x) \ //�ڴ�����ͷ��
    do {                              \
        x->prev = NULL;               \
        x->next = head;               \
        if (head) head->prev = x;     \
        head = x;                     \
    } while (0)

#define MP_DLINKLIST_DEL(head, x)                 \  //ɾ��x
    do {                                          \
        if (!x->prev) {                           \
            head = x->next;                       \
            if (x->next) x->next->prev = NULL;    \
        } else {                                  \
            x->prev->next = x->next;              \
            if (x->next) x->next->prev = x->prev; \
        }                                         \
    } while (0)

static _MP_Memory* extend_memory_list(MemoryPool* mp, mem_size_t new_mem_sz) {  //���ڴ��������
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

static int merge_free_chunk(MemoryPool* mp, _MP_Memory* mm, _MP_Chunk* c) {  //�ϲ�δ����Ŀ�
    _MP_Chunk* p0 = c, * p1 = c;
    while (p0->is_free) {
        p1 = p0;
        if ((char*)p0 - MP_CHUNKEND - MP_CHUNKHEADER <= mm->start) break;
        p0 = *(_MP_Chunk**)((char*)p0 - MP_CHUNKEND);
    }
    p0 = (_MP_Chunk*)((char*)p1 + p1->alloc_mem);
    while ((char*)p0 < mm->start + mp->mem_pool_size && p0->is_free) {  //p0��û�з��䣬��p0��usedĳ�ض�λ��ǰ��
        MP_DLINKLIST_DEL(mm->free_list, p0);
        p1->alloc_mem += p0->alloc_mem;
        p0 = (_MP_Chunk*)((char*)p0 + p0->alloc_mem);
    }
    *(_MP_Chunk**)((char*)p1 + p1->alloc_mem - MP_CHUNKEND) = p1;
    pthread_mutex_unlock(&mp->lock);
    return 0;
}

MemoryPool* MemoryPoolInit(mem_size_t maxmempoolsize, mem_size_t mempoolsize) {  //��ʼ��
    if (mempoolsize > maxmempoolsize)return NULL;
    MemoryPool* mp = (MemoryPool*)malloc(sizeof(MemoryPool));  //�ڴ��
    if (!mp) return NULL;
    mp->last_id = 0;
    if (mempoolsize < maxmempoolsize) mp->auto_extend = 1;
    mp->max_mem_pool_size = maxmempoolsize;
    mp->total_mem_pool_size = mp->mem_pool_size = mempoolsize;
    pthread_mutex_init(&mp->lock, NULL);
    char* s = (char*)malloc(sizeof(_MP_Memory) + sizeof(char) * mp->mem_pool_size);  //�ڴ�����
    if (!s) return NULL;
    mp->mlist = (_MP_Memory*)s;  
    mp->mlist->start = s + sizeof(_MP_Memory);  //�ڴ�����ʼ�ڵ�
    MP_INIT_MEMORY_STRUCT(mp->mlist, mp->mem_pool_size);  //��ʼ������ڵ㣬��ֵ
    mp->mlist->next = NULL;
    mp->mlist->id = mp->last_id++;
    return mp;
}

void* MemoryPoolAlloc(MemoryPool* mp, mem_size_t wantsize) { //nginxʵ���ڴ��
    if (wantsize <= 0) return NULL;
    mem_size_t total_needed_size = MP_ALIGN_SIZE(wantsize + MP_CHUNKHEADER + MP_CHUNKEND);
    //�ܹ���Ҫ�ģ��Ƿ����С�����Ͽ��ͷβ
    if (total_needed_size > mp->mem_pool_size) return NULL;
    _MP_Memory* mm = NULL, * mm1 = NULL;
    _MP_Chunk* _free = NULL, * _not_free = NULL;
    pthread_mutex_lock(&mp->lock);
    FIND_FREE_CHUNK:
    mm = mp->mlist;
    while (mm) {  //�ҵ��пռ���ڴ�����ڵ�
        if (mp->mem_pool_size - mm->alloc_mem < total_needed_size) {  
            //ʣ��ռ�С����Ҫ�ռ䣬��������һ��
            mm = mm->next;
            continue;
        }
        _free = mm->free_list;  //δ�����
        while (_free) {     //���ĸ��ڴ�����ڵ��п�ֵ
            if (_free->alloc_mem >= total_needed_size) { //ʣ�ı�Ҫ�Ķ࣬��
                if (_free->alloc_mem - total_needed_size >
                    MP_CHUNKHEADER + MP_CHUNKEND) {  //ʣ�µĵط����ܷŸ�ͷβ
                    _not_free = _free;
                    _free = (_MP_Chunk*)((char*)_not_free + total_needed_size);
                    *_free = *_not_free;
                    _free->alloc_mem -= total_needed_size;  //�ɷ���ռ䣬��ȥ��Ҫ�Ŀռ�
                    *(_MP_Chunk**)((char*)_free + _free->alloc_mem - MP_CHUNKEND) = _free;
                    //ʣ��ɷ���ռ��ж��
                    if (!_free->prev) mm->free_list = _free;
                    else _free->prev->next = _free;
                    if (_free->next) _free->next->prev = _free;
                    _not_free->is_free = 0;   //�ѷ���ռ䣬�����ͷ�
                    _not_free->alloc_mem = total_needed_size;  //�ѷ���Ŀ�����ܹ���Ҫ�Ŀ��С
                    *(_MP_Chunk**)((char*)_not_free + total_needed_size - MP_CHUNKEND) = _not_free;
                    //�ѷ���ռ���
                }
                else {//ʣ�Ĳ���������
                    _not_free = _free;  
                    MP_DLINKLIST_DEL(mm->free_list, _not_free);
                    _not_free->is_free = 0;
                }
                MP_DLINKLIST_INS_FRT(mm->alloc_list, _not_free); //�ֺõļӵ��ѷ������
                mm->alloc_mem += _not_free->alloc_mem;// �ѷ����ܴ�С
                mm->alloc_prog_mem += (_not_free->alloc_mem - MP_CHUNKHEADER - MP_CHUNKEND); //���ռ��С
                pthread_mutex_unlock(&mp->lock);
                return (void*)((task*)_not_free + MP_CHUNKHEADER);
            }
            _free = _free->next;
        }
        mm = mm->next;
    }
    //�����Ƿִ��ڴ������
    if (mp->auto_extend) {
        if (mp->total_mem_pool_size >= mp->max_mem_pool_size) {  //���ڴ�ش�С��������
            pthread_mutex_unlock(&mp->lock);
            return NULL;
        }
        mem_size_t add_mem_sz = mp->max_mem_pool_size - mp->mem_pool_size;  //���Ӵ�С
        add_mem_sz = add_mem_sz >= mp->mem_pool_size ? mp->mem_pool_size : add_mem_sz;
        mm1 = extend_memory_list(mp, add_mem_sz);  //���ڴ�ڵ�
        if (!mm1) {  
            pthread_mutex_unlock(&mp->lock);
            return NULL;
        }
        mp->total_mem_pool_size += add_mem_sz;  //�ڴ�ش�С����
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
    MP_DLINKLIST_DEL(mm->alloc_list, ck);  //�ѷ���ģ��ͷ�
    MP_DLINKLIST_INS_FRT(mm->free_list, ck); //δ����ģ�ͷ��
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

#define MEM_NUM 300000
int main() {
    char* test1[MEM_NUM],test2[MEM_NUM];
    MemoryPool* pool = MemoryPoolInit(500 * MB, 100 * MB);
    struct timeval t1, t2;
    double timeuse;
    gettimeofday(&t1, NULL);
    for (int i = 0; i < MEM_NUM; i++)
         test1[i] = (char*)MemoryPoolAlloc(pool, sizeof(char) * (i % 1000));  
         //�ҵ�malloc ��������300000����С��ͬ���ڴ�
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("Self memory malloc for different scale: %lf us\n", timeuse);
    gettimeofday(&t1, NULL);
    for (int i = 0; i < MEM_NUM; i++)
        test2[i] = (char*)MemoryPoolAlloc(pool, sizeof(char));
        //�ҵ�malloc��������300000��С�ڴ�
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("Self memory malloc for small scale: %lf us\n", timeuse);
    gettimeofday(&t1, NULL);
    for (int i = 0; i < MEM_NUM; i++) {
        MemoryPoolFree(pool, test1[i]);   //�ҵ��ͷ�
        /*MemoryPoolFree(pool, test2[i]); */
    }
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("Self free memory: %lf us\n", timeuse);
    MemoryPoolClear(pool);
    MemoryPoolDestroy(pool);
    gettimeofday(&t1, NULL);
    for (int i = 0; i < MEM_NUM; i++) 
        test1[i] = (char*)malloc(sizeof(char) * (i % 1000));
    //ϵͳmalloc�����С��ͬ��300000���ڴ�
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("System malloc for differt scale:%lf us\n", timeuse);
    gettimeofday(&t1, NULL);
    for (int i = 0; i < MEM_NUM; i++)
        test2[i] = (char*)malloc(sizeof(char));
    //ϵͳmalloc��������300000��С�ڴ�
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("System malloc for small scale:%lf us\n", timeuse);
    gettimeofday(&t1, NULL);
    for (int i = 0; i < 300000; ++i) {
        free(test1[i]);
        /*free(test2[i]); */  //ϵͳ�ͷ�
    }
    gettimeofday(&t2, NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("System free:%lf us\n", timeuse);
    return 0;
}