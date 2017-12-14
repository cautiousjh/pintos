#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define NUM_DIRECT_BLOCK 10
#define NUM_INDIRECT_BLOCK (BLOCK_SECTOR_SIZE / 4)
#define MAX_DIRECT (NUM_DIRECT_BLOCK*BLOCK_SECTOR_SIZE)
#define MAX_INDIRECT (NUM_INDIRECT_BLOCK*BLOCK_SECTOR_SIZE)
#define NULL_SECTOR 4294967295

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */

    block_sector_t direct_idx[NUM_DIRECT_BLOCK];
    block_sector_t indirect_idx;
    block_sector_t double_indirect_idx;
    uint32_t unused[113];               /* Not used. */
  };

bool inode_extend(struct inode_disk *disk_inode, off_t length);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  block_sector_t indirect_block[NUM_INDIRECT_BLOCK];
  block_sector_t double_indirect_block[NUM_INDIRECT_BLOCK];
  ASSERT (inode != NULL);
  if(pos >= inode->data.length)
    return NULL_SECTOR;

  // direct
  if(pos < MAX_DIRECT)
    return inode->data.direct_idx[pos / BLOCK_SECTOR_SIZE];
  // indirect
  else if (pos < MAX_DIRECT + MAX_INDIRECT){
    block_read(fs_device, inode->data.indirect_idx, &indirect_block);
    return indirect_block[pos / BLOCK_SECTOR_SIZE - NUM_DIRECT_BLOCK];
  }
  // double indirect
  else{
    block_read(fs_device, inode->data.double_indirect_idx, &indirect_block);
    block_read(fs_device, 
      double_indirect_block[(pos/BLOCK_SECTOR_SIZE-MAX_DIRECT-MAX_INDIRECT)/NUM_INDIRECT_BLOCK], 
      &double_indirect_block);
    return double_indirect_block[(pos/BLOCK_SECTOR_SIZE-MAX_DIRECT-MAX_INDIRECT)%NUM_INDIRECT_BLOCK];
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  cache_init();
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  int i;
  //struct inode_disk *disk_inode = NULL;
  struct inode_disk disk_inode;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  //ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  //disk_inode = calloc (1, sizeof *disk_inode);
  if (1)//disk_inode != NULL)
    {
      // initialization
      disk_inode.length = 0;
      disk_inode.magic = INODE_MAGIC;
      for(i=0;i<MAX_DIRECT;i++)
        disk_inode.direct_idx[i] = NULL_SECTOR;
      disk_inode.indirect_idx = NULL_SECTOR;
      disk_inode.double_indirect_idx = NULL_SECTOR;
      //disk_inode->start = sector;
      success = inode_extend(&disk_inode,length);
      if(success)
        block_write(fs_device,sector, &disk_inode);
      //free(disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  cache_read(inode->sector, &inode->data);
  //block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          cache_read (sector_idx, buffer + bytes_read);
          //block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          cache_read (sector_idx, bounce);
          //block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if(inode_length(inode) < size+offset)
    inode_extend(&inode->data, size+offset);

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          cache_write(sector_idx, buffer + bytes_written);
          //block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            cache_read (sector_idx, bounce);
            //block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cache_write(sector_idx, bounce);
          //block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_extend(struct inode_disk *disk_inode, off_t length)
{
  int i,j;
  bool flag = false;
  int num_to_extend;
  char zeros[BLOCK_SECTOR_SIZE] = {0};
  block_sector_t indirect_block[NUM_INDIRECT_BLOCK];
  block_sector_t double_indirect_block[NUM_INDIRECT_BLOCK];

  //initialization
  num_to_extend = bytes_to_sectors(length) - bytes_to_sectors(disk_inode->length);
  ASSERT(num_to_extend>=0);
  for(i=0;i<NUM_INDIRECT_BLOCK;i++){
    indirect_block[i]=NULL_SECTOR;
    double_indirect_block[i]=NULL_SECTOR;
  }

  // read direct index
  for(i=0;i<NUM_DIRECT_BLOCK&&num_to_extend;i++)
    if(disk_inode->direct_idx[i] == NULL_SECTOR){
      if(free_map_allocate(1,&disk_inode->direct_idx[i])){ // set sector
        block_write(fs_device, disk_inode->direct_idx[i], &zeros);
        num_to_extend--;
      }
      else
        return false;
    }

  // if indirect index already exists
  if(disk_inode->indirect_idx != NULL_SECTOR && num_to_extend)
    block_read(fs_device, disk_inode->indirect_idx, &indirect_block);
  // else, allocate indirect index
  else if(disk_inode->indirect_idx == NULL_SECTOR && num_to_extend){
    if(free_map_allocate(1,&disk_inode->indirect_idx))  // set sector
      block_write(fs_device, disk_inode->indirect_idx, &indirect_block);
    else
      return false;
  }

  // allocate indirect index
  for(i=0;i<NUM_INDIRECT_BLOCK&&num_to_extend;i++){
    if(indirect_block[i]==NULL_SECTOR){
      if(free_map_allocate(1, &indirect_block[i])){
        block_write(fs_device, indirect_block[i], &zeros);
        num_to_extend--;
        flag = true;
      }
      else
        return false;
    }
  }
  // write changed indirect indexes
  if(flag)
    block_write(fs_device, disk_inode->indirect_idx, &indirect_block);

  // double indirect index already exists
  if(disk_inode->double_indirect_idx !=NULL_SECTOR && num_to_extend)
    block_read(fs_device, disk_inode->double_indirect_idx, &double_indirect_block);
  // allocate double indirect index
  else if(disk_inode->double_indirect_idx == NULL_SECTOR && num_to_extend){
    if(free_map_allocate(1, &disk_inode->double_indirect_idx))
      block_write(fs_device,disk_inode->double_indirect_idx, &double_indirect_block);
    else 
      return false;
  }

  // allocate double indirect index
  for(i=0;i<NUM_INDIRECT_BLOCK && num_to_extend; i++){
    // allocate double indirect
    flag = false;
    if(double_indirect_block[i] == NULL_SECTOR){
      if(free_map_allocate(1,&double_indirect_block[i])){
        block_write(fs_device,double_indirect_block[i], &indirect_block);
        flag = true;
      }
      else
        return false;
    }

    // reset stuffs
    for(j=0;j<NUM_INDIRECT_BLOCK;j++)
      indirect_block[j] = NULL_SECTOR;
    bool flag2 = false;

    // (from double idx)indirect index already exists
    if(double_indirect_block[i] != NULL_SECTOR && num_to_extend )
      block_read(fs_device,double_indirect_block[i], &indirect_block);
    // else, allocate single indirect
    else if(double_indirect_block[i] == NULL_SECTOR && num_to_extend){
      if(free_map_allocate(1,&double_indirect_block[i]))
        block_write(fs_device,double_indirect_block[i],&indirect_block);
      else
        return false;
    }

    // allocate single indirects
    for(j=0;j<NUM_INDIRECT_BLOCK && num_to_extend; j++){
      if(indirect_block[j] == NULL_SECTOR){
        if(free_map_allocate(1,&indirect_block[j])){
          block_write(fs_device,indirect_block[j],&zeros);
          num_to_extend--;
          flag2 = true;
        }
        else
          return false;
      }
    }
    // write changed (single)indirect blocks
    if(flag2)
      block_write(fs_device, double_indirect_block[i], &indirect_block);
  }
  // wirte changed double indirect blocks
  if(flag)
    block_write(fs_device, disk_inode->double_indirect_idx, &double_indirect_block);

  // num_to_extend should be 0
  if(i==NUM_INDIRECT_BLOCK && num_to_extend)
    return false;

  disk_inode->length = length;
  return true;
}