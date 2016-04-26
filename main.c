//Computer Science 360, Washington State University
//Megan McPherson and Erik Lystad, April 2016

//used http://wiki.osdev.org/Ext2#Directory_Entry as a reference for EXT2

//Erik: mount_root*, rmdir, cd*, creat*, unlink, stat, touch*, close, write
//Megan: mkdir*, ls*, pwd*, link*, symlink, chmod, open, read, lseek, cp

#include "type.h"
#include <stdlib.h>

typedef unsigned int   u32;

MINODE minode[NMINODES];
MINODE *root;
PROC   proc[NPROC], *running;
MOUNT  mount_table[5];
OFT oft;

int dev;
int nblocks, ninodes, bmap, imap, iblock;
char pathname[256], parameter[256], diskName[256];
char *tokenized_pathname[256];
int num_tokens;

int imap, bmap;  // IMAP and BMAP block number
int ninodes, nblocks, nfreeInodes, nfreeBlocks;


int get_block(int fd, int blk, char buffer[ ]) //credit to KCW
{
  lseek(fd, (long)blk*BLKSIZE, 0);
  read(fd, buffer, BLKSIZE);
}

int put_block(int fd, int blk, char buffer[ ]) //credit to KCW
{
  lseek(fd, (long)blk*BLKSIZE, 0);
  write(fd, buffer, BLKSIZE);
}

int tokenize(char* name)
{
  int i = 0;
  char *token, *cwd_name = running->cwd->name;

  //printf("tokenize: name = %s\n", name);
  for(i = 0; i < 256; i++)
    tokenized_pathname[i] = 0;
  num_tokens = i = 0;

  if(strcmp(name, "/") == 0)
  {
    //printf("tokenize: strcmp returned 0\n");
    tokenized_pathname[0] = "/";
    num_tokens = 1;
    return 1;
  }

  token = strtok(name, "/");
  //printf("tokenize: token = %s\n", token);
  while(token != NULL)
  {
    tokenized_pathname[i] = token;
    i++;
    token = strtok(0, "/");
  }
  num_tokens = i;
  printf("pathname has been split\n");
  i = 0;
  while(tokenized_pathname[i] != 0)
  {
    printf("tokenized_pathname[%d] = %s\n", i, tokenized_pathname[i]);
    i++;
  }
  return 1;
}

int get_bit(char *buffer, int index) //get the value of a bit in a block
{
  int byte = index / 8;
  int bit = index % 8;
  
  if(buffer[byte] & (1 << bit))
    return 1;
  else
    return 0;
}

int set_bit(char *buffer, int index, int value) //set the value of a bit in a block
{
  int byte = index / 8;
  int bit = index % 8;

  if(value == 1)
    buffer[byte] |= (1 << bit);
  else
    buffer[byte] &= ~(1 << bit);
}

int clear_bit(char *buffer, int index)
{
  int byte = index / 8;
  int bit = index % 8;
  buffer[index] &= ~(1 << bit);
}

int dec_free_inodes(int dev) //when an inode is allocated, update free_inodes in SUPER and GD blocks
{
  char buffer[BLKSIZE];

  //1. update super block
  get_block(dev, 1, buffer);
  sp = (SUPER *)buffer;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buffer);

  //2. update group descriptor block
  get_block(dev, 2, buffer);
  gp = (GD *)buffer;
  gp->bg_free_inodes_count--;
  put_block(dev, 2, buffer);
}

int dec_free_blocks(int dev) //when a block is allocated, update free_inodes in SUPER and GD blocks
{
  char buffer[BLKSIZE];

  //1. update super block
  get_block(dev, 1, buffer);
  sp = (SUPER *)buffer;
  sp->s_free_blocks_count--;
  put_block(dev, 1, buffer);

  //2. update group descriptor block
  get_block(dev, 2, buffer);
  gp = (GD *)buffer;
  gp->bg_free_blocks_count--;
  put_block(dev, 2, buffer);
}

int inc_free_inodes(int dev) //when an inode is deallocated, update free_inodes in SUPER and GD blocks
{
  char buffer[BLKSIZE];

  //1. update super block
  get_block(dev, 1, buffer);
  sp = (SUPER *)buffer;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buffer);

  //2. update group descriptor block
  get_block(dev, 2, buffer);
  gp = (GD *)buffer;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buffer);
}

int inc_free_blocks(int dev) //when a block is deallocated, update free_inodes in SUPER and GD blocks
{
  char buffer[BLKSIZE];

  //1. update super block
  get_block(dev, 1, buffer);
  sp = (SUPER *)buffer;
  sp->s_free_blocks_count++;
  put_block(dev, 1, buffer);

  //2. update group descriptor block
  get_block(dev, 2, buffer);
  gp = (GD *)buffer;
  gp->bg_free_blocks_count++;
  put_block(dev, 2, buffer);
}

int ialloc(int dev) //allocate a free inode, return its inumber
{
  int  i;
  char buffer[BLKSIZE];
  
  // get the imap block
  get_block(dev, imap, buffer);
 
  for (i=0; i < ninodes; i++) //find the first free inode spot
  {
    if (get_bit(buffer, i) == 0) //if the bit at this buf location is 0, it's free
    {
       set_bit(buffer, i, 1);
       dec_free_inodes(dev); //free inodes--, update SUPER and GD blocks
       put_block(dev, imap, buffer); //write imap block back to device

       return i + 1;
    }
  }
  printf("ialloc(): no more free inodes\n");
  return 0;
}

