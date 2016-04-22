#include "type.h"

typedef unsigned int   u32;



MINODE minode[NMINODES];
MINODE *root;
PROC   proc[NPROC], *running;
MOUNT  mounttab[5];

int fd, dev;
int nblocks, ninodes, bmap, imap, iblock;
char pathname[256], parameter[256];
char *tokenized_pathname[256];
int num_tokens;

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

void tokenize(char* name)
{
  int i = 0;
  char *token = strtok(name, "/");
  while(token != 0)
  {
    tokenized_pathname[i] = token;
    i++;
    token = strtok(0, "/");
  }
  num_tokens = i;
  printf("\npathname has been split\n");
  i = 0;
  while(tokenized_pathname[i] != 0)
  {
    printf("pathname[%d] = %s\n", i, tokenized_pathname[i]);
    i++;
  }
}

int getino(int *dev, char *pathname) //int ino = getino(&dev, pathname) essentially returns (dev,ino) of a pathname
{
  int i = 0, inumber, blocknumber;
  //1. update *dev
  if (strcmp(pathname[0], "/") == 0)
    *dev = root->dev;
  else
    *dev = running->cwd->dev; //running is a pointer to PROC of current running process.

   tokenize(pathname);

  //2. find and return ino
  for (i = 0; i < num_tokens; i++) //n is number of steps in pathname
  {
    inumber = search(ip, pathname[i]);  
    if (inumber == 0) //: can't find name[i], BOMB OUT!
    {
      printf("inode could not be found\n");
      return 0;
    }
  }
   return inumber;
}

//search for a file by name starting with mip
//if found, return inumber. if not, return 0.
int search(MINODE *mip, char *name)
{
  int i;
  char *copy, sbuf[BLKSIZE];
  DIR *dp;
  INODE *ip;

  ip = &(mip->INODE);
  for(i = 0; i < 12; i++)
  {
    if(ip->i_block[0] == 0)
      return 0;

    get_block(fd, ip->i_block[i], sbuf);
    dp = (DIR*)sbuf;
    copy = sbuf;

    while(copy < sbuf + BLKSIZE)
    {
       if(strcmp(dp->name, name) == 0)
         return dp->inode;

       copy += dp->rec_len;
       dp = (DIR *)copy;
    }
  }
  return 0;
}

