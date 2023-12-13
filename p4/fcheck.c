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

  /*........................................................................................*/
  /*  Structure of the xv6 file system */
  /* |boot|super|  inodes   | bit map  |  data ... data |  log  | */
  /*   0    1    2                                               */

  /* read the super block */
  /* addr points to boot, so we need +1 * BLOCK_SIZE to point to super block */
  sb = (struct superblock *)(addr + 1 * BLOCK_SIZE);
  printf("no. of fs size(total blocks) %d, no. of data blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);
  /* number of blocks to store inode, +1 for the upper bound*/
  numinodeblocks = (sb->ninodes / (IPB)) + 1;
  /* number of blocks to store bitmap, +1 for the upper bound*/
  numbitmapblocks = (sb->size / (BPB)) + 1;
  /* start address of inode blocks, +2 * BLOCK_SIZE to point to startaddress of inode*/
  inodeAddr = (char *)(addr + 2 * BLOCK_SIZE);
  /* start address of bitmap blocks */
  bitmapAddr = (char *)(inodeAddr + numinodeblocks * BLOCK_SIZE);
  /* start address of data blocks */
  datablocksAddr = (char *)(bitmapAddr + numbitmapblocks * BLOCK_SIZE);
  /* block number of first data blocks */
  firstdatablocknumber = 2 + numinodeblocks + numbitmapblocks;
  /* read the specific file image inode address*/
  fimg_inode_address = (struct dinode *)(addr + IBLOCK(buf.st_ino) * BLOCK_SIZE);

  // output info
  printf("no. of inode per block %ld\n", IPB);
  printf("no. of bits per block %d\n", BPB);
  printf("no. of blocks for inode %d\n", numinodeblocks);
  printf("no. of blocks for bitmap %d   \n", numbitmapblocks);
  printf("start address of inode blocks for xv6 file system %p\n", inodeAddr);
  printf("start address of bitmap blocks %p \n", bitmapAddr);
  printf("start address of data blocks %p \n", datablocksAddr);
  printf("start address of first data blocks %d \n", firstdatablocknumber);
  printf("file system begin addr %p, file img inode address %p , offset from address %ld \n", addr, fimg_inode_address, (char *)fimg_inode_address - addr);

  /*........................................................................................*/

  struct dinode *startInodeAddress;
  startInodeAddress = (struct dinode *)inodeAddr;

  // traverse all the indoe
  for (int i = 0; i < sb->ninodes; i++)
  {
    // rule1 : Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV) If not, print ERROR: bad inode.
    // type = 0 unallocated, type = 1,2,3 (T_FILE, T_DIR, T_DEV)
    if ((startInodeAddress + i)->type < 0 || (startInodeAddress + i)->type > 3)
    {
      fprintf(stderr, "%s", "ERROR: bad inode.\n");
      exit(1);
    }

    /* rule2 : For in-use inodes, each block address that is used by the inode is valid (points to
    a valid data block address within the image). If the direct block is used and is
    invalid, print ERROR: bad direct address in inode.; if the indirect block is in use and is invalid, print ERROR: bad indirect address in inode. */

    // check direct block
    for (int j = 0; j < NDIRECT; j++)
    {
      // startInodeAddress[i] = *(startInodeAddress + i)
      uint directBlockAddress = startInodeAddress[i].addrs[j];
      // directBlock is not used
      if (directBlockAddress == 0)
        continue;
      // directBlock is out of range
      if (directBlockAddress < 0 || directBlockAddress >= sb->size)
      {
        fprintf(stderr, "%s", "ERROR: bad direct address in inode.\n");
        exit(1);
      }
    }

    // check Indirect block // 
    uint IndirectBlockAddress = startInodeAddress[i].addrs[NINDIRECT];
    fprintf(stdout, "%u\n", IndirectBlockAddress);

    uint *indirectblk; // 1 - 128
    indirectblk = (uint *)(addr + IndirectBlockAddress * BLOCK_SIZE);
    fprintf(stdout, "%p\n", indirectblk);
    // // IndirectBlock is not used
    // if (IndirectBlockAddress == 0)
    //   continue;
    // // IndirectBlock is out of range
    // if (IndirectBlockAddress < 0 || IndirectBlockAddress >= sb->size)
    // {
    //   fprintf(stderr, "%s", "ERROR: bad indirect address in inode.\n");
    //   exit(1);
    // }
    // // check the next level 128 direct pointers address inside the indrect block
    // for (int i = 0; i < NINDIRECT; i++)
    // {
    //   uint indirect
    // }
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