int balloc(int dev) //allocate a free data block, return its bnumber
{
  int  i;
  char buffer[BLKSIZE];

  //get the bmap block
  get_block(dev, bmap, buffer);
  
  for(i = 0; i < nblocks; i++) //look through all blocks
  {
    if(get_bit(buffer, i) == 0) //if this block is free
    {
      set_bit(buffer, i, 1); //make it not free
      dec_free_blocks(dev); //update super and gd blocks
      put_block(dev, nblocks, buffer); //write block back to device
      return i;
    }
  }
  return 0;
}

idealloc(int dev, int inode_index) //deallocates an inode number
{
  char buffer[BLKSIZE];

  get_block(dev, imap, buffer); //get the imap block
  set_bit(buffer, inode_index - 1, 0); //tell it to be free
  inc_free_inodes(dev); //update super and gd blocks
  put_block(dev, imap, buffer); //write it back to the device
}

bdealloc(int dev, int block_index) //deallocates an block number
{
  char buffer[BLKSIZE];

  get_block(dev, bmap, buffer); //get the imap block
  set_bit(buffer, block_index, 0); //tell it to be free
  inc_free_blocks(dev); //update super and gd blocks
  put_block(dev, bmap, buffer); //write it back to the device
}

int getino(int *device, char *path) //int ino = getino(&dev, pathname) essentially returns (dev,ino) of a pathname
{
  int i = 0, inumber, blocknumber;
  INODE *ip = &(root->INODE);
  char *dname, *copy = path, *cwd_name = running->cwd->name;
  char buffer[1024];

  printf("getino: path = %s\n", path);

  if(strcmp(path, "/") == 0)
    return root->ino;

  //1. update *dev
  if (pathname[0] == '/')
    *device = root->dev;
  else
  {
    *device = running->cwd->dev; //running is a pointer to PROC of current running process.
  }

  tokenize(path);
  printf("tokenized_pathname[i] = %s\n", tokenized_pathname[i]);
  printf("num_tokens = %d\n", num_tokens);
  //dname = dirname(copy);
  //printf("dname: %s\n\n", dname);

  //if(strcmp(dname, ".") == 0)
  //  dname = "/";

  printf("tokenized_pathname[i] = %s\n", tokenized_pathname[i]);
  //2. find and return ino
  for (i = 0; i < num_tokens; i++) //n is number of steps in pathname
  {
    printf("i = %d tokenized_pathname[i] = %s\n", i, tokenized_pathname[i]);
    inumber = inode_search(ip, tokenized_pathname[i]);  
    if (inumber < 1) //: inumber not found
    {
      printf("inode could not be found\n");
      return 0;
    }
    
    blocknumber = ((inumber -1) / 8) + 10;
    get_block(dev, blocknumber, buffer);
    ip = (INODE *)buffer + (inumber - 1) % 8;
  }

 printf("getino: inumber = %d\n", inumber);
 return inumber;
}

//search for a file by name starting with mip
//if found, return inumber. if not, return 0.
int search(MINODE *mip, char *name)
{
  int i = 0, block_position = 0, block;
  char *copy, sbuf[BLKSIZE];

  printf("search: name = %s mip->name = %s dev = %d ino = %d\n", name, mip->name, mip->dev, mip->ino);

  if(strcmp(name, "/") == 0)
    return root->ino;

  for(i = 0; i < 12; i++)
  {
    block_position = 0;
    if(mip->INODE.i_block[i] == 0)
    {
      printf("search: returning 0\n");
      return 0;
    }

    get_block(mip->dev, mip->INODE.i_block[i], sbuf);
    dp = (DIR*)sbuf;
    copy = sbuf;

    printf("i_block[%d] = %d\n", i, mip->INODE.i_block[i]);

    while(block_position < BLKSIZE)
    {
      printf("block_position = %d\n",block_position);
      printf("dp->name = %s\tdp->rec_len = %d\n", dp->name, dp->rec_len);
      if(strcmp(dp->name, name) == 0)
        return dp->inode;

      copy += dp->rec_len;
      dp = (DIR *)copy;
      block_position += dp->rec_len;
    }
  }
  return 0;
}

int inode_search(INODE * inodePtr, char * name)
{
  char buffer[1024];
  printf("searching for: %s\n", name);
  get_block(dev, inodePtr->i_block[0], buffer);

  //read the block into buf 
  //let DIR *dp and char *cp BOTH point at buf
  DIR *dp = (DIR *)buffer;
  char *copy = buffer;

  printf("\n********Root Directory Entries******\n");
  printf("  inode  rec_len  name_len  name\n");

  // search for name string in the data blocks of this INODE
  int block_position = 0;
  while(block_position < BLKSIZE)
    {
      printf("block_position = %d\n",block_position);
      printf("dp->name = %s\tdp->rec_len = %d\n", dp->name, dp->rec_len);
      if(strcmp(dp->name, name) == 0)
        return dp->inode;

      copy += dp->rec_len;
      dp = (DIR *)copy;
      block_position += dp->rec_len;
    }

  //if we've gotten here, the name wasn't found
  return 0;   
}

