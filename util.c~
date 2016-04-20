//util.c for CS360 final project
//Erik Lystad and Megan McPherson

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>  //dirname() and basename(), remember that these destroy the parameter string, so make copies using strdup()!
#include "type.h"

typedef unsigned int   u32;

char* pathname[BLKSIZE];
int n;  //number of tokens in pathname
char buf[BLKSIZE];
char ip_buf[BLKSIZE];
int fd;

MINODE minode[NMINODES];

PROC *running; //pointer to PROC structure of current running process
MINODE *root;   //pointer to root inode

int get_block(int fd, int blk, char buf[ ]) //credit to KCW
{
  lseek(fd, (long)blk*BLKSIZE, 0);
  read(fd, buf, BLKSIZE);
}

int put_block(int fd, int blk, char buf[ ]) //credit to KCW
{
  lseek(fd, (long)blk*BLKSIZE, 0);
  write(fd, buf, BLKSIZE);
}

void tokenize(char* name)
{
  int i = 0;
  char *token = strtok(name, "/");
  while(token != 0)
  {
    pathname[i] = token;
    i++;
    token = strtok(0, "/");
  }

  printf("\nPathname has been split\n");
  i = 0;
  while(pathname[i] != 0)
  {
    printf("pathname[%d] = %s\n", i, pathname[i]);
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

  //2. find and return ino
  for (i = 0; i < n; i++) //n is number of steps in pathname
  {
    inumber = search(ip, pathname[i]);  
    if (inumber == 0) //: can't find name[i], BOMB OUT!
    {
      printf("inode could not be found\n");
      exit(1);
    }
  }
   return inumber;
}

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
  mip->refCount--;
  if(mip->refcount > 0 || mip->dirty == 0)
  {
    return;
  }
  if(mip->refcount == 0 && mip->dirty == 1) //ASK KC ABOUT THIS LOGIC
  {
    //Writing Inode back to disk
    int block = (mip->ino - 1) / 8 + INODEBLOCK;
    int offset = (mip->ino -1) % 8;
    
    //read block into buf
    getblock(fd, block, buf);
    ip = (INODE *)buf + offset;
    *ip = mip->INODE;
    putblock(fd, block, buf);
  }
}

int findino(MINODE *mip; int *myino, *parentino)
{
  //ASK KC ABOUT THIS
}

MINODE *iget(int dev, int ino)
{
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
  get_block(fd, blk, buf); 
  ip = (INODE *)buf + offset; //ip already defined in types.h
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
