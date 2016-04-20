#include <stdio.h>
#include "type.h"
#include "ext2_fs.h"

//GLOBALS
int fd;
int imap, bmap;  // IMAP and BMAP block number
int ninodes, nblocks, nfreeInodes, nfreeBlocks;

//get_block() and put_block() are in util.c

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
