//showblock displays the disk blocks of a file in an EXT2 file system
//showblock device pathname
//showblock diskimage /a/b/c
//first, be sure to make a diskimage using {kcwmkfs mydisk 1440 184}


//Programmer: Megan McPherson

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
//#include <time.h>
#include "ext2_fs.h"

#define BLKSIZE 1024
typedef unsigned int   u32;

char *disk = "mydisk";
char buf[BLKSIZE];
char ip_buf[BLKSIZE];
int fd;
int inodes_begin;
char* pathname[BLKSIZE];
int n;  //number of tokens in pathname
int ino;
int num_blocks;
int used_inodes;

typedef struct ext2_group_desc  GD;
typedef struct ext2_super_block SUPER;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;

//blocks
GD *bg;
SUPER *sp;
INODE *ip;
DIR *dp;

//INODE *root_inode;

int get_block(int fd, int blk, char buf[ ]) //credit to KCW
{
  lseek(fd, (long)blk*BLKSIZE, 0);
  read(fd, buf, BLKSIZE);
}

void super() //credit to KCW, read in and print SUPER block
{
  // read SUPER block
  get_block(fd, 1, buf);  
  sp = (SUPER *)buf;

  // check for EXT2 magic number:

  //printf("s_magic = %x\n", sp->s_magic);
  if (sp->s_magic != 0xEF53){
    printf("NOT an EXT2 FS\n");
    exit(1);
  }

  printf("************Super Block***************\n");

  printf("s_inodes_count = %d\n", sp->s_inodes_count);
  printf("s_blocks_count = %d\n", sp->s_blocks_count);

  printf("s_free_inodes_count = %d\n", sp->s_free_inodes_count);
  printf("s_free_blocks_count = %d\n", sp->s_free_blocks_count);
  printf("s_first_data_block = %d\n", sp->s_first_data_block);


  printf("s_log_block_size = %d\n", sp->s_log_block_size);
  printf("s_log_frag_size = %d\n", sp->s_log_frag_size);

  printf("s_blocks_per_group = %d\n", sp->s_blocks_per_group);
  printf("s_frags_per_group = %d\n", sp->s_frags_per_group);
  printf("s_inodes_per_group = %d\n", sp->s_inodes_per_group);


  printf("s_mnt_count = %d\n", sp->s_mnt_count);
  printf("s_max_mnt_count = %d\n", sp->s_max_mnt_count);

  printf("s_magic = %x\n", sp->s_magic);

  //printf("s_mtime = %s", ctime(&sp->s_mtime));
  //printf("s_wtime = %s", ctime(&sp->s_wtime));
  
  used_inodes = sp->s_inodes_count - sp->s_free_inodes_count;
}

void group_descriptor() //read in gd block
{
  // read GD block
  get_block(fd, 2, buf);  
  bg = (GD *)buf;

  printf("\n*******Group Descriptor Block********\n");

  printf("bg_block_bitmap = %d\n", bg->bg_block_bitmap);
  printf("bg_inode_bitmap = %d\n", bg->bg_inode_bitmap);
  printf("bg_inode_table = %d\n", bg->bg_inode_table);
  printf("bg_free_blocks_count = %d\n", bg->bg_free_blocks_count);
  printf("bg_free_inodes_count = %d\n", bg->bg_free_inodes_count);
  printf("bg_used_dirs_count = %d\n", bg->bg_used_dirs_count);

  //determine where inodes begin on the disk using bg_inode_table
  inodes_begin = bg->bg_inode_table;
}

void get_root_inode()
{
  // read first inode block
  get_block(fd, inodes_begin, ip_buf);  
  
  ip = (INODE* )ip_buf + 1;  //second inode, / directory
  
  printf("\n*************Root inode**************\n");  

  printf("mode = %4x ", ip->i_mode);
  printf("uid = %d  gid = %d\n", ip->i_uid, ip->i_gid);
  printf("size = %d\n", ip->i_size);
  //printf("time = %s", ctime(&ip->i_ctime));
  printf("link = %d\n", ip->i_links_count);
  printf("i_blocks = %d\n", ip->i_blocks);
  printf("i_block[0] = %d\n", ip->i_block[0]);

  //root directory contains entries in the 0th data block (i_block[0])

  //read the block into buf
  get_block(fd, ip->i_block[0], buf);
  //let DIR *dp point at buf
  dp = (DIR *)buf;

  /*printf("\nRoot Directory Entry\n");
  printf("inode = %d\n", dp->inode);
  printf("rec_len = %d\n", dp->rec_len);
  printf("name_len = %d\n", dp->name_len);
  printf("name = %s\n", dp->name); */

}

