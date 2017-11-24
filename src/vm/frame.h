#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "filesys/off_t.h"
#include "vm/swap.h"

struct list frames;

struct frame{
	void* kpage;
	void* upage;
	struct thread* t;
	struct page* related_page;
	struct list_elem elem;
};

void frames_init(void);
void* frame_alloc(struct frame* f);
void* frame_evict(struct frame* f);
void* frame_swap(struct frame* victim);
void free_frame (struct frame* f);

#endif