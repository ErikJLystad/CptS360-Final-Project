#include "type.h"

MINODE minode[NMINODES];
MINODE *root;
PROC   proc[NPROC], *running;
MOUNT  mounttab[5];

int fd, dev;
int nblocks, ninodes, bmap, imap, iblock;
char pathname[256], parameter[256];

//level 1

int is_dir(MINODE *mip)
{
  if((mip->INODE.i_mode & 0x4000) == 0x4000)
    return 1;
  else
    return 0;
}

int make_dir() //returns 1 on success, 0 on failure
{
  MINODE *mip, *parent_mip;
  int parent_ino;
  char *parent, *child, *temp_path;

  if(pathname[0] == '/') //if pathname is absolute
  {
    mip = root; //start at root minode
    dev = root->dev;
  }
  else //if pathname is relative
  {
    mip = running->cwd; //start at running proc's cwd
    dev = running->cwd->dev;
  }

  temp_path = strdup(pathname); //prepare for destruction 
  parent = dirname(temp_path);
  temp_path = strdup(pathname);
  child = basename(temp_path);

  //get IN_MEMORY minode of parent
  parent_ino = getino(&dev, parent);
  parent_mip = iget(dev, parent_ino);

  //verify parent inode is a dir 
  if(is_dir(parent_mip) == 0)
  {
    printf("mkdir: parent is not a directory\n");
    return 0;
  }
     
  //verify child doesn't already exist in parent
  if(search(parent_mip, child) == 0)
  {
    printf("mkdir: directory already exists\n");
    return 0;
  }
  
  mymkdir(parent_mip, child);

  parent_mip->INODE.i_links_count++;
  parent_mip->INODE.i_atime = time(0);
  parent_mip->dirty = 1;
  
  iput(parent_mip);
}

int mymkdir(MINODE *parent_mip, char *name)
{
  MINODE *mip; //new dir minode
  INODE *ip;
  char buffer[BLKSIZE];
  int i;

  //allocate inode and disk blocks for new directory
  int inumber = ialloc(dev);
  int bnumber = balloc(dev);

  mip = iget(dev, inumber); //load inode into a minode[]
  ip = &mip->INODE; 

  //write new dir attributes to mip->INODE
  ip->i_mode = 0x41ED;
  ip->i_uid = running->uid;
  ip->i_gid = running->gid;
  ip->i_size = BLKSIZE;                       //size in bytes
  ip->i_links_count = 2;                      //. and ..
  ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L); //current time
  ip->i_blocks = 2;
  ip->i_block[0] = bnumber;
 
  for(i = 1; i < 15; i++)
  {
    ip->i_block[i] = 0;
  }

  mip->dirty = 1; //mark minode dirty
  iput(mip);      //write INODE to disk
   
  //create an entry for the new directory
  create_dir_entry(parent_mip, inumber, name);
  mip->INODE.i_links_count++;

  //create . and .. entries for the new directory
  create_dir_entry(mip, inumber, ".");
  create_dir_entry(mip, parent_mip->ino, "..");
  
  iput(mip);
  return;
}

int create_dir_entry(MINODE *parent, int inumber, char *name)
{
  char buffer[BLKSIZE], *string_position;
  int ideal_length, i = 0, block, remaining_space, new_dir_length;
  int block_position = 0;
  DIR *dir;
  
  new_dir_length = 4 * ((8 + strlen(name) + 3) / 4);

  for(i = 0; i < 12; i++) //assume 12 direct blocks
  {
    if(parent->INODE.i_block[i] == 0)
      break;

    //get parent's ith data block into a buffer  
    get_block(parent->dev, parent->INODE.i_block[i], buffer);
    dir = (DIR *)buffer;
    string_position = buffer;

    //step to last entry in data block
    block = parent->INODE.i_block[i];
    printf("stepping to last entry in data block %d\n", block);
    while(block_position < BLKSIZE);
    {
      ideal_length = 4 * ((8 + dir->name_len + 3) / 4); //multiples of 4
      remaining_space = dir->rec_len + ideal_length;

      //print dir entries to see what they are
      printf("%s\n", dir->name);

      if(remaining_space >= new_dir_length)
      {
        //resize the current last entry
        dir->rec_len = ideal_length;
        
        //move to the end of the current last entry
        string_position = (char *)dir;
        string_position += dir->rec_len;
        block_position += dir->rec_len;
        dir = (DIR *)string_position;
 
        //create the new directory entry
        dir->inode = inumber;
        dir->rec_len = BLKSIZE - block_position;
        dir->name_len = strlen(name);
        dir->file_type = EXT2_FT_DIR;
        strcpy(dir->name, name);
        
        //write the block back to the device
        put_block(parent->dev, parent->INODE.i_block[i], buffer);
        return;
      }
      
      //move rec_len bytes over
      block_position += dir->rec_len;
      string_position = (char *)dir;
      string_position += dir->rec_len;
      dir = (DIR *)string_position;
    } 
  }
  //if we've gotten here, there's no space in existing data blocks
  int new_block = balloc(dev); //allocate a new block
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
  return;
}

int ls(char *path)
{
  int inumber, dev = running->cwd->dev, i = 0, bnumber, block_position;
  MINODE *mip = running->cwd;
  char buffer[BLKSIZE], *string_position;
  DIR *dir;
 
  if(path)  //there should always be a path passed in
  {
    if(path[0] == '/') //if path is absolute
      dev = root->dev;

    inumber = getino(&dev, path); //get this specific path

    if(inumber == 0)
    {
      printf("ls: could not find directory\n");
      return 0;
    }

    mip = iget(dev, inumber);
    if(is_dir(mip) == 0)
    {
      printf("ls: that's not a directory\n");
      return 0;
    }

    //first 12 data blocks
    for(i = 0; i < 12; i++)
    {
      if(mip->INODE.i_block[i] == 0) //if empty block, we're done
        return 0;

      bnumber = get_block(0, mip->INODE.i_block[i], buffer); 
      dir = (DIR *)buffer;

      block_position = 0;
      while(block_position < BLKSIZE) //print all dir entries in the block
      {
        printf("%s\n", dir->name);
    
        //move rec_len bytes
        block_position += dir->rec_len;
        string_position = (char *)dir;
        string_position += dir->rec_len;
        dir = (DIR *)string_position;
      }
    }
    
  }
}

int main()
{
}
