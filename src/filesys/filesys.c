#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

struct dir* path_parser(char* path, char* filename);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  cache_init();
  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
  thread_current()->dir_current = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  char filename[512]={0};
  struct dir *dir;

  // set filename and directory
  dir = path_parser(name,filename);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, -1)
                  && dir_add (dir, filename, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  //cache_flush();

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode *inode = NULL;
  char filename[512]={0};
  struct dir *dir;

  // set filename and directory
  dir = path_parser(name,filename);

  if(dir != NULL){
    if(!strcmp(filename,"."))
      return (struct file*)dir;
    else if(!strcmp(filename,".."))
      inode = inode_open(inode_get_parent(dir_get_inode(dir)));
    else if(strlen(filename))
      dir_lookup(dir,filename,&inode);
    else
      return (struct file*)dir;
  }
  else
    return NULL;

  dir_close(dir);

  if(inode){
    if(inode_get_parent(inode) == -1) // if not directory
      return file_open(inode);
    else
      return (struct file*) dir_open(inode);
  }
  else
    return NULL;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;
  char filename[512]={0};
  bool success;

  dir = path_parser(name,filename);

  success =  dir != NULL && dir_remove (dir, filename);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16, ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

struct dir* path_parser(char* path, char* filename){
  struct dir *dir;
  struct dir *curr_dir = thread_current()->dir_current;
  struct inode *inode;
  int i=-1,cnt;
  char *token, *save_ptr;

  // get filename
  for(i=strlen(path);i>=0;i--){
    if(path[i]=='/'){
      path[i] = '\0';
      token = path[i+1];
      strlcpy(filename, token, strlen(token)+1);
    }
  }

  // case: path contains filename only
  if(i==-1){
    strlcpy(filename,path,strlen(path)+1);
    if(!curr_dir)
      return dir_open_root();
    else
      return dir_reopen(curr_dir);
  }

  // get default directory
  if(path[0]=='/')
    dir = dir_open_root();
  else
    dir = dir_reopen(curr_dir);

  // dir parsing
  token = strtok_r(path,"/",&save_ptr);
  while(token){
    if(strlen(token)){
      inode = NULL;
      if(!strcmp(token,"."));
      else if(!strcmp(token,"..")){
        if (inode_get_parent(dir_get_inode(dir)) != ROOT_DIR_SECTOR){
          block_sector_t parent_dir = inode_get_parent(dir_get_inode(dir));
          dir_close(dir);
          inode = inode_open(parent_dir);
          if((dir = dir_open(inode)) == NULL)
            inode_close(inode);
        }
      }
      else if(!dir_lookup(dir, token, &inode) || inode == NULL 
              || inode_get_parent(inode) == -1){  //TODO check inode?
        dir_close(dir);
        inode_close(inode);
        return dir;
      }
      else{
        dir_close(dir);
        dir = dir_open(inode);
      }
    }
    if(dir == NULL)
      return dir;
    token = strtok_r(NULL,"/",&save_ptr);
  }
  return dir;
}