//This function releases a Minode[] pointed by mip.
int iput(MINODE *mip)
{
  char buffer[BLKSIZE], *position;
  int block, offset;

  mip->refCount--;
  if(mip->refCount > 0 || mip->dirty == 0)
  {
    printf("iput: not doing the thing, refCount = %d\n", mip->refCount);
    return;
  }
  if(mip->refCount == 0 && mip->dirty == 1) //ASK KC ABOUT THIS LOGIC
  {
    //Writing Inode back to disk
    block = (mip->ino - 1) / 8 + INODEBLOCK;
    offset = (mip->ino -1) % 8;
    
    //read block into buf
    get_block(dev, block, buffer);
    position = buffer + offset * 128;
    memcpy(position, &(mip->INODE), 128);
    put_block(mip->dev, block, buffer);
  }
}

int findino(MINODE *mip, int *myino, int *parentino)
{
  *myino = search(mip, ".");
  *parentino = search(mip, "..");
  return *myino;
}

MINODE* iget(int dev, int ino)
{
  char buffer[BLKSIZE];

  MINODE *mip;
  //search MINODE array for inode
  int i = 0, blk, offset;
  for(i = 0; i < NMINODES; i++)
  {
    if(minode[i].refCount > 0 && minode[i].dev == dev && minode[i].ino == ino)
    {
      minode[i].refCount++;
      return &minode[i];
    }
    if(minode[i].refCount == 0) //find a minode whose refCount = 0
    {
      mip = &minode[i];
    }
  }
  
  //use mailman's algorithm to compute
  blk = (ino - 1)/8 + mount_table[0].iblock;
  offset = (ino - 1) % 8;

  //read blk into buf[]
  get_block(dev, blk, buffer); 
  ip = (INODE *)buffer + offset; 
  //printf("ip->i_mode: %d, %d, %d, %d\n", ip->i_mode, ino, dev, blk);
  mip->INODE = *ip;
  //printf("\nis dir? %d\n", is_dir(mip));

  //initialize fields of *mip
  mip->dev = dev;
  mip->ino = ino;
  mip->refCount = 1;
  mip->dirty = 0;
  mip->mounted = 0;
  mip->mountptr = NULL;
  strcpy(mip->name, "");

  return mip;
}

//function finds the name string of myino in the parent's data block
int findmyname(MINODE *parent, int myino, char *myname)
{
  int i;
  char *copy, sbuf[BLKSIZE];
  DIR *dp;
  INODE *ip;

  ip = &(parent->INODE);
  for(i = 0; i < 12; i++)
  {
    if(ip->i_block[0] == 0)
      return 0;

    get_block(dev, ip->i_block[i], sbuf);
    dp = (DIR*)sbuf;
    copy = sbuf;

    while(copy < sbuf + BLKSIZE)
    {
       if(dp->inode == myino)
       {
         strcpy(myname, dp->name);
         return 1;
       }

       copy += dp->rec_len;
       dp = (DIR *)copy;
    }
  }
  return 0;
  
}

char* get_cwd_path(int this_ino)
{
  MINODE *parent_mip, *mip;
  int parent_ino, blocknumber, block_position = 0;
  char cat_name[128], buffer[1024], *string_position;
  DIR *dir;

  if(this_ino == 2) //reached root
  {
    return cat_name;
  }

  mip = iget(dev, this_ino);

  //blocknumber = ((this_ino - 1) / 8) + 10;
 
  //printf("get_cwd_path: dir->name = %s\n", dir->name);
  parent_ino = search(mip, "..");
  printf("get_cwd_path: parent_ino = %d\n", parent_ino);
  iput(mip);

  parent_mip = iget(dev, parent_ino);
  get_block(dev, parent_mip->INODE.i_block[0], buffer);
  dir = (DIR *)buffer;
  printf("get_cwd_path: dir_name = %s\n", dir->name);
  
  while(block_position < BLKSIZE)
  {
    if(dir->inode == this_ino)
    {
      printf("get_cwd_path: dir_name = %s\n", dir->name);
      strcpy(cat_name, "/");
      strcat(cat_name, dir->name);
      printf("get_cwd_path: cat_name = %s\n", cat_name);
      break;
    }

    //move rec_len over
    block_position += dir->rec_len;
    string_position = (char *)dir;
    string_position += dir->rec_len;
    dir = (DIR *) string_position;
  }

  iput(parent_mip);
  return strcat(get_cwd_path(parent_ino), cat_name);
}

//level 1

int init() //initialize level 1 data structures
{
  int i = 0, j = 0;

  PROC *p;

  for (i = 0; i < NMINODES; i++)
    minode[i].refCount = 0;

  for (i = 0; i < NMOUNT; i++)
    mount_table[i].busy = 0;

  for (i = 0; i < NPROC; i++)
  {
    proc[i].status = FREE;

    for (j = 0; j < NFD; j++)
      proc[i].fd[j] = 0;

    proc[i].next = &proc[i+1];
  }

  mount_root();

  printf("\ncreating P0\n");
  p = running = &proc[0];
  p->status = BUSY;
  p->uid = 0; 
  p->pid = p->ppid = p->gid = 0;
  p->parent = p->sibling = p;
  p->child = 0;
  p->cwd = root;
  p->cwd->refCount++;

  printf("creating P1\n");
  p = &proc[1];
  p->next = &proc[0];
  p->status = BUSY;
  p->uid = 2; 
  p->pid = 1;
  p->ppid = p->gid = 0;
  p->cwd = root;
  p->cwd->refCount++;

  num_tokens = 1;
  
  return 1;
}

