#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "filesys/off_t.h"

static struct lock vmlock;
static struct hash frames;


struct frame{
	void* kpage;
	void* upage;
	struct thread* t;
	struct hash_elem elem;
}

static void frames_init();
static void* frame_alloc(struct frame* f);
static void* frame_evict(struct frame* f);

unsigned frame_hash_func(const struct hash_elem *e,void *aux);
bool frame_less_func(const struct hash_elem *a,
                     const struct hash_elem *b,
                     void *aux);


#endif