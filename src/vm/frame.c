#include "vm/frame.h"
#include <debug.h>

#include "threads/palloc.h"

static struct lock frame_lock;

void
frames_init(void)
{
	lock_init(&frame_lock);
	list_init(&frames);
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
      	list_push_back(&frames,&new_frame->elem);
		return new_frame->kpage;
	}
	else{
		lock_acquire(&frame_lock);
		frame_evict(new_frame);

		new_frame->kpage = palloc_get_page(PAL_USER);
		new_frame->t = thread_current();
    	pagedir_set_accessed(new_frame->t->pagedir, new_frame->kpage, true);
		lock_release(&frame_lock);

      	list_push_back(&frames,&new_frame->elem);
		return new_frame->kpage;
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
	struct list_elem* iter;

  	for(iter = list_begin(&frames);
      	iter != list_end(&frames);
      	iter = iter->next){
  		temp_frame = list_entry(iter, struct frame, elem);
		if(temp_frame->kpage)
			if(pagedir_is_accessed(temp_frame->t->pagedir, temp_frame->related_page->addr))
				pagedir_set_accessed(temp_frame->t->pagedir, temp_frame->related_page->addr, false);
			else
				frame_swap(temp_frame);  		
  	}

  	for(iter = list_begin(&frames);
      	iter != list_end(&frames);
      	iter = iter->next){
		temp_frame = list_entry(iter, struct frame, elem);
		if(temp_frame->kpage)
			if(!pagedir_is_dirty(temp_frame->t->pagedir, temp_frame->related_page->addr))
				frame_swap(temp_frame); 		
  	}

  	for(iter = list_begin(&frames);
      	iter != list_end(&frames);
      	iter = iter->next){
		temp_frame = list_entry(iter, struct frame, elem);
		if(temp_frame->kpage)
			frame_swap(temp_frame); 		
  	}
	free(temp_frame);
	return NULL;
}


void*
frame_swap(struct frame* victim)
{
	// memory to swap_disk
	swap_out(victim->related_page);
	victim->related_page->frame_entry = NULL;
	victim->related_page->status = IN_SWAP_TABLE;
	victim->related_page->kpage = victim->kpage;
	palloc_free_page(victim->kpage);
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