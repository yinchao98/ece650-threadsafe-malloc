#include <stdio.h>
#include <stdlib.h>
#define BLOCKSIZE sizeof(block)

// structure: linked list of free memory blocks
typedef struct block_t {
	size_t size; // size of this free memory block
	struct block_t * next; // pointer to next free memory block
} block;

block * headBlock = NULL;

// first fit
void * ff_malloc(size_t size);
void ff_free(void * ptr);

// best fit
void * bf_malloc(size_t size);
void bf_free(void * ptr);

// get whole segment size and free space size
unsigned long get_largest_free_data_segment_size();
unsigned long get_total_free_size();

