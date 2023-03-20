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
typedef unsigned char           u8;           

SUPER *sp;
GD    *gp;
INODE *ip;
DIR   *dp;   

#define BLKSIZE  1024             // block size, for bufs

char buf[BLKSIZE], gbuf[BLKSIZE];

int indbuf[256], dbuf[256];

char gpath[256];
char *name[64];
int   n;

int ninodes, nblocks;
int imap, bmap, inodes_start; 
int dev; 
int ino;

char pathname[128];


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
      
       printf("%4d %4d %4d %s\n", dp->inode, dp->rec_len, dp->name_len, temp);

       cp += dp->rec_len;      // advance cp by rec_len
       dp = (DIR *)cp;         // pull dp to cp
   }
}

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, blk*BLKSIZE, 0);
   int n = read(dev, buf, BLKSIZE);
   return n;
}   

int search(INODE *ip, char *name)
{
   int i;
   char *cp, sbuf[BLKSIZE], temp[256];
   DIR *dp;

   printf("search for %s in INODE ino=%d at %x\n", name, ino, ip);

   /**********  search for a file name ***************/
   for (i=0; i<12; i++){ /* search direct blocks only */
        if (ip->i_block[i] == 0) 
           return 0;
        printf("i=%d  i_block[%d]=%d\n", i, i, ip->i_block[i]);

        get_block(dev, ip->i_block[i], sbuf);
        dp = (DIR *)sbuf;
        cp = sbuf;
        printf("   i_number rec_len name_len    name\n");

        while(cp < sbuf + BLKSIZE){

	    strncpy(temp, dp->name, (u8)dp->name_len);
            temp[dp->name_len] = 0;
            printf("%8d%8d%8u        %s\n", 
		   dp->inode, dp->rec_len, (u8)dp->name_len, temp);

            if (strcmp(name, temp)==0){
                printf("found %s : ino = %d\n", name, dp->inode);
                return(dp->inode);
            }

            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
   }
   return(0);
}

/************************************************************************
 tokenize() breaks up a pathname into component strings.
 Example : pathname = /this/is/a/test
 Then n=4, namePtr[0] ="this", namePtr[1] ="is", namePtr[2] ="a", 
           namePtr[3] ="test" 
 The component names will be used to search for a child under its parent
*************************************************************************/

int tokenize(char *pathname)
{
  int i;
  char *s;
  printf("tokenize %s\n", pathname);

  strcpy(gpath, pathname);
  n = 0;

  s = strtok(gpath, "/");

  while(s){
    name[n++] = s;
    s = strtok(0, "/");
  }
  /*
  for (i= 0; i<n; i++)
    printf("%s  ", name[i]);
  printf("\n");
  */
}

INODE *iget(int dev, int ino)
{
  INODE *ip;  
  int blk, offset;

  blk    = (ino-1) / 8 + inodes_start;
  offset = (ino-1) % 8;

  get_block(dev, blk, gbuf);
  ip = (INODE *)gbuf + offset;

  return ip;
}

char *disk = "mydisk";

int main(int argc, char *argv[])
{
  int i, j;
  char *cp, mbuf[BLKSIZE], temp[256];
  char *tst;

  if (argc < 2){
    printf("enter a pathname : ");
    scanf("%s", pathname);
  }
  else
    strcpy(pathname, argv[1]);

  if ((dev = open(disk, O_RDONLY)) < 0){
      printf("open %s failed\n", disk);  exit(1);
  }

  /********** read super block at 1024 ****************/
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  printf("(1). Verify it's an ext2 file system: ");
  printf("s_magic=%x  ", sp->s_magic);
  if (sp->s_magic != 0xEF53){
      printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
      exit(1);
  }     
  printf("OK\n");

  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;
  printf("     ninodes=%d nblcks=%d\n", ninodes, nblocks);
    
  printf("(2). Read group descriptor 0 to get bmap imap inodes_start\n");
  get_block(dev, 2, buf); 
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  inodes_start = gp->bg_inode_table;
  printf("     bmap=%d imap=%d inodes_start=%d\n", bmap, imap, inodes_start);

  printf("*********** get root inode *************\n");
  get_block(dev, inodes_start, buf);
  ip = (INODE *)buf + 1;

  ip = iget(dev, 2);

  printf("(3). Show root DIR contents\n");
  printf("root inode data block = %d\n", ip->i_block[0]);

  get_block(dev, ip->i_block[0], mbuf);
  dp = (DIR *)mbuf;
  cp = mbuf;

  printf("   i_number rec_len name_len   name\n");
  while (cp < mbuf + BLKSIZE){
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;

	printf("%8d%8d%8u       %s\n", 
                dp->inode, dp->rec_len, dp->name_len, temp);
	
        cp += dp->rec_len;
        dp = (DIR *)cp;
  }

  printf("enter a key to continue : "); getchar();

  tokenize(pathname);

  for (i=0; i < n; i++){
    printf("name[%d] = %s\n", i, name[i]);
  }

  getchar();
  
  ino = 2;
  ip = iget(dev, 2);

  // show_dir(ip);

  for (i=0; i<n; i++){
    ino = search(ip, name[i]);
    if (ino == 0){
      printf("can't find %s\n", name[i]);
      exit(1);
    }
    //ip = iget(dev, ino);
    int blk = (ino-1)/8 + inodes_start;
    int offset = (ino-1)%8;
    get_block(dev, blk, buf);
    ip = (INODE *)buf + offset;
    
  }

  printf("get INODE of ino=%d\n", ino);
  ip = iget(dev, ino);

  printf("file size = %d bytes\n", ip->i_size);
  printf("All disk block numbers in INODE:\n");
  for (i=0; i<15; i++){
    printf("i_block[%d] = %d\n", i, ip->i_block[i]);
  }

  printf("enter a key to continue");
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
    get_block(dev, ip->i_block[12], (char *)indbuf);
    for (i=0; i<256; i++){
      if (indbuf[i])
	printf("%d ", indbuf[i]);
    }
  }
  printf("\n");

  if (ip->i_block[13]){
    get_block(dev, ip->i_block[13], (char *)dbuf);
    printf("----------- DOUBLE INDIRECT BLOCKS ---------------\n");

    for (i=0; i<256; i++){
      if (dbuf[i] == 0)
         break;
      get_block(dev, dbuf[i], (char *)indbuf);
      for (j=0; j<256; j++){
	if (indbuf[j] == 0)
	  break;
        printf("%d ", indbuf[j]);
      }
    }
  }
  printf("\n");

}