int is_dir(MINODE *mip)
{
  if((mip->INODE.i_mode & 0x4000) == 0x4000)
    return 1;
  else
    return 0;
}

int is_reg_file(MINODE *mip)
{
  if((mip->INODE.i_mode & 0x8000) == 0x8000)
    return 1;
  else
    return 0;
}

int is_symlink_file(MINODE *mip)
{
  if((mip->INODE.i_mode & 0xA000) == 0xA000)
    return 1;
  else
    return 0;
}

void enter_name(int dev, int ino, int mode, int uid, int gid, int size)
{
  int i = 0;
  MINODE *mip = iget(dev, ino);

  mip->INODE.i_mode = mode;
  mip->INODE.i_uid = uid;
  mip->INODE.i_gid = gid;
  mip->INODE.i_size = size;
  mip->INODE.i_links_count = 0;

  mip->INODE.i_atime = mip->INODE.i_ctime = mip->INODE.i_mtime = time(0);

  mip->INODE.i_blocks = 0;
  mip->dirty = 1;

  for(i = 0; i < 15; ++i)
  {
    mip->INODE.i_block[i] = 0;
  }

  iput(mip);
}

int make_dir(char *pathname, char *parameter)
{
  char *parent, *child, *temp_path, cwd[128];
  int inumber, parent_inumber;
  MINODE *parent_minode, *new_minode;

  if(pathname[0] == '/') //absolute path
  {
    temp_path = strdup(pathname); //prepare for destruction 
    printf("temp_path = %s\n", temp_path);
    parent = dirname(temp_path);

    temp_path = strdup(pathname);
    printf("temp_path = %s\n", temp_path);
    child = basename(temp_path);

    if(strcmp(parent, ".") == 0)
      parent = "/";
  }
 
  else
  { 
    strcpy(cwd, "");
    strcat(cwd, get_cwd_path(running->cwd->ino));
    if(strcmp(get_cwd_path(running->cwd->ino), "") != 0)
      strcat(cwd, "/");
    temp_path = strcat(cwd, pathname);
    printf("temp_path = %s\n", temp_path);
    parent = dirname(temp_path);

    
    if(strcmp(get_cwd_path(running->cwd->ino), "") != 0)
      strcat(cwd, "/");
    //temp_path = strcat(cwd, pathname);
    printf("temp_path = %s\n", temp_path);
    child = basename(temp_path);

    if(strcmp(parent, ".") == 0)
      parent = "/";

    tokenize(parent);
    if(num_tokens == 1)
    {
      parent = "/";
      strcpy(child, pathname);
    }
  }

  

  printf("num_tokens = %d\n", num_tokens);
  printf("parent = %s\n", parent);
  printf("child = %s\n", child);
	
  //get the inumber of the alleged parent directory
  parent_inumber = getino(&dev, parent);
  if(parent_inumber < 1)
  {
    printf("mkdir: parent directory does not exist.\n");
    return 0;
  }

  //load the parent into memory
  parent_minode = iget(dev, parent_inumber);
  if(is_dir(parent_minode) == 0)
  {
    printf("mkdir: %s is not a directory.\n", parent);
    return 0;
  }

  if(search(parent_minode, child) != 0)
  {
    printf("mkdir: %s already exists in that directory\n", child);
    return 0;
  }

  inumber = ialloc(dev); //allocate the space for a new inode
  create_dir_entry(parent_minode, inumber, child, EXT2_FT_DIR); //give it a dir entry

  //populate mip->INODE with things
  enter_name(dev, inumber, DIR_MODE, running->uid, running->gid, 1024);

  //load the freshly created inode into memory
  new_minode = iget(dev, inumber); 
  new_minode->INODE.i_links_count++;

  //create . and .. entries for the new dir
  create_dir_entry(new_minode, inumber, ".", EXT2_FT_DIR);
  create_dir_entry(new_minode, parent_minode->ino, "..", EXT2_FT_DIR);

  //update the proud parent
  parent_minode->INODE.i_atime = parent_minode->INODE.i_mtime = time(0);
  parent_minode->INODE.i_links_count++;

  //release minode pointers, write back to block
  iput(parent_minode); 
  iput(new_minode);
    
   return 0;
}



