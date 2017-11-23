#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "filesys/off_t.h"

struct hash frames;


struct frame{
	void* kpage;
	void* upage;
	struct thread* t;
	struct page* related_page;
	struct hash_elem elem;
};

void frames_init(void);
void* frame_alloc(struct frame* f);
void* frame_evict(struct frame* f);
void* frame_swap(struct frame* new_frame, struct frame* victim);
void free_frame (struct frame* f);

unsigned frame_hash_func(const struct hash_elem *e,void *aux);
bool frame_less_func(const struct hash_elem *a,
                     const struct hash_elem *b,
                     void *aux);


#endif