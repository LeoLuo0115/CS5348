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
#define DPB (BSIZE / sizeof(struct dirent))
#define T_DIR 1
#define T_FILE 2
#define T_DEV 3

char *addr;                        // Pointer to the address where the file system image will be mapped into memory
struct dinode *fimg_inode_address; // Pointer to the inode in the file system image
struct superblock *sb;             // Pointer to the superblock structure in the file system image
struct dirent *de;                 // Pointer to the directory entry in the file system image
uint numinodeblocks;               // Number of inode blocks in the file system image
uint numbitmapblocks;              // Number of bitmap blocks in the file system image
uint firstdatablocknumber;         // The block number of the first data block in the file system image
char *inodeAddr;                   // Address where the inode blocks start in the file system image
char *bitmapAddr;                  // Address where the bitmap blocks start in the file system image
char *datablocksAddr;              // Address where the data blocks start in the file system image

int main(int argc, char *argv[])
{

  if (argc < 2)
  {
    fprintf(stderr, "Usage: fcheck <file_system_image>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDONLY);
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

  // preprocess the file system image and print out the information
  // printf("start address of file in memory: %p\n", addr);
  // printf("no. of fs size(total blocks) %d, no. of data blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);
  // printf("no. of inode per block %ld\n", IPB);
  // printf("no. of bits per block %d\n", BPB);
  // printf("no. of blocks for inode %d\n", numinodeblocks);
  // printf("no. of blocks for bitmap %d   \n", numbitmapblocks);
  // printf("start address of inode blocks for xv6 file system %p\n", inodeAddr);
  // printf("start address of bitmap blocks %p \n", bitmapAddr);
  // printf("start address of data blocks %p \n", datablocksAddr);
  // printf("start address of first data blocks %d \n", firstdatablocknumber);
  // printf("file system begin addr %p, file img inode address %p , offset from address %ld \n", addr, fimg_inode_address, (char *)fimg_inode_address - addr);

  /*........................................................................................*/

  struct dinode *startInodeAddress;
  startInodeAddress = (struct dinode *)inodeAddr;
  uint data_block_start_number = (datablocksAddr - addr) / BSIZE;
  int blocks_in_use[sb->size];
  memset(blocks_in_use, 0, (sb->size) * sizeof(blocks_in_use[0]));
  int inodes_in_use[sb->ninodes];
  memset(inodes_in_use, 0, (sb->ninodes) * sizeof(inodes_in_use[0]));

  // traverse all the indoe
  for (int i = 0; i < sb->ninodes; i++)
  {

    // rule1 : Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV). If not, print ERROR: bad inode.
    if (startInodeAddress[i].type < 0 || startInodeAddress[i].type > 3)
    {
      fprintf(stderr, "%s", "ERROR: bad inode.\n");
      exit(1);
    }

    // rule 2-1 check direct block: For in-use inodes, each address that is used by inode is valid (points to a valid datablock address within the image). If the direct block is used and is invalid, print ERROR: bad direct address in inode.
    for (int j = 0; j < NDIRECT; j++)
    {
      // startInodeAddress[i] = *(startInodeAddress + i)
      uint directDataBlockNum = startInodeAddress[i].addrs[j];
      // fprintf(stdout, "%u", directDataBlockNum);
      // directBlock is used
      if (directDataBlockNum != 0)
      {
        // directBlock is used but invalid
        if (directDataBlockNum < data_block_start_number || directDataBlockNum >= data_block_start_number + sb->nblocks)
        {
          fprintf(stderr, "%s", "ERROR: bad direct address in inode.\n");
          exit(1);
        }
        // directBlock is used more than once
        if (blocks_in_use[directDataBlockNum] == 1)
        {
          fprintf(stderr, "%s", "ERROR: direct address used more than once.\n");
          exit(1);
        }
        // directBlock is used once
        blocks_in_use[directDataBlockNum] = 1;
      }
    }

    // rule 2-2 check indirect block: For in-use inodes, each indirect address in use is also valid. If the indirect block is in use and is invalid, print ERROR: bad indirect address in inode.
    // we have to first make sure the indirect block number is valid, then we can check the indirect block content, we will need indirect block adress to check the indirect block content, and for each indirect block content(direct data block number), we will need to check whether it is valid
    uint indirectBlockNum = startInodeAddress[i].addrs[NDIRECT];
    // indirectBlock is used, we can not skip inode if indeirectBlock is not used, because we need to check directory entry in direct block; if we skip inode, we will miss the directory entry in direct block in rule 4
    if (indirectBlockNum != 0)
    {
      // indirectBlock is used but invalid
      if ((indirectBlockNum < data_block_start_number || indirectBlockNum >= data_block_start_number + sb->nblocks))
      {
        fprintf(stderr, "%s", "ERROR: bad indirect address in inode.\n");
        exit(1);
      }
      // indirectBlock is used more than once
      if (blocks_in_use[indirectBlockNum] == 1)
      {
        fprintf(stderr, "%s", "ERROR: indirect address used more than once.\n");
        exit(1);
      }
      // indirectBlock is used once
      blocks_in_use[indirectBlockNum] = 1;

      // check indirect block points to valid data block
      uint *indirectBlockAddr = (uint *)(addr + indirectBlockNum * BSIZE);
      for (int j = 0; j < NINDIRECT; j++)
      {
        uint indirectDataBlockNum = indirectBlockAddr[j];
        // indirectBlock is not used
        if (indirectDataBlockNum == 0)
          continue;
        // indirectBlock is used but invalid
        if (indirectDataBlockNum < data_block_start_number || indirectDataBlockNum >= data_block_start_number + sb->nblocks)
        {
          fprintf(stderr, "%s", "ERROR: bad indirect address in inode.\n");
          exit(1);
        }
        if (blocks_in_use[indirectDataBlockNum] == 1)
        {
          fprintf(stderr, "%s", "ERROR: indirect address used more than once.\n");
          exit(1);
        }
        blocks_in_use[indirectDataBlockNum] = 1;
      }
    }

    // rule 4: Each directory contains . and .. entries, and the . entry points to the directory itself. If not, print ERROR: directory not properly formatted.
    if (startInodeAddress[i].type == T_DIR)
    {
      // check . entry, . entry points to the directory itself and its inode number is i
      struct dirent *de = (struct dirent *)(addr + startInodeAddress[i].addrs[0] * BSIZE);
      if (strcmp(de[1].name, "..") != 0 || strcmp(de[0].name, ".") != 0 || de[0].inum != i)
      {
        fprintf(stderr, "%s", "ERROR: directory not properly formatted.\n");
        exit(1);
      }
    }

    // rule 3: Root directory exists, its inode number is 1, and the parent of the root directory is itself. If not, print ERROR: root directory does not exist.
    if (i == ROOTINO)
    {
      if (startInodeAddress[i].type != T_DIR)
      {
        fprintf(stderr, "%s", "ERROR: root directory does not exist.\n");
        exit(1);
      }
      // check . entry, . entry points to the directory itself and its inode number is 1
      struct dirent *de = (struct dirent *)(addr + startInodeAddress[i].addrs[0] * BSIZE);
      if (de->inum != ROOTINO || strcmp(de->name, ".") != 0)
      {
        fprintf(stderr, "%s", "ERROR: root directory does not exist.\n");
        exit(1);
      }
      // check .. entry, .. entry points to the directory itself and its inode number is 1
      de = (struct dirent *)(addr + startInodeAddress[i].addrs[0] * BSIZE + sizeof(struct dirent));
      if (de->inum != ROOTINO || strcmp(de->name, "..") != 0)
      {
        fprintf(stderr, "%s", "ERROR: root directory does not exist.\n");
        exit(1);
      }
    }
    
    
  }

  exit(0);
}