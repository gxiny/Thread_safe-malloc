#ifndef _MY_MALLOC_H
#define _MY_MALLOC_H
#define __USE_C99_MATH
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct _block{
  size_t size;
  struct _block *prev;
  struct _block *next;
  //bool empty;            //true is empty
  // void *ptr;               // a pointer to the allocated block
  // char data[1];           //first char of the block
}block;

#define block_size sizeof(block)

size_t is_8(size_t size);



block *bf_block(size_t size);

block *add_new_block(size_t size);

void seperate_block(block *current,size_t size);

block *merge_block(block *current);

block *get_block(void *p);

//int valid_addr(void *p);

//first fit malloc/free

//Best fit malloc/free
void *ts_malloc_lock(size_t size);

void ts_free_lock(void *p);

void *ts_malloc_nolock(size_t size);

void ts_free_nolock(void *p);

unsigned long get_data_segment_size();

unsigned long get_data_segment_free_space_size();

#endif
