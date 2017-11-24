#include "vm/frame.h"
#include <debug.h>

#include "threads/palloc.h"
#include "userprog/pagedir.h"

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

	if(new_frame->kpage){
      	list_push_back(&frames,&new_frame->elem);
		lock_release(&frame_lock);
		return new_frame->kpage;
	}
	else{
		frame_evict(new_frame);

		new_frame->kpage = palloc_get_page(PAL_USER);
		new_frame->t = thread_current();
    	pagedir_set_accessed(new_frame->t->pagedir, new_frame->kpage, true);

      	list_push_back(&frames,&new_frame->elem);
		lock_release(&frame_lock);
		return new_frame->kpage;
	}
}

void
free_frame (struct frame* f)
{
	lock_acquire(&frame_lock);
  	list_remove(&f->elem);
	palloc_free_page(f->kpage);
	lock_release(&frame_lock);
}


void* 
frame_evict(struct frame* f) 
{
	struct frame* temp_frame;
	struct list_elem* iter;
	bool finished = false;

  	for(iter = list_begin(&frames);
      	iter != list_end(&frames);
      	iter = iter->next){
  		if(finished) break;
  		temp_frame = list_entry(iter, struct frame, elem);
		if(temp_frame->kpage)
			if(pagedir_is_accessed(temp_frame->t->pagedir, temp_frame->related_page->addr))
				pagedir_set_accessed(temp_frame->t->pagedir, temp_frame->related_page->addr, false);
			else
				finished = true;
  	}

  	for(iter = list_begin(&frames);
      	iter != list_end(&frames);
      	iter = iter->next){
  		if(finished) break;
		temp_frame = list_entry(iter, struct frame, elem);
		if(temp_frame->kpage)
			if(!pagedir_is_dirty(temp_frame->t->pagedir, temp_frame->related_page->addr))
				finished = true;
  	}

  	for(iter = list_begin(&frames);
      	iter != list_end(&frames);
      	iter = iter->next){
  		if(finished) break;
		temp_frame = list_entry(iter, struct frame, elem);
		if(temp_frame->kpage)
				finished = true;
  	}
	
	frame_swap(temp_frame);  
  	list_remove(&temp_frame->elem);
	//free(temp_frame);
	return;		
	
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