void split_pathname(char* name)
{
  int i = 0;
  char *token = strtok(name, "/");
  printf("name = %s\n", name);
  printf("token = %s\n", token);
  while(token != 0)
  {
    pathname[i] = token;
    printf("token = %s\n", token);
    i++;
    token = strtok(0, "/");
  }
  n = i;

  printf("\nPathname has been split\n");
  i = 0;
  while(pathname[i] != 0)
  {
    printf("inside while loop\n");
    printf("pathname[%d] = %s\n", i, pathname[i]);
    i++;
  }
  printf("leaving split_pathname\n");
}

//inodePtr passed in will not always be root
//if found, return inode number. if not, return 0
int search(INODE * inodePtr, char * name)
{
  get_block(fd, inodePtr->i_block[0], buf);

  //read the block into buf 
  //let DIR *dp and char *cp BOTH point at buf
  DIR *dir = (DIR *)buf;
  char *cp = buf;

  printf("\n********Root Directory Entries******\n");
  printf("  inode  rec_len  name_len  name\n");

  // search for name string in the data blocks of this INODE
  int i = 0;
  do
  {
    printf("    %d      %d       %d       %s\n", dir->inode, dir->rec_len, dir->name_len, dir->name);
    if(strcmp(dir->name, name) == 0)
    {
      printf("\ninumber for %s has been found: %d\n", dir->name, dir->inode);
      return dir->inode;
    }
    else
    {
      cp += dir->rec_len; //advance cp by rec_len bytes
      dir = (DIR *)cp; //pull dir along to the next record
    }
  }while(strcmp(dir->name, "") != 0);

  //if we've gotten here, the name wasn't found
  return 0;   
}

int get_ino() //returns the ino and sets *ip to INDODE of pathname
{
  int i = 0, inumber, blocknumber;
  for (i = 0; i < n; i++) //n is number of steps in pathname
  {
    inumber = search(ip, pathname[i]);  
    //printf("\ninumber = %d\n", inumber);
    if (inumber == 0) //: can't find name[i], BOMB OUT!
    {
      printf("inode could not be found\n");
      exit(1);
    }
    //use inumber to read in its INODE and let ip --> this INODE  
   //Recall that ino counts from 1.  Use the Mailman's algorithm

   //            (ino - 1) / 8    and   InodeBeginBlock    
   //            (ino - 1) % 8 
               
    blocknumber = ((inumber - 1) / 8) + inodes_begin;
    printf("block number = %d\n", blocknumber)

;
    get_block(fd, blocknumber, ip_buf);  
    ip = (INODE *)ip_buf + (inumber - 1) % 8;
  }
   //if you reach here, you must have ip --> the INODE of pathname.
   return inumber;
}

int get_block_number(int inumber)
{
  return (inumber - 1) / 8 + inodes_begin;
}

void display_inode(INODE *inode)
{
  printf("\n*************inode****************\n");  

  printf("mode = %4x ", ip->i_mode);
  printf("uid = %d  gid = %d\n", ip->i_uid, ip->i_gid);
  printf("size = %d\n", ip->i_size);
  //printf("time = %s", ctime(&ip->i_ctime));
  printf("link = %d\n", ip->i_links_count);
  printf("i_blocks = %d\n", ip->i_blocks);
  printf("i_block[0] = %d\n", ip->i_block[0]);
  
}

void display_direct_blocks(INODE *inode)
{
  int i = 0;
  printf("\n************Direct Blocks***********\n");
  while(inode->i_block[i] != 0 && i < 12)
  {
     printf("%d\n", inode->i_block[i]);
     i++;
  }
}

void display_indirect_block(INODE *inode)
{
  int i = 0;
  if(inode->i_block[12] != 0)
  {
    printf("\n************Indirect Block***********\n");
    
    for(i = 0; i < 256; i++)
    {
      printf("%d\n", inode->i_block[12]);
    }
  }
}

