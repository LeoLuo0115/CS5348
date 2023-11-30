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

int r, i, n, fd;
char *addr;
struct dinode *dip;
struct superblock *sb;
struct dirent *de;
uint numinodeblocks;
uint numbitmapblocks;
uint firstdatablock;
char *inodeblocks;
char *bitmapblocks;
char *datablocks;

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
  printf("no. of inode per block %ld\n", IPB);
  printf("no. of bits per block %d\n", BPB);
  printf("no. of blocks for inode %d,\nno. of blocks for bitmap %d \n", numinodeblocks, numbitmapblocks);

  /* read the inodes */
  dip = (struct dinode *)(addr + IBLOCK(buf.st_ino) * BLOCK_SIZE);
  printf("begin addr %p, begin inode %p , offset %ld \n", addr, dip, (char *)dip - addr);

  // // read root inode
  // printf("Root inode  size %d links %d type %d \n", dip[ROOTINO].size, dip[ROOTINO].nlink, dip[ROOTINO].type);

  // // get the address of root dir
  // de = (struct dirent *)(addr + (dip[ROOTINO].addrs[0]) * BLOCK_SIZE);

  // // print the entries in the first block of root dir
  // n = dip[ROOTINO].size / sizeof(struct dirent);
  // for (i = 0; i < n; i++, de++)
  // {
  //   printf(" inum %d, name %s ", de->inum, de->name);
  //   printf("inode  size %d links %d type %d \n", dip[de->inum].size, dip[de->inum].nlink, dip[de->inum].type);
  // }
  exit(0);
}
