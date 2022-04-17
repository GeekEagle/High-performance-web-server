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
    mem_size_t alloc_mem;
    struct _mp_chunk* prev, * next;
    int is_free;
} _MP_Chunk;

typedef struct _mp_mem_pool_list {
    char* start;
    unsigned int id;
    mem_size_t mem_pool_size;
    mem_size_t alloc_mem;
    mem_size_t alloc_prog_mem;
    _MP_Chunk* free_list, * alloc_list;
    struct _mp_mem_pool_list* next;
} _MP_Memory;

typedef struct _mp_mem_pool {
    unsigned int last_id;
    int auto_extend;
    mem_size_t mem_pool_size, total_mem_pool_size, max_mem_pool_size;
    struct _mp_mem_pool_list* mlist;
#ifdef _Z_MEMORYPOOL_THREAD_
    pthread_mutex_t lock;
#endif
} MemoryPool;

/*
 *  内存池API
 */
MemoryPool* MemoryPoolInit(mem_size_t maxmempoolsize, mem_size_t mempoolsize);
void* MemoryPoolAlloc(MemoryPool* mp, mem_size_t want_size);
int MemoryPoolFree(MemoryPool* mp, void* p);
MemoryPool* MemoryPoolClear(MemoryPool* mp);
int MemoryPoolDestroy(MemoryPool* mp);
int MemoryPoolSetThreadSafe(MemoryPool* mp, int thread_safe);

/*
 *  内存池信息API
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

#define MP_INIT_MEMORY_STRUCT(mm, mem_sz)       \  
do {
    \
        mm->mem_pool_size = mem_sz;             \
        mm->alloc_mem = 0;                      \
        mm->alloc_prog_mem = 0;                 \
        mm->free_list = (_MP_Chunk*)mm->start; \
        mm->free_list->is_free = 1;             \
        mm->free_list->alloc_mem = mem_sz;      \
        mm->free_list->prev = NULL;             \
        mm->free_list->next = NULL;             \
        mm->alloc_list = NULL;                  \
} while (0)

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