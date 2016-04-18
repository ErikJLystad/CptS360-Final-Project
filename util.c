#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "ext2_fs.h"

#define BLKSIZE 1024
typedef unsigned int   u32;

char* pathname[BLKSIZE];
int n;  //number of tokens in pathname
char buf[BLKSIZE];
char ip_buf[BLKSIZE];

typedef struct ext2_group_desc  GD;
typedef struct ext2_super_block SUPER;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;

//blocks
GD *bg;
SUPER *sp;
INODE *ip;
DIR *dp;

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

