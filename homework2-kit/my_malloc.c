#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  
#include <limits.h>
#include <stdint.h>
#include <pthread.h>
#include "my_malloc.h"

// split the old block and return the new block
block * splitBlock(block* oldBlock, size_t size) {
	block * newBlock = (block*)((char*)oldBlock + BLOCKSIZE + size);
	newBlock->next = oldBlock->next;
	newBlock->size = oldBlock->size - size - BLOCKSIZE;
	oldBlock->size = size;
	return newBlock;
}

// to see whether need to split the block
void trySplit(block * prev, block * toUse, size_t size, block ** head) {
	// need to split
	if(toUse->size > size + BLOCKSIZE) {
		block * new = splitBlock(toUse, size);
		if(prev == NULL) {
			*head = new;
		} else {
			prev->next = new;
		}
	} else { // no need to split
		if(prev == NULL) {
			*head = (*head)->next;
		} else {
			prev->next = toUse->next;
		}
	}
	
}

// merge two blocks
void merge(block * prev, block * post) {
	prev->next = post->next;
	prev->size = prev->size + BLOCKSIZE + post->size;
	return;
}

// to see whether to merge with previous one or post one
void tryMerge(block * prev, block * middle, block * post) {
	int mergePrev = 0;
	// merge prev and middle
	if(prev != NULL) {
		if((char*)prev + BLOCKSIZE + prev->size == (char*)middle) {
			merge(prev, middle);
			mergePrev = 1;
		}
	}
	// merge middle and post
	if(post != NULL) {
		if((char*)middle + BLOCKSIZE + middle->size == (char*)post) {
			// already merge the previous one
			if(mergePrev) {
				merge(prev, post);
			} else {
				merge(middle, post);
			}
		}
	}
	return;
}

void * bf_malloc(size_t size, block ** head, void* (*func)(size_t)) {
	// size cannot be 0, return NULL
	if(size == 0) {
		return NULL;
	}
	// no free memory blocks, request for new memory block
	if(*head == NULL) {
		return (*func)(size);
	}
	// have memory blocks, look for the best fit block
	size_t bestSize = SIZE_MAX;
	block * bestBlock = NULL;
	block * bestPrev = NULL;
	// use two pointers to track
	block * prevBlock = NULL;
	block * curBlock = *head;
	while(curBlock != NULL) {
		int remainSize = curBlock->size - size;
		if(remainSize == 0) {
			// current block is head block
			if(prevBlock == NULL) {
				*head = curBlock -> next;
			} else {
				prevBlock -> next = curBlock -> next;
			}
			return (void*)(curBlock + 1);
		}
		if(remainSize > 0 && remainSize < bestSize) {
			bestSize = remainSize;
			bestBlock = curBlock;
		  	bestPrev = prevBlock;
		}
		prevBlock = curBlock;
		curBlock = curBlock->next;
	}
	// if no block has enough space
	if(bestBlock == NULL) {
		return (*func)(size);
	}
	trySplit(bestPrev, bestBlock, size, head);
  	return (void*)(bestBlock + 1);
}

void my_free(void * ptr, block ** head) {
	if(ptr == NULL) {
		return;
	}
	block * ptrBlock = (block*)((char*)ptr - BLOCKSIZE);
	if(*head == NULL) {
		ptrBlock->next = NULL;
		*head = ptrBlock;
		return;
	}
	// if the block should be inserted in the head
	if(ptrBlock < *head) {
		ptrBlock->next = *head;
		tryMerge(NULL, ptrBlock, *head);
		*head = ptrBlock;
		return;
	}
	// if the block should be inserted after the head
	block * curr = *head;
	while(curr->next != NULL && ptrBlock > curr->next) {
		curr = curr->next;
	}
	block * temp = curr->next;
	curr->next = ptrBlock;
	ptrBlock->next = temp;
	tryMerge(curr, ptrBlock, temp);
	return;
}

/* 
	Thread Safe malloc/free: locking version
*/

// get new memory block
void * getNewMemory(size_t size) {
	// compare the size with pagesize
	void * new = sbrk(size + BLOCKSIZE);
	// sbrk failed
	if(new == (void*) - 1) {
		return NULL;
	}
	block * newBlock = new;
	newBlock->size = size;
	newBlock->next = NULL;
	return (void*)(newBlock + 1);
}


void *ts_malloc_lock(size_t size) {
	pthread_mutex_lock(&lock);
	void * bestBlock = bf_malloc(size, &headBlock, getNewMemory);
	pthread_mutex_unlock(&lock);
  	return bestBlock;
}

void ts_free_lock(void * ptr) {
	pthread_mutex_lock(&lock);
	my_free(ptr, &headBlock);
	pthread_mutex_unlock(&lock);
	return;
}

/*
	Thread Safe malloc/free: non-locking version 
*/

// get new memory block
void * tlsGetNewMemory(size_t size) {
	// compare the size with pagesize
	pthread_mutex_lock(&lock);
	void * new = sbrk(size + BLOCKSIZE);
	// sbrk failed
	if(new == (void*) - 1) {
		pthread_mutex_unlock(&lock);
		return NULL;
	}
	block * newBlock = new;
	newBlock->size = size;
	newBlock->next = NULL;
	pthread_mutex_unlock(&lock);
	return (void*)(newBlock + 1);
}

void *ts_malloc_nolock(size_t size) {
	void * bestBlock = bf_malloc(size, &tlsHeadBlock, tlsGetNewMemory);
	return bestBlock;
}

void ts_free_nolock(void *ptr) {
	my_free(ptr, &tlsHeadBlock);
	return;
}