//This function releases a Minode[] pointed by mip.
int iput(MINODE *mip)
{
  char buffer[BLKSIZE];

  mip->refCount--;
  if(mip->refCount > 0 || mip->dirty == 0)
  {
    return;
  }
  if(mip->refCount == 0 && mip->dirty == 1) //ASK KC ABOUT THIS LOGIC
  {
    //Writing Inode back to disk
    int block = (mip->ino - 1) / 8 + INODEBLOCK;
    int offset = (mip->ino -1) % 8;
    
    //read block into buf
    get_block(fd, block, buffer);
    ip = (INODE *)buffer + offset;
    *ip = mip->INODE;
    put_block(fd, block, buffer);
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
  blk = (ino - 1)/8 + INODEBLOCK;
  offset = (ino - 1) % 8;

  //read blk into buf[]
  get_block(fd, blk, buffer); 
  ip = (INODE *)buffer + offset; //ip already defined in types.h
  mip->INODE = *ip;

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

    get_block(fd, ip->i_block[i], sbuf);
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

//level 1

int init() //initialize level 1 data structures
{
  int i = 0;

  //initialize super user
  proc[0].uid = 0;
  proc[0].cwd = &minode[0];

  //initialize regular user
  proc[1].uid = 1;
  proc[1].cwd = &minode[0];

  running = &proc[0]; //beginning process

  root = 0; //root 

  for(i = 0; i < 100; i++)
  {
    minode[i].refCount = 0;
  }
}

int is_dir(MINODE *mip)
{
  if((mip->INODE.i_mode & 0x4000) == 0x4000)
    return 1;
  else
    return 0;
}

int make_dir(char *path) //returns 1 on success, 0 on failure
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

  temp_path = strdup(path); //prepare for destruction 
  parent = dirname(temp_path);
  temp_path = strdup(path);
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

int myls(char *path)
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

int mount_root()  // mount root file system, establish / and CWDs
{
  char *buffer[BLKSIZE];
  
  printf("Beggining mount process for %s\n", diskName);

  //open device for RW
  fd = open("mydisk", O_RDWR);
  if(fd < 0)
  {
    printf("Could not open %s\n", diskname);
    exit(1);
  }

  //read SUPER block to verify it's an EXT2 FS
  get_block(fd, 1, buffer);  
  sp = (SUPER *)buffer;

  if (sp->s_magic != 0xEF53)
  {
    printf("%s is NOT an EXT2 FS, you foolish fool!\n", diskName);
    exit(1);
  }
  root = iget(dev, 2);    /* get root inode */
  
  //Let cwd of both P0 and P1 point at the root minode (refCount=3)
  root = iget(fd, 2);
  P0.cwd = iget(fd, 2); 
  P1.cwd = iget(fd, 2);
}

int myrmdir()
{
  MINODE *pip, *mip;
  char *parentPath;
  int parentIno, ino;

  parentPath = getParentPath();

  printf("attempting to remove %s...\n", pathname);

  //get the inumber of the pathname
  ino = getino(&dev, pathname);
  //get the minode[] pointer
  mip = iget(dev, ino);
  
  if(running.uid != 0) //we only check access priveleges if it's not the super user
  {
    if(mip->INODE.i_uid != running.uid) //if the uid of the current process doesn't match the parent id
    printf("Access denied\n"); //they dont have access priveleges
    return 0;
  }
  
  if(is_dir(mip) == 0) //check if it's not a directory
    {
      printf("%s is not a directory\n", );
      return 0;
    }
  if(dir_is_busy(mip) == 1) //check if the directory is busy
    {
      printf("The directory is currently in use. Please end all tasks from this directory and try again...\n");
      return 0;
    }
  if(dir_is_empty(mip) == 0) // check if the directory is empty
  {
    printf("The directory is not empty...");
    return 0;
  }
  
  //Deallocate all the directory's blocks and inode
  for (i=0; i<12; i++) // loop through the blocks first
  {
    if (mip->INODE.i_block[i]==0)
      continue;
    bdealloc(mip->dev, mip->INODE.i_block[i]); //deallocate the blocks
  }
  idealloc(mip->dev, mip->ino); //Now deallocate the inode
  iput(mip); (which clears mip->refCount = 0); //clear mip-> refCount
  
  parentIno = getino(&dev, parentPath); //get the parent inode

  pip = iget(mip->dev, parentIno); //get the parent MINODE pointer
  
  //remove child entry from parent directory
  rm_child(pip, tokenized_pathname[num_tokens]);

  //decrement pips link count
  pip->INODE.i_links_count--;
  //update pips atime and mtime
  touch(pathname);
  //mark pip as dirty
  pip->dirty = 1;
  //clear refCount
  iput(pip) 
  
  return 1;

}

char * getParentPath()
{
  char *str = "";
  int i;
  for(i = 0; i < num_tokens-1; i++;)
  {
    strcat(str, tokenized_pathname[i]);
    strcat(str, "/");
  }
  strcat(str, tokenized_pathname[i+1]);
  return str;
}

int rm_child(MINODE *parent, char *name)
{
  int searchResult;
  searchResult = search(parent, name);
  
}

int mytouch()
{
  MINODE *mip;
  int ino;

  //get the inumber of the pathname
  ino = getino(&dev, pathname);
  //get the minode[] pointer
  mip = iget(dev, ino);

}

int myrmdir(char* path){}
int mycd(char * path){}
int mypwd(char *path){}
int mycreat(char *path){}
int mylink(char *path){}
int myunlink(char *path){}
int mysymlink(char *path){}
int mymenu(){}
int myexit(){}
int mystat(char *path){}
int mychmod(char *path){}
int mychown(char *path){}
int mytouch(char *path){}
int mychgrp(char *path){}

char *commands[] = {"mkdir", "rmdir", "cd", "ls", "pwd", "creat", "link", "unlink", "symlink", "menu", "exit", "stat", "chmod", "touch", "chown", "chgrp", "0"};
int (*function[]) (char*) = {make_dir, myrmdir, mycd, myls, mypwd, mycreat, mylink, myunlink, mysymlink, mymenu, myexit, mystat, mychmod, mytouch, mychown, mychgrp};

int main()
{
  int i, cmd;
  char line[128], cname[64];

  init();
  //mount_root();

  while(1)
  {
    printf("P%d running: ", running->pid);
    printf("command: ");
    fgets(line, 128, stdin);
    
    line[strlen(line) - 1] = 0; //get rid of /r at the end
    sscanf(line, "%s %s %64c", cname, pathname, parameter);
    printf("%s %s %64c", cname, pathname, parameter);
    
    for (i = 0; i < 18; ++i)  
    {
      if (!strcmp(cname, commands[i]))
      {
          (*function[i])(parameter);
      }
    }
  }
}
