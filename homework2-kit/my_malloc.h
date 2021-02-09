#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define BLOCKSIZE sizeof(block)

// structure: linked list of free memory blocks
typedef struct block_t {
	size_t size; // size of this free memory block
	struct block_t * next; // pointer to next free memory block
} block;

// lock version headBlock
block * headBlock = NULL;

// no lock version headBlock
__thread block * tlsHeadBlock = NULL;

// initialize the lock
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//Thread Safe malloc/free: locking version
void *ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);

//Thread Safe malloc/free: non-locking version 
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);