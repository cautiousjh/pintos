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
	lock_acquire(&frame_lock);
	new_frame->kpage = palloc_get_page(PAL_USER);
	new_frame->t = thread_current();
    pagedir_set_accessed(new_frame->t->pagedir, new_frame->kpage, true);
	lock_release(&frame_lock);

	if(new_frame->kpage){
      	hash_insert(&frames,&new_frame->elem);
		return new_frame->kpage;
	}
	else{
		lock_acquire(&frame_lock);
		frame_evict(new_frame);
		lock_release(&frame_lock);
		return NULL;
	}

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
	struct frame* temp_frame;
	struct hash_iterator iter;

	lock_acquire(&frame_lock);

	hash_first(&iter, &frames);
	while(hash_next(&iter)){
		temp_frame = hash_entry(hash_cur(&iter), struct frame, elem);
		if(temp_frame->kpage){
			if(pagedir_is_accessed(temp_frame->t->pagedir, temp_frame->related_page->addr))
				pagedir_set_accessed(temp_frame->t->pagedir, temp_frame->related_page->addr, false);
			else{
				lock_release(&frame_lock);
				return frame_swap(f, temp_frame);
			}
		}
	}

	hash_first(&iter, &frames);
	while(hash_next(&iter)){
		temp_frame = hash_entry(hash_cur(&iter), struct frame, elem);
		if(temp_frame->kpage){
			if(!pagedir_is_dirty(temp_frame->t->pagedir, temp_frame->related_page->addr)){
				lock_release(&frame_lock);
				return frame_swap(f, temp_frame);
			}
		}
	}

	hash_first(&iter, &frames);
	while(hash_next(&iter)){
		temp_frame = hash_entry(hash_cur(&iter), struct frame, elem);
		if(temp_frame->kpage){
			lock_release(&frame_lock);
			return frame_swap(f, temp_frame);
		}
	}

	return NULL;
	lock_release(&frame_lock);
}

void*
frame_swap(struct frame* new_frame, struct frame* victim)
{
	// memory to swap_disk
	swap_out(victim->related_page);
	victim->related_page->frame_entry = NULL;
	victim->related_page->status = IN_SWAP_TABLE;
	pagedir_clear_page(victim->t->pagedir, victim->related_page->addr);
	return NULL;

}

unsigned
frame_hash_func(const struct hash_elem *e,void *aux UNUSED)
{
	const struct frame* f = hash_entry(e, struct frame, elem);
	return hash_bytes(&f->upage, sizeof(f->upage));
}

bool 
frame_less_func(const struct hash_elem *_a,
                const struct hash_elem *_b,
                void *aux UNUSED)
{
	struct frame* a = hash_entry(_a, struct frame, elem);
	struct frame* b = hash_entry(_b, struct frame, elem);
	return a->upage != b->upage? a->upage < b->upage : a->t->tid < b->t->tid;
}