void display_double_indirect_block(INODE *inode)
{
  if(inode->i_block[13] != 0)
  {
    printf("\n************Indirect Block***********\n");
    printf("%d\n", inode->i_block[12]);
  }
}

int main(int argc, char *argv[ ])
{
  //1. Open the device for READ. 
  if (argc > 1)
    disk = argv[1];
  fd = open(disk, O_RDONLY);
  if (fd < 0)
  {
    printf("open failed\n");
    exit(1);
  }

  //read in superblock
  super();

  //read in group descriptor block, find where inodes begin
  group_descriptor();

  //inodes_begin global variable is now set, read in inode_begin block
  get_root_inode();
  
  //break up the pathname into components
  split_pathname(argv[2]);

  ino = get_ino();
  display_inode(ip);

  //display direct, indirect, and double indirect blocks
  display_direct_blocks(ip);
  display_indirect_block(ip);
  display_double_indirect_block(ip);
}


//6. Start from the root INODE in (3), search for name[0] in its data block(s).
   //For DIRs, you may assume that (the number of entries is small so that) it 
   //only has DIRECT data blocks. Therefore, search only the direct blocks for 
   //name[0].

   //Each data block of a DIR inode contains DIR structures of the form 

   //  [ino rlen nlen .   ] [ino rlen nlen ..  ] [ino rlen nlen NAME ] ....
   //  [ino rlen nlen NAME] [ino rlen nlen NAME] [ino rlen nlen NAME ] ....
     
   //where each NAME is a string (without terminating NULL !!!) of nlen chars. 
   //You may use nlen to extract the NAME string, and rlen to advance to the 
   //next DIR structure (Listen to lecture in class). 

   //If name[0] exists. you can find its inode number.

//7. Use the inode number, ino, to locate the corresponding INODE:
   //Recall that ino counts from 1.  Use the Mailman's algorithm

   //            (ino - 1) / 8    and   InodeBeginBlock    
   //            (ino - 1) % 8 
               
   //to read in the INODE of /cs360

   //NOTE: the number 8 comes from : for FD, blockSize=1024 and inodeSize=128. 
   //      If BlockSize != 1024, you must adjust the number 8 accordingly.

   //From the INODE of /cs360, you can easily determine whether it's a DIR.
   
   //(Remember S_ISDIR(), S_ISREG() ?)
  
   //If it's not a DIR, there can't be anything like /cs360/is ...., so give up.

   //If it's a DIR and there are more components yet to search (BAD NEWS!)
   //you must go on.

   //The problem now becomes:
   //    Search for name[1] in the INODE of /cs360
   //which is exactly the same as that of Step (6).

//8. Since Steps 6-7 will be repeated n times, you should implement a function
 
     //int search(INODE * inodePtr, char * name)
     //    {
            // search for name string in the data blocks of this INODE
            // if found, return name's inumber
            // else      return 0
     //    }


//7. Then, all you have to do is call search() n times, as sketched below.

   //Assume:    n,  name[0], ...., name[n-1]   are globals

   //ip --> INODE of /

   //for (i= 0; i < n; i++){
   //    inumber = search(ip, name[i])  
   //    if (inumber == 0) : can't find name[i], BOMB OUT!
   //    -------------------------------------------------------
   //    use inumber to read in its INODE and let ip --> this INODE 
   //}
  
   // if you reach here, you must have ip --> the INODE of pathname.


//8. Extract information from ip --> as required.
   //Pat yourself on the back and say: Good Job!

//9. EXTRA CREDITS (50%):
   //Make your showblock work (CORRECTLY!) for hard disk partitions
   //HELP: Consult Chapter 3.4.5 of TEXT on large EXT2/3 FS.

//10. SAMPLES SOLUTION in samples/SHOWBLOCK/:
   //         showblock.bin     diskimage
   //Run it under Linux as
   //         showblock.bin diskimage pathname 
                                          
//11. HOW TO CHECK EXT2 FS contents:

   //     #------ Run as a sh script -----------
   //    mount -o loop diskimage  /mnt
   //     ls -l /mnt
   //     (cd into /mnt to browse its contents)
   //     umount /mnt
   //     #------------------------------------
