#include "vm/frame.h"
#include <debug.h>

#include "threads/palloc.h"

static struct lock frame_lock;

void
frames_init(void)
{
	lock_init(&frame_lock);
	hash_init(&frames, frame_hash_func, frame_less_func, NULL);
}

void*
frame_alloc(struct frame* new_frame)
{
	new_frame = (struct frame*)malloc(sizeof(struct frame));

	lock_acquire(&frame_lock);
	new_frame->kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	new_frame->t = thread_current();
	lock_release(&frame_lock);

	if(new_frame->kpage){
      	hash_insert(&frames,&new_frame->elem);
		return new_frame->kpage;
	}
	else
		return frame_evict(new_frame);

}

void
free_frame (struct frame* f)
{
	lock_acquire(&frame_lock);
	palloc_free_page(f->kpage);
	lock_release(&frame_lock);
}


void* 
frame_evict(struct frame* f)
{
	return NULL;
}

unsigned
frame_hash_func(const struct hash_elem *e,void *aux)
{
	const struct frame* f = hash_entry(e, struct frame, elem);
	return hash_bytes(&f->upage, sizeof(f->upage));
}

bool 
frame_less_func(const struct hash_elem *_a,
                const struct hash_elem *_b,
                void *aux)
{
	struct frame* a = hash_entry(_a, struct frame, elem);
	struct frame* b = hash_entry(_b, struct frame, elem);
	return a->upage != b->upage? a->upage < b->upage : a->t->tid < b->t->tid;
}