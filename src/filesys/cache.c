#include "filesys/cache.h"
#include <stdlib.h>
#include <stdbool.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/block.h"
#include "devices/timer.h"
#include "filesys/filesys.h"

struct cache_block{

	block_sector_t sector;
	uint8_t data[BLOCK_SECTOR_SIZE];
	bool isDirty;

	// for synch
	int reader_cnt;
	bool hasWriter;
	struct lock cache_lock;
	struct condition cache_condvar;

};

struct cache_block* cache_array;
int cache_history[CACHE_SIZE_MAX];
struct lock eviction_lock;

struct cache_block* get_cache_block(block_sector_t sector);
void update_cache_history(int target);
void cache_read_lock(struct cache_block* c);
void cache_read_unlock(struct cache_block* c);
void cache_write_lock(struct cache_block* c);
void cache_write_unlock(struct cache_block* c);

void write_back_thread_function(void* aux);


void cache_init(void)
{
	int i;
	lock_init(&eviction_lock);
	cache_array = (struct cache_block*)malloc(CACHE_SIZE_MAX*sizeof(struct cache_block));
	for(i=0;i<CACHE_SIZE_MAX;i++){
		// for cache history
		cache_history[i] = i;
		// for each cache entry
		cache_array[i].sector = -1;
		cache_array[i].isDirty = false;
		cache_array[i].reader_cnt = 0;
		cache_array[i].hasWriter = false;
		lock_init(&cache_array[i].cache_lock);
		cond_init(&cache_array[i].cache_condvar);
	}
	//thread_create("bgndWriteBackThread",0,write_back_thread_function,NULL);
}

struct cache_block* get_cache_block(block_sector_t sector)
{
	struct cache_block* iter_cache;
	struct cache_block* target_cache = NULL;
	int i;

	//[TODO][CHECK] should entry_lock / rw_lock be separated?
	// get target cache
	for(i=0;i<CACHE_SIZE_MAX;i++){
		iter_cache = cache_array + i;
		lock_acquire(&iter_cache->cache_lock);
		if(iter_cache->sector == sector){
			target_cache = iter_cache;
			lock_release(&iter_cache->cache_lock);
			break;
		}
		lock_release(&iter_cache->cache_lock);
	}

	// if cache fault ->eviction
	if (!target_cache){
		// get victim entry
		lock_acquire(&eviction_lock);
		target_cache = cache_array + cache_history[0];
		cache_read_lock(target_cache);
		// write to disk if dirty
		if(target_cache->isDirty)
			block_write(fs_device, target_cache->sector,target_cache->data);
		update_cache_history(cache_history[0]);
		// update cache_block info
		target_cache->sector = sector;
		target_cache->isDirty = false;
		target_cache->reader_cnt = 0;
		target_cache->hasWriter = false;
		cache_read_unlock(target_cache);
		// read data from disk
		cache_write_lock(target_cache);
		block_read(fs_device,target_cache->sector,target_cache->data);
		cache_write_unlock(target_cache);
		lock_release(&eviction_lock);
	}
	return target_cache;
}

void cache_read(block_sector_t sector, void* buffer)
{
	struct cache_block* target_cache = get_cache_block(sector);
	cache_read_lock(target_cache);
	memcpy(buffer, target_cache->data, BLOCK_SECTOR_SIZE);
	cache_read_unlock(target_cache);

}
void cache_write(block_sector_t sector, void* buffer)
{
	struct cache_block* target_cache = get_cache_block(sector);
	cache_write_lock(target_cache);
	memcpy(target_cache->data, buffer, BLOCK_SECTOR_SIZE);
	cache_write_unlock(target_cache);
}
void cache_flush(){
	int i;
	struct cache_block* iter_cache;
	if(!cache_array){
		for(i=0;i<CACHE_SIZE_MAX;i++){
			iter_cache = cache_array + i;
			if(iter_cache->isDirty){
				iter_cache->isDirty = false;
				block_write(fs_device, iter_cache->sector,iter_cache->data);
			}
		}
	}
}


void update_cache_history(int target){
	int i, j, found;
	for(i=0;i<CACHE_SIZE_MAX;i++)
		if(cache_history[i] == target){
			for(j=i;j<CACHE_SIZE_MAX-1;j++)
				cache_history[j] = cache_history[j+1];
			cache_history[CACHE_SIZE_MAX-1] = target;
		}
}


void cache_read_lock(struct cache_block* c){
	lock_acquire(&c->cache_lock);
	while(c->hasWriter)
		cond_wait(&c->cache_condvar,&c->cache_lock);
	c->reader_cnt++;
	lock_release(&c->cache_lock);
}
void cache_read_unlock(struct cache_block* c){
	lock_acquire(&c->cache_lock);
	c->reader_cnt--;
	if(c->reader_cnt==0)
		cond_broadcast(&c->cache_condvar,&c->cache_lock);
	lock_release(&c->cache_lock);
}
void cache_write_lock(struct cache_block* c){
	lock_acquire(&c->cache_lock);
	c->isDirty = true;
	if(c->hasWriter || c->reader_cnt > 0)
		cond_wait(&c->cache_condvar,&c->cache_lock);
	lock_release(&c->cache_lock);
}
void cache_write_unlock(struct cache_block* c){
	lock_acquire(&c->cache_lock);
	c->hasWriter = false;
	cond_broadcast(&c->cache_condvar,&c->cache_lock);
	lock_release(&c->cache_lock);
}


void write_back_thread_function(void* aux UNUSED){
	int i;
	while(true){
		timer_sleep(WRITE_BACK_PERIOD);
		cache_flush();
	}
}