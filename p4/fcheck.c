#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "fs.h"

#define BLOCK_SIZE (BSIZE)
#define T_DIR 1
#define T_FILE 2
#define T_DEV 3

int r, i, n, fd;
char *addr;
struct dinode *fimg_inode_address;
struct superblock *sb;
struct dirent *de;
uint numinodeblocks;
uint numbitmapblocks;
uint firstdatablocknumber;
char *inodeAddr;
char *bitmapAddr;
char *datablocksAddr;

int main(int argc, char *argv[])
{

  if (argc < 2)
  {
    fprintf(stderr, "Usage: fcheck <file_system_image>\n");
    exit(1);
  }

  fd = open(argv[1], O_RDONLY);
  if (fd < 0)
  {
    fprintf(stderr, "image not found\n");
    exit(1);
  }

  struct stat buf;
  if (fstat(fd, &buf) != 0)
  {
    fprintf(stderr, "failed to fstat file\n");
    exit(1);
  }

  /* Dont hard code the size of file. Use fstat to get the size */
  addr = mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED)
  {
    fprintf(stderr, "ERROR: mmap failed.\n");
    exit(1);
  }

  /* read the super block */
  sb = (struct superblock *)(addr + 1 * BLOCK_SIZE);
  printf("no. of fs size(total blocks) %d, no. of data blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);

  /* number of blocks to store inode */
  numinodeblocks = (sb->ninodes / (IPB)) + 1;
  /* number of blocks to store bitmap */
  numbitmapblocks = (sb->size / (BPB)) + 1;
  /* start address of inode blocks */
  inodeAddr = (char *)(addr + 2 * BLOCK_SIZE);
  /* start address of bitmap blocks */
  bitmapAddr = (char *)(inodeAddr + numinodeblocks * BLOCK_SIZE);
  /* start address of data blocks */
  datablocksAddr = (char *)(bitmapAddr + numbitmapblocks * BLOCK_SIZE);
  /* block number of first data blocks */
  firstdatablocknumber = 2 + numinodeblocks + numbitmapblocks;
  /* read the this file image inode address, calculate an offset based on a specific inode number to locate the position of a specific inode.*/
  fimg_inode_address = (struct dinode *)(addr + IBLOCK(buf.st_ino) * BLOCK_SIZE);

  // output info
  printf("no. of inode per block %ld\n", IPB);
  printf("no. of bits per block %d\n", BPB);
  printf("no. of blocks for inode %d\n", numinodeblocks);
  printf("no. of blocks for bitmap %d   \n", numbitmapblocks);
  printf("start address of inode blocks %p\n", inodeAddr);
  printf("start address of bitmap blocks %p \n", bitmapAddr);
  printf("start address of data blocks %p \n", datablocksAddr);
  printf("block number of first data blocks %d \n", firstdatablocknumber);
  // printf("fs begin addr %p, file img inode address %p , offset from address %ld \n", addr, fimg_inode_address, (char *)fimg_inode_address - addr);

  struct dinode *dip;
  dip = (struct dinode *)inodeAddr;
  // rule1
  for (int i = 0; i < sb->ninodes; i++)
  {
    if (dip[i].type < 0 || dip[i].type > 3)
    {
      fprintf(stderr, "%s", "ERROR: bad inode.\n");
      exit(1);
    }
  }

  /*************************************************************************************/
  // read root inode
  // printf("Root inode  size %d links %d type %d \n", fimg_inode_address[ROOTINO].size, fimg_inode_address[ROOTINO].nlink, fimg_inode_address[ROOTINO].type);

  // // get the address of root dir
  // de = (struct dirent *)(addr + (fimg_inode_address[ROOTINO].addrs[0]) * BLOCK_SIZE);

  // // print the entries in the first block of root dir
  // n = fimg_inode_address[ROOTINO].size / sizeof(struct dirent);
  // for (i = 0; i < n; i++, de++)
  // {
  //   printf(" inum %d, name %s ", de->inum, de->name);
  //   printf("inode  size %d links %d type %d \n", fimg_inode_address[de->inum].size, fimg_inode_address[de->inum].nlink, fimg_inode_address[de->inum].type);
  // }
  /*************************************************************************************/

  exit(0);
}