int create_dir_entry(MINODE *parent, int inumber, char *name, int type)
{
  char buffer[BLKSIZE], *string_position;
  int ideal_length = 0, i = 2, block, remaining_space = 0, new_dir_length;
  int block_position = 0, new_block;
  DIR *dir;

  printf("create_dir_entry: parent->name = %s dev = %d ino = %d\n", parent->name, parent->dev, parent->ino);
  
  new_dir_length = 4 * ((8 + strlen(name) + 3) / 4);

  for(i = 0; i < 12; i++) //assume 12 direct blocks
  {
    if(parent->INODE.i_block[i] == 0)
    {
      printf("create_dir_entry: returning 0\n");
      return 0;
    }

    //get parent's ith data block into a buffer  
    get_block(parent->dev, parent->INODE.i_block[i], buffer);
    dir = (DIR *)buffer;
    
    while(block_position < BLKSIZE)
    {
      printf("hello\n");
      ideal_length = 4 * ((8 + dir->name_len + 3) / 4); //multiples of 4
      printf("ideal_length = %d, remaining space = %d\n", ideal_length, remaining_space);
      remaining_space = dir->rec_len + ideal_length;

      printf("ideal_length = %d, remaining space = %d\n", ideal_length, remaining_space);

      //print dir entries to see what they are
      printf("block_position = %d\n", block_position);
      printf("dir->name = %s\tdp->rec_len = %d\n", dir->name, dir->rec_len);

      if(dir->rec_len > ideal_length)
      {
        printf("in if statement\n");
        //resize the current last entry
        dir->rec_len = ideal_length;
        
        //move to the end of the current last entry
        //string_position = (char *)dir;
        string_position += dir->rec_len;
        dir = (DIR *)string_position;
        block_position += dir->rec_len;
 
        //create the new directory entry
        dir->inode = inumber;
        dir->rec_len = BLKSIZE - block_position;
        dir->name_len = strlen(name);
        dir->file_type = type;
        strcpy(dir->name, name);
        
        //write the block back to the device
        put_block(parent->dev, parent->INODE.i_block[i], buffer);
        return 1;
      }
      
      //move rec_len bytes over
      
      //string_position = (char *)dir;
      block_position += dir->rec_len;
      string_position = (char *)dir;
      string_position += dir->rec_len;
      dir = (DIR *)string_position;
    
    }
  }
  //if we've gotten here, there's no space in existing data blocks
  new_block = balloc(dev); //allocate a new block
  parent->INODE.i_size += BLKSIZE;

  //enter the new dir as the first entry in the new block
  get_block(parent->dev, new_block, buffer);
  dir = (DIR *)buffer;

  dir->inode = inumber;
  dir->rec_len = BLKSIZE - block_position;
  dir->name_len = strlen(name);
  dir->file_type = EXT2_FT_DIR;
  strcpy(dir->name, name);

  //write the block back to the device
  put_block(parent->dev, parent->INODE.i_block[i], buffer); 
  printf("here\n");
  return 1;
}

int myls(char *path, char *parameter)
{
  int inumber, dev = running->cwd->dev, i = 0, bnumber, block_position;
  MINODE *mip = running->cwd, *current_mip;
  char buffer[BLKSIZE], *string_position, *mtime, *temp_path, *parent, *child, cwd[128];
  DIR *dir;

  if(strcmp(path, "") == 0 || path == 0)
  {
    path = running->cwd->name;
  }
 
  else if(path[0] == '/') //absolute path
  {
    temp_path = strdup(pathname); //prepare for destruction 
    printf("temp_path = %s\n", temp_path);
    parent = dirname(temp_path);

    temp_path = strdup(pathname);
    printf("temp_path = %s\n", temp_path);
    child = basename(temp_path);

    if(strcmp(parent, ".") == 0)
      parent = "/";
  }
 
  else
  { 
    strcpy(cwd, "");
    strcat(cwd, get_cwd_path(running->cwd->ino));
    if(strcmp(get_cwd_path(running->cwd->ino), "") != 0)
      strcat(cwd, "/");
    temp_path = strcat(cwd, pathname);
    printf("temp_path = %s\n", temp_path);
    parent = dirname(temp_path);

    
    if(strcmp(get_cwd_path(running->cwd->ino), "") != 0)
      strcat(cwd, "/");
    temp_path = strcat(cwd, pathname);
    printf("temp_path = %s\n", temp_path);
    child = basename(temp_path);

    if(strcmp(parent, ".") == 0)
      parent = "/";

    tokenize(parent);
    if(num_tokens == 1)
    {
      parent = "/";
      strcpy(child, pathname);
    }
  }

  inumber = getino(&dev, path); //get this specific path

  if(inumber == 0 || inumber == -1)
  {
    printf("ls: could not find directory\n");
    return 0;
  }

  mip = iget(dev, inumber);
  if(is_dir(mip) == 0)
  {
    printf("ls: %s is not a directory\n", path);
    return 0;
  }

    //first 12 data blocks
    for(i = 0; i < 12; i++)
    {
      if(mip->INODE.i_block[i] == 0) //if empty block, we're done
        return 0;

      bnumber = get_block(dev, mip->INODE.i_block[i], buffer); 
      dir = (DIR *)buffer;
      block_position = 0;

      while(block_position < BLKSIZE) //print all dir entries in the block
      {
        //get current minode 
        //printf("dev: %d dir->inode: %d\n", dev, dir->inode);
        current_mip = iget(dev, dir->inode);

        //print file type
        if(is_dir(mip))
          printf("\nd ");
        else if(is_reg_file(mip))
          printf("\nr ");
        else 
          printf("\ns ");

        //print permissions
        printf((mip->INODE.i_mode & 0x0100) ? "r" : "-");
    	printf((mip->INODE.i_mode & 0x0080) ? "w" : "-");
    	printf((mip->INODE.i_mode & 0x0040) ? "x" : "-");
    	printf((mip->INODE.i_mode & 0x0020) ? "r" : "-");
    	printf((mip->INODE.i_mode & 0x0010) ? "w" : "-");
    	printf((mip->INODE.i_mode & 0x0008) ? "x" : "-");
    	printf((mip->INODE.i_mode & 0x0004) ? "r" : "-");
    	printf((mip->INODE.i_mode & 0x0002) ? "w" : "-");
    	printf((mip->INODE.i_mode & 0x0001) ? "x" : "-");

        //print everything else
        //mtime = ctime(&mip->INODE.i_mtime);
        //mtime[24] = '\0';

        printf("   %d  %d  %d  %d\t%d\t%s\n", mip->INODE.i_links_count, mip->INODE.i_uid, mip->INODE.i_gid, 
           mip->INODE.i_mtime, mip->INODE.i_size, dir->name);

        printf("dir->rec_len = %d, position = %d\n", dir->rec_len, block_position);
        iput(current_mip); //write the minode back to the disk
    
        //move rec_len bytes
        block_position += dir->rec_len;
        string_position = (char *)dir;
        string_position += dir->rec_len;
        dir = (DIR *)string_position;

        //strcpy(mtime, "");
      }
      iput(mip);
    }
    return 1;
  
}

