#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "my_malloc.h"
#define __USE_C99_MATH
#include <stdbool.h>
#include "pthread.h"
#define block_size sizeof(block)

pthread_mutex_t lock1 =  PTHREAD_MUTEX_INITIALIZER;

size_t is_8(size_t size){
  if(size % 8 == 0) return size;
  return ((size>>3)+1)<<3;
}
void *first_block = NULL;

__thread void *first_block_nolock = NULL;

block *bf_block(size_t size){
  block *current  = first_block; 
  block *bf_block = NULL;
  while(current!=NULL){                                               //search through the first list to find the suitable block
    if(current->size>=size){
      if(bf_block==NULL || current->size <bf_block->size){ 
	bf_block = current;
	if(current->size == size) break;
      }
    }
    current = current->next;
  }
  if(bf_block == NULL) return NULL;                                  // remove the block from the free list block
  if(bf_block == first_block){
    first_block = bf_block->next;
    if(bf_block->next!=NULL){
      bf_block->next->prev = NULL;
    }
  }
  else{
    bf_block->prev->next = bf_block->next;
    if(bf_block->next!=NULL){
      bf_block->next->prev = bf_block->prev;
    }
  }
  return bf_block;
}

block *bf_block_nolock(size_t size){
  block *current  = first_block_nolock;
  block *bf_block = NULL;
  while(current!=NULL){
    if(current->size>=size){
      if(bf_block==NULL || current->size <bf_block->size){
	bf_block = current;
	if(current->size == size) break;
      }
    }
    current = current->next;
  }

  if(bf_block == NULL) return NULL;
  if(bf_block->size>8+size+block_size){
    seperate_block(bf_block,size);
  }
  if(bf_block == first_block_nolock){
    first_block_nolock = bf_block->next;
    if(bf_block->next!=NULL){
      bf_block->next->prev = NULL;
    }
  }
  else{
    bf_block->prev->next = bf_block->next;
    if(bf_block->next!=NULL){
      bf_block->next->prev = bf_block->prev;
    }
  }
  return bf_block;
}

block *add_new_block(size_t size){
  block *add;
  add = sbrk(0);
  block *new_block = sbrk(size+block_size);           // create space   the size need to be 8
  assert((void *) new_block == add);
  if(new_block==(void*)-1){                           //sbrk failed
    return NULL;
  }
  add->size = size;
  add->prev = NULL;
  add->next = NULL;
  return add;
}


block *add_new_block_nolock(size_t size){

  pthread_mutex_lock(&lock1);
  block *new_block = sbrk(size+block_size);           // create space   the size need to be 8
  pthread_mutex_unlock(&lock1);
  new_block->size = size;
  new_block->prev = NULL;
  new_block->next = NULL;
  return new_block;
}

void seperate_block(block *current,size_t size){
  block *unused_block;
  unused_block = (block*)((size_t)current+size+block_size);
  unused_block->size = current->size - size - block_size;
  unused_block->next = current->next;
  unused_block->prev = current;
  current->size = size;
  current->next = unused_block;
  if(unused_block->next!=NULL){
    unused_block->next->prev = unused_block;
  }
}
void *ts_malloc_lock(size_t size){
  if(size<=0) return NULL;
  pthread_mutex_lock(&lock1);
  if(first_block == NULL){
    first_block = sbrk(block_size);
  }
  block *target = NULL;
  size_t s = is_8(size);

  if(first_block!=NULL){
    target = bf_block(s);
   // if(target!=NULL){
    //   pthread_mutex_unlock(&lock1);
    //   return (void *)((size_t)target+block_size);
   // }
    if(target==NULL){
      target = add_new_block(s);
      if(target==NULL) return NULL;
      //pthread_mutex_unlock(&lock1);
    }
  }
  else{
    target = add_new_block(s);
    if(target == NULL) return NULL;
    //pthread_mutex_unlock(&lock1);
  }
  pthread_mutex_unlock(&lock1);
  return (void *)((size_t)target+block_size);
}



