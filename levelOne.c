/* 
ERIK:
mount_root
rmdir
cd
unlink
creat
touch
stat

MEGAN:
mkdir
ls
pwd
link
symlink
chmod
*/

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
