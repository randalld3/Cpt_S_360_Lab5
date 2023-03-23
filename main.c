/**************************************************
*  CS_360 Lab5                                    *
*  Authors: Randall Dickinson && Nicholas Gerth   *
*  Sample Code Provided by Professor K.C. Wang    *
*  Submission Date: 03.23.23                      *
**************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ext2fs/ext2_fs.h>
#include "type.h"

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc  GD;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;      

#define BLKSIZE  1024      

char buf[BLKSIZE], gbuf[BLKSIZE];

int ibuf[256], dbuf[256];

char *name[64];
char gpath[256];
char pathname[128];

SUPER *sp;
GD    *gp;
INODE *ip; 

int dev; 
int ino;
int bmap, imap, inodes_start; 
int ninodes, nblocks;

const char *disk = "diskimage";

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, blk*BLKSIZE, 0);
   return read(dev, buf, BLKSIZE);
}   

int tokenize(char *pathname)
{
  int i, n = 0;
  char *s;
  printf("tokenize %s\n", pathname);

  strcpy(gpath, pathname);

  s = strtok(gpath, "/");

  while(s){
    name[n++] = s;
    s = strtok(0, "/");
  }
  name[n] = 0;

  return n;
}

INODE *mount_root(int dev, int ino)
{
  INODE *ip;  
  int blk, offset;

  blk    = (ino-1) / 8 + inodes_start;
  offset = (ino-1) % 8;

  get_block(dev, blk, gbuf);
  ip = (INODE *)gbuf + offset;

  return ip;
}

int show_dir(INODE *ip)
{
  char sbuf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;
 
  // ASSUME only one data block i_block[0]
  // YOU SHOULD print i_block[0] number to see its value
  get_block(dev, ip->i_block[0], sbuf);

  dp = (DIR *)sbuf;
  cp = sbuf;
 
  while(cp < sbuf + BLKSIZE){
    strncpy(temp, dp->name, dp->name_len);
    temp[dp->name_len] = 0;  // convert dp->name into a string  

    printf("%8d%8d%8u       %s\n", dp->inode, dp->rec_len, dp->name_len, temp);

    cp += dp->rec_len;      // advance cp by rec_len
    dp = (DIR *)cp;         // pull dp to cp
   }
}

int search(INODE *ip, char *name)
{
  char sbuf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;
 
  // ASSUME only one data block i_block[0]
  // YOU SHOULD print i_block[0] number to see its value
  get_block(dev, ip->i_block[0], sbuf);

  dp = (DIR *)sbuf;
  cp = sbuf;
 
  while(cp < sbuf + BLKSIZE){
    strncpy(temp, dp->name, dp->name_len);
    temp[dp->name_len] = 0;  // convert dp->name into a string
      
    printf("%8d%8d%8u       %s\n", dp->inode, dp->rec_len, dp->name_len, temp);

    if (!strcmp(temp, name)){
      printf("found %s : ino = %d\n", dp->name, dp->inode);
      return dp->inode;
    }
    cp += dp->rec_len;      // advance cp by rec_len
    dp = (DIR *)cp;         // pull dp to cp
   }

   return(0);
}

int main(int argc, char *argv[])
{
  int i, j, n;
  char mbuf[BLKSIZE], temp[256];

  if (argc < 2){
    printf("Enter a pathname: ");
    scanf("%s", pathname);
  }
  else
    strcpy(pathname, argv[1]);

  if ((dev = open(disk, O_RDONLY)) < 0){
      printf("Failed to open %s \n", disk);  
      exit(1);
  }

  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  printf("(1). Verify it's an ext2 file system: ");
  printf("s_magic=%x  ", sp->s_magic);
  if (sp->s_magic != 0xEF53){
      printf("magic = %x it's not an ext2 file system\n", sp->s_magic);
      exit(1);
  }     
  printf("OK\n");

  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;
  printf("     ninodes=%d nblcks=%d\n", ninodes, nblocks);
    
  printf("(2). Read group descriptor 0 to get bmap imap inodes_start\n");
  get_block(dev, 2, buf); 
  gp = (GD *)buf;
  inodes_start = gp->bg_inode_table;
  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;

  printf("     bmap=%d imap=%d inodes_start=%d\n", bmap, imap, inodes_start);
  printf("*********** get root inode *************\n");

  ip = mount_root(dev, 2);

  printf("(3). Show root DIR contents\n");
  printf("root inode data block = %d\n", ip->i_block[0]);

  get_block(dev, ip->i_block[0], mbuf);

  printf("   i_number rec_len name_len   name\n");
  show_dir(ip);

  printf("enter a key to continue : "); 
  getchar();
  n = tokenize(pathname);

  for (i=0; i < n; i++)
    printf("name[%d] = %s\n", i, name[i]);
  getchar();

  ino = 2;
  ip = mount_root(dev, 2);

  for (i=0; i<n; i++){
    ino = search(ip, name[i]);
    if (ino == 0){
      printf("%s not found\n", name[i]);
      exit(1);
    }

    int blk = (ino-1)/8 + inodes_start;
    int offset = (ino-1)%8;
    get_block(dev, blk, buf);
    ip = (INODE *)buf + offset;
  }

  printf("get INODE of ino=%d\n", ino);
  ip = mount_root(dev, ino);

  printf("file size = %d bytes\n", ip->i_size);
  printf("All disk block numbers in INODE: \n");
  for (i=0; i<15; i++)
    printf("i_block[%d] = %d\n", i, ip->i_block[i]);
  
  printf("enter a key to continue ");
  getchar();
  printf("----------- DIRECT BLOCKS ----------------\n");
  for (i=0; i<12; i++){
    if (ip->i_block[i] == 0)
      break;
    printf("%d ", ip->i_block[i]);
  }
  printf("\n");
  
  if (ip->i_block[12]){
    printf("----------- INDIRECT BLOCKS ---------------\n");
    get_block(dev, ip->i_block[12], (char *)ibuf);

    i = 0;
    while(ibuf[i] && i < 256){
      printf("%d ", ibuf[i]);
      i++;
    }
  }
  printf("\n");

  if (ip->i_block[13]){
    get_block(dev, ip->i_block[13], (char *)dbuf);
    printf("----------- DOUBLE INDIRECT BLOCKS ---------------\n");

    i = 0;
    while(ibuf[i] && i < 256){
      get_block(dev, dbuf[i], (char *)ibuf);
      
      j = 0;
      while(ibuf[j] && j < 256){
        printf("%d ", ibuf[j]);
        j++;
      }
      i++;
    }
  }

  printf("\n");
}


