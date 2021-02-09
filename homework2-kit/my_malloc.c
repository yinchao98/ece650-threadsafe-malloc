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

/* 
	Thread Safe malloc/free: locking version
*/

// get new memory block
block * getNewMemory(size_t size) {
	// compare the size with pagesize
	void * new = sbrk(size + BLOCKSIZE);
	// sbrk failed
	if(new == (void*) - 1) {
		return NULL;
	}
	block * newBlock = new;
	newBlock->size = size;
	newBlock->next = NULL;
	return newBlock;
}

void *ts_malloc_lock(size_t size) {
	pthread_mutex_lock(&lock);
	// size cannot be 0, return NULL
	if(size == 0) {
		pthread_mutex_unlock(&lock);
		return NULL;
	}
	// no free memory blocks, request for new memory block
	if(headBlock == NULL) {
		block * newBlock = getNewMemory(size);
		if (newBlock == NULL) {
			pthread_mutex_unlock(&lock);
			return NULL;
		}
		pthread_mutex_unlock(&lock);
		return (void*)(newBlock + 1);
	}
	// have memory blocks, look for the best fit block
	size_t bestSize = SIZE_MAX;
	block * bestBlock = NULL;
	block * bestPrev = NULL;
	// use two pointers to track
	block * prevBlock = NULL;
	block * curBlock = headBlock;
	while(curBlock != NULL) {
		int remainSize = curBlock->size - size;
		if(remainSize == 0) {
			// current block is head block
			if(prevBlock == NULL) {
				headBlock = curBlock -> next;
			} else {
				prevBlock -> next = curBlock -> next;
			}
			pthread_mutex_unlock(&lock);
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
		block * newBlock = getNewMemory(size);
		if (newBlock == NULL) {
			pthread_mutex_unlock(&lock);
			return NULL;
		}
		pthread_mutex_unlock(&lock);
		return (void*)(newBlock + 1);
	}
	trySplit(bestPrev, bestBlock, size, &headBlock);
	pthread_mutex_unlock(&lock);
  	return (void*)(bestBlock + 1);
}

void ts_free_lock(void * ptr) {
	pthread_mutex_lock(&lock);
	if(ptr == NULL) {
		pthread_mutex_unlock(&lock);
		return;
	}
	block * ptrBlock = (block*)((char*)ptr - BLOCKSIZE);
	if(headBlock == NULL) {
		ptrBlock->next = NULL;
		headBlock = ptrBlock;
		pthread_mutex_unlock(&lock);
		return;
	}
	// if the block should be inserted in the head
	if(ptrBlock < headBlock) {
		ptrBlock->next = headBlock;
		tryMerge(NULL, ptrBlock, headBlock);
		headBlock = ptrBlock;
		pthread_mutex_unlock(&lock);
		return;
	}
	// if the block should be inserted after the head
	block * curr = headBlock;
	while(curr->next != NULL && ptrBlock > curr->next) {
		curr = curr->next;
	}
	block * temp = curr->next;
	curr->next = ptrBlock;
	ptrBlock->next = temp;
	tryMerge(curr, ptrBlock, temp);
	pthread_mutex_unlock(&lock);
	return;
}

/*
	Thread Safe malloc/free: non-locking version 
*/

// get new memory block
block * tlsGetNewMemory(size_t size) {
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
	return newBlock;
}

void *ts_malloc_nolock(size_t size) {
	// size cannot be 0, return NULL
	if(size == 0) {
		return NULL;
	}
	// no free memory blocks, request for new memory block
	if(tlsHeadBlock == NULL) {
		block * newBlock = tlsGetNewMemory(size);
		if (newBlock == NULL) {
			return NULL;
		}
		return (void*)(newBlock + 1);
	}
	// have memory blocks, look for the best fit block
	size_t bestSize = SIZE_MAX;
	block * bestBlock = NULL;
	block * bestPrev = NULL;
	// use two pointers to track
	block * prevBlock = NULL;
	block * curBlock = tlsHeadBlock;
	while(curBlock != NULL) {
		int remainSize = curBlock->size - size;
		if(remainSize == 0) {
			// current block is head block
			if(prevBlock == NULL) {
				tlsHeadBlock = curBlock -> next;
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
		block * newBlock = tlsGetNewMemory(size);
		if (newBlock == NULL) {
			return NULL;
		}
		return (void*)(newBlock + 1);
	}
	trySplit(bestPrev, bestBlock, size, &tlsHeadBlock);
  	return (void*)(bestBlock + 1);
}

void ts_free_nolock(void *ptr) {
	if(ptr == NULL) {
		return;
	}
	block * ptrBlock = (block*)((char*)ptr - BLOCKSIZE);
	if(tlsHeadBlock == NULL) {
		ptrBlock->next = NULL;
		tlsHeadBlock = ptrBlock;
		return;
	}
	// if the block should be inserted in the head
	if(ptrBlock < tlsHeadBlock) {
		ptrBlock->next = tlsHeadBlock;
		tryMerge(NULL, ptrBlock, tlsHeadBlock);
		tlsHeadBlock = ptrBlock;
		return;
	}
	// if the block should be inserted after the head
	block * curr = tlsHeadBlock;
	while(curr->next != NULL && ptrBlock > curr->next) {
		curr = curr->next;
	}
	block * temp = curr->next;
	curr->next = ptrBlock;
	ptrBlock->next = temp;
	tryMerge(curr, ptrBlock, temp);
	return;
}