int mount_root()  // mount root file system, establish / and CWDs
{
  char sbuffer[BLKSIZE], gbuffer[BLKSIZE];
  GD * gp;
  
  printf("Beggining mount process for %s\n", diskName);

  //open device for RW
  dev = open(diskName, O_RDWR);
  if(dev < 0)
  {
    printf("Could not open %s\n", diskName);
    return 0;
  }

  //read SUPER block to verify it's an EXT2 FS
  get_block(dev, 1, sbuffer);  
  sp = (SUPER *)sbuffer;

  get_block(dev, 2, gbuffer);
  gp = (GD *)gbuffer;

  if (sp->s_magic != 0xEF53)
  {
    printf("%s is NOT an EXT2 FS, you foolish fool!\n", diskName);
    return 0;
  }

  printf("magic: %d bmap: %d imap: %d iblock: %d\n",
    sp->s_magic, gp->bg_block_bitmap, gp->bg_inode_bitmap, gp->bg_inode_table);
  

  
  //initialize entry in mount_table
  mount_table[0].busy = 0;
  strcpy(mount_table[0].name, diskName);
  strcpy(mount_table[0].mount_name, "/");
  mount_table[0].ninodes = sp->s_inodes_count;
  mount_table[0].nblocks = sp->s_blocks_count;
  mount_table[0].iblock = gp->bg_inode_table;
  
  root = iget(dev, 2); //get root inode
  strcpy(root->name, "/");

  //root = mount_table[0].mounted_inode;
  printf("root %s has been mounted.\n", root->name);
  printf("nblocks: %d    free blocks: %d \nnum inodes: %d    free inodes: %d\n",
    sp->s_blocks_count, gp->bg_free_blocks_count, sp->s_inodes_count, gp->bg_free_inodes_count);

  //initialize globals
  imap = gp->bg_inode_bitmap;
  bmap = gp->bg_block_bitmap;
  ninodes = sp->s_blocks_count;

  //Let cwd of both P0 and P1 point at the root minode (refCount=3)
  proc[0].cwd = root; 
  proc[1].cwd = root;

  return 1;
}

char* getParentPath()
{
  char *str = "";
  int i;
  for(i = 0; i < num_tokens-1; i++)
  {
    strcat(str, tokenized_pathname[i]);
    strcat(str, "/");
  }
  strcat(str, tokenized_pathname[i+1]);
  return str;
}

rm_child(MINODE *pip, char *name)
{
  char buffer[BLKSIZE];
  int i;

  for(i = 0; i < 12; i++)
  {
    if(pip->INODE.i_block[i] == 0)
      return 0;
    
    
  }
}

int myrmdir(char *path, char *parameter)
{
  MINODE *pip, *mip;
  char *parentPath;
  int parentIno, ino, i;

  parentPath = getParentPath();

  printf("attempting to remove %s...\n", pathname);

  //get the inumber of the pathname
  ino = getino(&dev, pathname);
  //get the minode[] pointer
  mip = iget(dev, ino);
  
  if(running->uid != 0) //we only check access priveleges if it's not the super user
  {
    if(mip->INODE.i_uid != running->uid) //if the uid of the current process doesn't match the parent id
    printf("Access denied\n"); //they dont have access priveleges
    return 0;
  }
  
  if(is_dir(mip) == 0) //check if it's not a directory
    {
      printf("rmdir: that's not a directory\n");
      return 0;
    }
  /*if(dir_is_busy(mip) == 1) //check if the directory is busy
    {
      printf("The directory is currently in use. Please end all tasks from this directory and try again...\n");
      return 0;
    }
  if(dir_is_empty(mip) == 0) // check if the directory is empty
  {
    printf("The directory is not empty...");
    return 0;
  } */
  
  //Deallocate all the directory's blocks and inode
  for (i=0; i<12; i++) // loop through the blocks first
  {
    if (mip->INODE.i_block[i]==0)
      continue;
    bdealloc(mip->dev, mip->INODE.i_block[i]); //deallocate the blocks
  }
  idealloc(mip->dev, mip->ino); //Now deallocate the inode
  iput(mip); //clear mip-> refCount
  
  parentIno = getino(&dev, parentPath); //get the parent inode

  pip = iget(mip->dev, parentIno); //get the parent MINODE pointer
  
  //remove child entry from parent directory
  rm_child(pip, tokenized_pathname[num_tokens]);

  pip->INODE.i_links_count--; //decrement pips link count

  //update pips atime and mtime
  pip->INODE.i_atime = time(NULL);
  pip->INODE.i_mtime = time(NULL);
  
  pip->dirty = 1; //mark pip as dirty
  
  iput(pip); //clear refCount
  
  return 1;

}