block *merge_block(block *current){
  if(current->next!=NULL && ((size_t)current+current->size+block_size == (size_t)(current->next))){
    current->size += current->next->size + block_size;
    current->next = current->next->next;
    if(current->next){
      current->next->prev = current;
    }
  }
  return current;
}

//check if the address is valid
block *get_block(void *p){
  size_t temp = (size_t)p - block_size;
  return (block *)temp;
}

void ts_free_lock(void *p){
  if(p==NULL) return;
  pthread_mutex_lock(&lock1);
  block *current;
  block *free_block;
  free_block = get_block(p);
  current = first_block;
  //pthread_mutex_lock(&lock1);
  if(current == NULL){
    first_block = free_block;
    free_block->next = NULL;
    free_block->prev = NULL;
    return;
  }
  while(current!=NULL && current->next!=NULL && (size_t)free_block>(size_t)current){
    current = current->next;
  }

  if((size_t)free_block < (size_t)current){
    if(current == first_block){
      first_block = free_block;
      free_block->prev = NULL;
      free_block->next = current;
      if(current!=NULL){
	current->prev = free_block;
      }
    }
    else {
      free_block->next =  current;
      free_block->prev = current->prev;
      current->prev->next = free_block;
      current->prev = free_block;
    }
  }
  else if((size_t)free_block>(size_t)current){
    current->next = free_block;
    free_block->prev = current;
    free_block->next = NULL;
  }

   if(free_block->prev && ((size_t)free_block->prev+block_size+(free_block->prev->size) == (size_t)free_block)){
    free_block = merge_block(free_block->prev);
  }
  if(free_block->next && ((size_t)free_block+block_size+free_block->size == (size_t)free_block->next)) free_block = merge_block(free_block);
  
  pthread_mutex_unlock(&lock1);
}


void *ts_malloc_nolock(size_t size){
  if(size<=0) return NULL;
  if(first_block_nolock == NULL){
    pthread_mutex_lock(&lock1);
    first_block_nolock = sbrk(block_size);
    pthread_mutex_unlock(&lock1);
  }
  block *target = NULL;
  size_t s = is_8(size);
  if(first_block_nolock!=NULL){
    target = bf_block_nolock(s);
   // if(target!=NULL){
    //   pthread_mutex_unlock(&lock1);
    //   return (void *)((size_t)target+block_size);
   // }
    if(target==NULL){
      target = add_new_block_nolock(s);
      if(target==NULL) return NULL;
    }
  }
  else{
    target = add_new_block_nolock(s);
    if(target == NULL) return NULL;
  }
  return (void *)((size_t)target+block_size);
}

void ts_free_nolock(void *p){
  if(p==NULL) return;
  block *current;
  block *free_block;
  free_block = get_block(p);
  current = first_block_nolock;
  if(current == NULL){
    first_block_nolock = free_block;
    free_block->next = NULL;
    free_block->prev = NULL;
    return;
  }
  while(current!=NULL && current->next!=NULL && (size_t)free_block>(size_t)current){
    current = current->next;
  }

  if((size_t)free_block < (size_t)current){
    if(current == first_block_nolock){
      first_block_nolock = free_block;
      free_block->prev = NULL;
      free_block->next = current;
      if(current!=NULL){
	current->prev = free_block;
      }
    }
    else {
      free_block->next =  current;
      free_block->prev = current->prev;
      current->prev->next = free_block;
      current->prev = free_block;
    }
  }
  else if((size_t)free_block>(size_t)current){
    current->next = free_block;
    free_block->prev = current;
    free_block->next = NULL;
  }

   if(free_block->prev && ((size_t)free_block->prev+block_size+(free_block->prev->size) == (size_t)free_block)){
    free_block = merge_block(free_block->prev);
  }
  if(free_block->next && ((size_t)free_block+block_size+free_block->size == (size_t)free_block->next)) free_block = merge_block(free_block);

}