//TODO: FINISH THIS FUNCTION
/*
int rm_child(MINODE *parent, char *name)
{
  int searchResult;
  searchResult = search(parent, name);
  
}
*/

int mycd(char *path, char *parameter)
{
  int ino;
  MINODE *startPoint;

  if(strcmp(path, "") == 0 || path == NULL) //if there is no path passed in
  {
    running->cwd = root; //set the cwd to the root
  }
  else //if path was passed
  {
    ino = getino(&dev, path); //get the ino 

    if(ino == 0) //if the ino was never found
      return 0; //return unsuccessfull

    startPoint = iget(dev, ino); //get the MINODE pointer
    strcpy(startPoint->name, path); //add the name to the MINODE pointer
    running->cwd = startPoint; //set the cwd to the MINODE pointer
    iput(startPoint);
  }

  return 1;
}

int mytouch(char *path, char *parameter) //modify INODE's atime and mtime
{
  MINODE *mip;
  int ino;

  ino = getino(&dev, path); //get the inumber of the pathname
  mip = iget(dev, ino);//get the minode[] pointer

  //update the inodes atime and mtime
  mip->INODE.i_atime = time(NULL);
  mip->INODE.i_mtime = time(NULL);
  
  mip->dirty = 1; //mark that the inode ahs been updated

  iput(mip);

  return;
}



int mypwd(char *path, char *parameter)
{
  printf("mypwd: running->cwd->ino = %d\n", running->cwd->ino);
  
  if(running->cwd->ino == 2)
    printf("/\n");
  else
    printf("%s\n", get_cwd_path(running->cwd->ino));
}

//TODO: WRITE THIS FUNCTION
int truncate(MINODE *mip)
{
  
}

int myunlink(char *path, char *parameter)
{
  int ino, parentIno;
  MINODE *mip, *pip;
  char *parentPath, *basePath;

  strcpy(parentPath,getParentPath(path));
  strcpy(basePath, basename(path));

  //Get the pathnames INODE
  ino = getino(&dev, path);
  mip = iget(dev, ino);

  //Check that it is a file
  if(is_reg_file(mip) == 0 || is_symlink_file(mip) == 0) //not a file type
  {
    printf("%s is not a file\n", basename(path));
    return 0;
  }

  mip->INODE.i_links_count--; //decrement the inodes link count

  if(mip->INODE.i_links_count == 0)
  {
    //deallocate the pathnames data blocks
    truncate(mip);
    
    //deallocate the pathnames INODE
    idealloc(dev, mip->ino); 
  }

  parentIno = getino(&dev, parentPath);
  pip = iget(dev, parentIno);
  rm_child(pip, basePath);

  return 1;
}



int execute_stat(char *pathname, stat *st)
{
  int ino;
  MINODE *mip;

//get INODE of pathname into a minode;
  ino = getino(&dev, pathname);
  mip = iget(dev, ino);
//copy (dev, ino) of minode to (st_dev, st_ino) of the STAT structure in user space;
  st->st_dev = dev;
  st->st_ino = ino;
//copy other fields of INODE to STAT structure in user space;
  st->st_mode = mip->INODE.i_mode;
  st->st_nlink = mip->INODE.i_links_count;
  st->st_uid = mip->INODE.i_uid;
  st->st_gid = mip->INODE.i_gid;
  st->st_size = mip->INODE.i_size;
  st->st_atime = mip->INODE.i_atime;
  st->st_mtime = mip->INODE.i_mtime;
  st->st_ctime = mip->INODE.i_ctime;
  st->st_blksize = BLKSIZE;
  st->st_blocks = mip->INODE.i_blocks;

  printf("dev=%d\tino=%d\tmod=%s\n", st->st_dev, st->st_ino, st->st_mode);
  printf("uid=%d\tgid=%d\tnlink=%d\n", st_uid, st_gid, st_nlink);
  printf("size=%d\ttime=%s\n", st_size, st_atime);

  if(iput(mip) == 0) //iput failed
    return 0;

  return 1;
}

int mystat(char *path, char *parameter)
{
  struct stat mystat, *st;
  st = &mystat;

  execute_stat(path, st);

  return 1;
}

//same algorithm as mkdir! slightly different.
int mycreat(char *pathname, char *parameter)
{
  char *parent, *child, *temp_path, cwd[128];
  int inumber, parent_inumber;
  MINODE *parent_minode, *new_minode;

  if(pathname[0] == '/') //absolute path
  {
    temp_path = strdup(pathname); //prepare for destruction 
    printf("temp_path = %s\n", temp_path);
    parent = dirname(temp_path);

    temp_path = strdup(pathname);
    printf("temp_path = %s\n", temp_path);
    child = basename(temp_path);

    if(strcmp(parent, ".") == 0)
      parent = "/";
  }
 
  else
  { 
    strcpy(cwd, "");
    strcat(cwd, get_cwd_path(running->cwd->ino));
    if(strcmp(get_cwd_path(running->cwd->ino), "") != 0)
      strcat(cwd, "/");
    temp_path = strcat(cwd, pathname);
    printf("temp_path = %s\n", temp_path);
    parent = dirname(temp_path);

    
    if(strcmp(get_cwd_path(running->cwd->ino), "") != 0)
      strcat(cwd, "/");
    temp_path = strcat(cwd, pathname);
    printf("temp_path = %s\n", temp_path);
    child = basename(temp_path);

    if(strcmp(parent, ".") == 0)
      parent = "/";

    tokenize(parent);
    if(num_tokens == 1)
    {
      parent = "/";
      strcpy(child, pathname);
    }
  }
	
  //get the inumber of the alleged parent directory
  parent_inumber = getino(&dev, parent);
  if(parent_inumber < 1)
  {
    printf("creat: parent directory does not exist.\n");
    return 0;
  }

  //load the parent into memory
  parent_minode = iget(dev, parent_inumber);
  if(is_dir(parent_minode) == 0)
  {
    printf("creat: %s is not a directory.\n", parent);
    return 0;
  }

  if(search(parent_minode, child) != 0)
  {
    printf("creat: %s already exists in that directory\n", child);
    return 0;
  }

  inumber = ialloc(dev); //allocate the space for a new inode
  create_dir_entry(parent_minode, inumber, child, EXT2_FT_REG_FILE); //give it a dir entry

  //populate mip->INODE with things
  enter_name(dev, inumber, FILE_MODE, running->uid, running->gid, 0);

  //load the freshly created inode into memory
  new_minode = iget(dev, inumber); 
  new_minode->INODE.i_links_count++;

  //update the proud parent
  parent_minode->INODE.i_atime = parent_minode->INODE.i_mtime = time(0);
  parent_minode->INODE.i_links_count++;

  //release minode pointers, write back to block
  iput(parent_minode); 
  iput(new_minode);
    
   return 0;
}

//hardlink: create a new file, same inumber as the old file
int mylink(char *oldfile, char *newfile) 
{
  int inumber, parent_inumber;
  MINODE *mip, *parent_mip;
  char *newfile_path, *newfile_name, *temp, *copy, *buffer;

  //get an mip that represents oldfile
  if(oldfile[0] != '/') //if oldfile is relative
  {
    temp = strcat(running->cwd->name, "/");
    oldfile = strcat(temp, oldfile);
  }
  if(oldfile[0] != '/') //if newfile is relative
  {
    temp = strcat(running->cwd->name, "/");
    newfile = strcat(temp, newfile);
  }
  inumber = getino(&dev, oldfile);
  mip = iget(dev, inumber);

  //check oldfile is reg or link file (symlink?)
  if(is_reg_file(mip) == 0)
  {
    printf("link: file is not a regular or link file\n");
    iput(mip);
    return 0;
  }

  //check newfile path exists, but dirname doesn't yet
  copy = newfile;
  newfile_path = dirname(copy);
  newfile_name = basename(copy);

  printf("link: new path = %s, name = %s\n", newfile_path, newfile_name);

  //if newfile path DNE or dirname already exists
  if(search(root, newfile_path) == 0)
  {
    printf("link: %s does not exist\n", newfile_path);
    iput(mip);
    return 0;
  }
  if(search(root, newfile) == 1)
  {
    printf("link: %s already exists\n", newfile);
    iput(mip);
    return 0;
  }

  //add an entry to the data block of newfile_path, same inumber
  parent_inumber = getino(&dev, newfile_path);
  parent_mip = iget(dev, parent_inumber);
  create_dir_entry(parent_mip, mip->ino, newfile, EXT2_FT_REG_FILE);
  
  mip->INODE.i_links_count++; //increment links count
 
  //write inode back to disk
  iput(mip);
  iput(parent_mip);

  return 1;
  
}

//create a new file & inode, point to same inumber as the old file
//link across file systems or to directories
int mysymlink(char *path, char *parameter){}
int mymenu(char *path, char *parameter){}

int myquit(char *path, char *parameter)
{
  int i = 0;
  
  iput(root);

  printf("Goodbye!\n");
  exit(1);
}

int mystat(char *path, char *parameter){}
int mychmod(char *path, char *parameter){}
int mychown(char *path, char *parameter){}
int mychgrp(char *path, char *parameter){}

char *commands[] = {"mkdir", "rmdir", "cd", "ls", "pwd", "creat", "link", "unlink", "symlink", "menu", "quit", "stat", "chmod", "touch", "chown", "chgrp", "0"};
int (*function[]) (char*, char*) = {make_dir, myrmdir, mycd, myls, mypwd, mycreat, mylink, myunlink, mysymlink, mymenu, myquit, mystat, mychmod, mytouch, mychown, mychgrp};

int main(int argc, char *argv[])
{
  int i, cmd;
  char line[128], cname[64];

  printf("Welcome!\n");

  if(argc == 1)
  {
    printf("please enter a device to be mounted as the root directory: ");
    fgets(diskName, 256, stdin);
  }
  else
  {
    //printf("argv[1] = %s\n", argv[1]);
    strcpy(diskName, argv[1]);
  }

  init(); //init() 

  while(1)
  {
    //reset pathname and parameter each time
    memset(line, 0, 128);
    memset(cname, 0, 64);
    memset(pathname, 0, 64);
    memset(parameter,0, 64); 

    printf("P%d running, ", running->pid);
    printf("command: ");
    fgets(line, 128, stdin);
    
    line[strlen(line) - 1] = 0; //get rid of /r at the end
    sscanf(line, "%s %s %64c", cname, pathname, parameter);
    //printf("%s %s %64c", cname, pathname, parameter);
    
    for (i = 0; i < 18; ++i)  
    {
      if (!strcmp(cname, commands[i]))
      {
          (*function[i])(pathname, parameter);
          break;
      }
    }
  }
  return 0;
}
