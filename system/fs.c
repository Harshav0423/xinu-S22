#include <xinu.h>
#include <kernel.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// #ifdef FS
#include <fs.h>

static fsystem_t fsd;
int dev0_numblocks;
int dev0_blocksize;
char *dev0_blocks;

extern int dev0;

char block_cache[512];

#define SB_BLK 0 // Superblock
#define BM_BLK 1 // Bitmapblock

#define NUM_FD 16

filetable_t oft[NUM_FD]; // open file table
#define isbadfd(fd) (fd < 0 || fd >= NUM_FD || oft[fd].in.id == EMPTY)

#define INODES_PER_BLOCK (fsd.blocksz / sizeof(inode_t))
#define NUM_INODE_BLOCKS (((fsd.ninodes % INODES_PER_BLOCK) == 0) ? fsd.ninodes / INODES_PER_BLOCK : (fsd.ninodes / INODES_PER_BLOCK) + 1)
#define FIRST_INODE_BLOCK 2

/**
 * Helper functions
 */
int _fs_fileblock_to_diskblock(int dev, int fd, int fileblock)
{
  int diskblock;

  if (fileblock >= INODEDIRECTBLOCKS)
  {
    errormsg("No indirect block support! (%d >= %d)\n", fileblock, INODEBLOCKS - 2);
    return SYSERR;
  }

  // Get the logical block address
  diskblock = oft[fd].in.blocks[fileblock];

  return diskblock;
}

/**
 * Filesystem functions
 */
int _fs_get_inode_by_num(int dev, int inode_number, inode_t *out)
{
  int bl, inn;
  int inode_off;

  if (dev != dev0)
  {
    errormsg("Unsupported device: %d\n", dev);
    return SYSERR;
  }
  if (inode_number >= fsd.ninodes)
  {
    errormsg("inode %d out of range (> %s)\n", inode_number, fsd.ninodes);
    return SYSERR;
  }

  bl = inode_number / INODES_PER_BLOCK;
  inn = inode_number % INODES_PER_BLOCK;
  bl += FIRST_INODE_BLOCK;

  inode_off = inn * sizeof(inode_t);

  bs_bread(dev0, bl, 0, &block_cache[0], fsd.blocksz);
  memcpy(out, &block_cache[inode_off], sizeof(inode_t));

  return OK;
}

int _fs_put_inode_by_num(int dev, int inode_number, inode_t *in)
{
  int bl, inn;

  if (dev != dev0)
  {
    errormsg("Unsupported device: %d\n", dev);
    return SYSERR;
  }
  if (inode_number >= fsd.ninodes)
  {
    errormsg("inode %d out of range (> %d)\n", inode_number, fsd.ninodes);
    return SYSERR;
  }

  bl = inode_number / INODES_PER_BLOCK;
  inn = inode_number % INODES_PER_BLOCK;
  bl += FIRST_INODE_BLOCK;

  bs_bread(dev0, bl, 0, block_cache, fsd.blocksz);
  memcpy(&block_cache[(inn * sizeof(inode_t))], in, sizeof(inode_t));
  bs_bwrite(dev0, bl, 0, block_cache, fsd.blocksz);

  return OK;
}

int fs_mkfs(int dev, int num_inodes)
{
  int i;

  if (dev == dev0)
  {
    fsd.nblocks = dev0_numblocks;
    fsd.blocksz = dev0_blocksize;
  }
  else
  {
    errormsg("Unsupported device: %d\n", dev);
    return SYSERR;
  }

  if (num_inodes < 1)
  {
    fsd.ninodes = DEFAULT_NUM_INODES;
  }
  else
  {
    fsd.ninodes = num_inodes;
  }

  i = fsd.nblocks;
  while ((i % 8) != 0)
  {
    i++;
  }
  fsd.freemaskbytes = i / 8;

  if ((fsd.freemask = getmem(fsd.freemaskbytes)) == (void *)SYSERR)
  {
    errormsg("fs_mkfs memget failed\n");
    return SYSERR;
  }

  /* zero the free mask */
  for (i = 0; i < fsd.freemaskbytes; i++)
  {
    fsd.freemask[i] = '\0';
  }

  fsd.inodes_used = 0;

  /* write the fsystem block to SB_BLK, mark block used */
  fs_setmaskbit(SB_BLK);
  bs_bwrite(dev0, SB_BLK, 0, &fsd, sizeof(fsystem_t));

  /* write the free block bitmask in BM_BLK, mark block used */
  fs_setmaskbit(BM_BLK);
  bs_bwrite(dev0, BM_BLK, 0, fsd.freemask, fsd.freemaskbytes);

  // Initialize all inode IDs to EMPTY
  inode_t tmp_in;
  for (i = 0; i < fsd.ninodes; i++)
  {
    _fs_get_inode_by_num(dev0, i, &tmp_in);
    tmp_in.id = EMPTY;
    _fs_put_inode_by_num(dev0, i, &tmp_in);
  }
  fsd.root_dir.numentries = 0;
  for (i = 0; i < DIRECTORY_SIZE; i++)
  {
    fsd.root_dir.entry[i].inode_num = EMPTY;
    memset(fsd.root_dir.entry[i].name, 0, FILENAMELEN);
  }

  for (i = 0; i < NUM_FD; i++)
  {
    oft[i].state = 0;
    oft[i].fileptr = 0;
    oft[i].de = NULL;
    oft[i].in.id = EMPTY;
    oft[i].in.type = 0;
    oft[i].in.nlink = 0;
    oft[i].in.device = 0;
    oft[i].in.size = 0;
    memset(oft[i].in.blocks, 0, sizeof(oft[i].in.blocks));
    oft[i].flag = 0;
  }

  return OK;
}

int fs_freefs(int dev)
{
  if (freemem(fsd.freemask, fsd.freemaskbytes) == SYSERR)
  {
    return SYSERR;
  }

  return OK;
}

/**
 * Debugging functions
 */
void fs_print_oft(void)
{
  int i;

  printf("\n\033[35moft[]\033[39m\n");
  printf("%3s  %5s  %7s  %8s  %6s  %5s  %4s  %s\n", "Num", "state", "fileptr", "de", "de.num", "in.id", "flag", "de.name");
  for (i = 0; i < NUM_FD; i++)
  {
    if (oft[i].de != NULL)
      printf("%3d  %5d  %7d  %8d  %6d  %5d  %4d  %s\n", i, oft[i].state, oft[i].fileptr, oft[i].de, oft[i].de->inode_num, oft[i].in.id, oft[i].flag, oft[i].de->name);
  }

  printf("\n\033[35mfsd.root_dir.entry[] (numentries: %d)\033[39m\n", fsd.root_dir.numentries);
  printf("%3s  %3s  %s\n", "ID", "id", "filename");
  for (i = 0; i < DIRECTORY_SIZE; i++)
  {
    if (fsd.root_dir.entry[i].inode_num != EMPTY)
      printf("%3d  %3d  %s\n", i, fsd.root_dir.entry[i].inode_num, fsd.root_dir.entry[i].name);
  }
  printf("\n");
}

void fs_print_inode(int fd)
{
  int i;

  printf("\n\033[35mInode FS=%d\033[39m\n", fd);
  printf("Name:    %s\n", oft[fd].de->name);
  printf("State:   %d\n", oft[fd].state);
  printf("Flag:    %d\n", oft[fd].flag);
  printf("Fileptr: %d\n", oft[fd].fileptr);
  printf("Type:    %d\n", oft[fd].in.type);
  printf("nlink:   %d\n", oft[fd].in.nlink);
  printf("device:  %d\n", oft[fd].in.device);
  printf("size:    %d\n", oft[fd].in.size);
  printf("blocks: ");
  for (i = 0; i < INODEBLOCKS; i++)
  {
    printf(" %d", oft[fd].in.blocks[i]);
  }
  printf("\n");
  return;
}

void fs_print_fsd(void)
{
  int i;

  printf("\033[35mfsystem_t fsd\033[39m\n");
  printf("fsd.nblocks:       %d\n", fsd.nblocks);
  printf("fsd.blocksz:       %d\n", fsd.blocksz);
  printf("fsd.ninodes:       %d\n", fsd.ninodes);
  printf("fsd.inodes_used:   %d\n", fsd.inodes_used);
  printf("fsd.freemaskbytes  %d\n", fsd.freemaskbytes);
  printf("sizeof(inode_t):   %d\n", sizeof(inode_t));
  printf("INODES_PER_BLOCK:  %d\n", INODES_PER_BLOCK);
  printf("NUM_INODE_BLOCKS:  %d\n", NUM_INODE_BLOCKS);

  inode_t tmp_in;
  printf("\n\033[35mBlocks\033[39m\n");
  printf("%3s  %3s  %4s  %4s  %3s  %4s\n", "Num", "id", "type", "nlnk", "dev", "size");
  for (i = 0; i < NUM_FD; i++)
  {
    _fs_get_inode_by_num(dev0, i, &tmp_in);
    if (tmp_in.id != EMPTY)
      printf("%3d  %3d  %4d  %4d  %3d  %4d\n", i, tmp_in.id, tmp_in.type, tmp_in.nlink, tmp_in.device, tmp_in.size);
  }
  for (i = NUM_FD; i < fsd.ninodes; i++)
  {
    _fs_get_inode_by_num(dev0, i, &tmp_in);
    if (tmp_in.id != EMPTY)
    {
      printf("%3d:", i);
      int j;
      for (j = 0; j < 64; j++)
      {
        printf(" %3d", *(((char *)&tmp_in) + j));
      }
      printf("\n");
    }
  }
  printf("\n");
}

void fs_print_dir(void)
{
  int i;

  printf("%22s  %9s  %s\n", "DirectoryEntry", "inode_num", "name");
  for (i = 0; i < DIRECTORY_SIZE; i++)
  {
    printf("fsd.root_dir.entry[%2d]  %9d  %s\n", i, fsd.root_dir.entry[i].inode_num, fsd.root_dir.entry[i].name);
  }
}

int fs_setmaskbit(int b)
{
  int mbyte, mbit;
  mbyte = b / 8;
  mbit = b % 8;

  fsd.freemask[mbyte] |= (0x80 >> mbit);
  return OK;
}

int fs_getmaskbit(int b)
{
  int mbyte, mbit;
  mbyte = b / 8;
  mbit = b % 8;

  return (((fsd.freemask[mbyte] << mbit) & 0x80) >> 7);
}

int fs_clearmaskbit(int b)
{
  int mbyte, mbit, invb;
  mbyte = b / 8;
  mbit = b % 8;

  invb = ~(0x80 >> mbit);
  invb &= 0xFF;

  fsd.freemask[mbyte] &= invb;
  return OK;
}

/**
 * This is maybe a little overcomplicated since the lowest-numbered
 * block is indicated in the high-order bit.  Shift the byte by j
 * positions to make the match in bit7 (the 8th bit) and then shift
 * that value 7 times to the low-order bit to print.  Yes, it could be
 * the other way...
 */
void fs_printfreemask(void)
{ // print block bitmask
  int i, j;

  for (i = 0; i < fsd.freemaskbytes; i++)
  {
    for (j = 0; j < 8; j++)
    {
      printf("%d", ((fsd.freemask[i] << j) & 0x80) >> 7);
    }
    printf(" ");
    if ((i % 8) == 7)
    {
      printf("\n");
    }
  }
  printf("\n");
}

/**
 * TODO: implement the functions below
 */
int fs_open(char *filename, int flags)
{

  if (flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR || fsd.root_dir.numentries <= 0)
  {
    return SYSERR;
  }

  // searching in the root DIRECTORY_SIZE
  for (int i = 0; i < DIRECTORY_SIZE; i++)
  {

    if (fsd.root_dir.entry[i].inode_num != -1)
    {

      if (strcmp(fsd.root_dir.entry[i].name, filename) == 0 && oft[i].state == FSTATE_OPEN)
      {
        return SYSERR;
      }
      if (oft[i].state != FSTATE_OPEN)
      {

        if (strcmp(fsd.root_dir.entry[i].name, filename) == 0)
        {

          // making an entry into file_table
          oft[i].state = FSTATE_OPEN;
          oft[i].fileptr = 0;
          oft[i].de = &fsd.root_dir.entry[i];
          // getting file entry inode to new DS
          _fs_get_inode_by_num(dev0, fsd.root_dir.entry[i].inode_num, &oft[i].in);
          oft[i].flag = flags;

          return i;
        }
      }
    }
  }

  return SYSERR;
}

int fs_close(int fd)
{
  // check state as other than closed
  if (oft[fd].state != FSTATE_CLOSED)
  {
    // fd id of filetable
    if (fd >= 0 && fd <= NUM_FD)
    {
      oft[fd].state = FSTATE_CLOSED;
      oft[fd].fileptr = 0;
      return OK;
    }
    else
    {
      return SYSERR;
    }
  }
  else
  {
    return SYSERR;
  }
}

int fs_create(char *filename, int mode)
{
  // inodes not matching
  if (fsd.ninodes == fsd.inodes_used || fsd.root_dir.numentries >= DIRECTORY_SIZE)
  {
    return SYSERR;
  }
  // searching DIR for file
  for (int i = 0; i < DIRECTORY_SIZE; i++)
  {
    if (strcmp(fsd.root_dir.entry[i].name, filename) == 0)
    {
      return SYSERR;
    }
  }
  // should add mode O_CREATE
  if (fsd.root_dir.numentries >= NUM_FD || mode != O_CREAT)
  {
    return SYSERR;
  }
  struct inode new_inode;
  int new_inode_id = -1;
  for (int i = 0; i < fsd.ninodes; i++)
  {
    _fs_get_inode_by_num(dev0, i, &new_inode);
    if (new_inode.id == -1)
    {
      new_inode_id = i;
      // inode_true = i;
      break;
    }
  }
  if (new_inode_id == -1)
  {
    return SYSERR;
  }

  new_inode.id = new_inode_id;
  new_inode.type = INODE_TYPE_FILE;
  new_inode.nlink = 1;
  new_inode.device = 0;
  new_inode.size = 0;

  _fs_put_inode_by_num(dev0, new_inode.id, &new_inode);

  // setting blocks to -1
  // memset(new_inode.blocks, EMPTY, sizeof(new_inode.blocks));

  for (int i = 0; i < DIRECTORY_SIZE; i++)
  {
    if (fsd.root_dir.entry[i].inode_num == -1)
    {
      strcpy(fsd.root_dir.entry[i].name, filename);
      fsd.root_dir.entry[i].inode_num = new_inode.id;
      break;
    }
  }

  fsd.root_dir.numentries++;
  fsd.inodes_used++;

  return fs_open(filename, O_RDWR);
}

int fs_seek(int fd, int offset)
{
  if (oft[fd].state == FSTATE_CLOSED || isbadfd(fd) || offset < 0 || offset > oft[fd].in.size) // file not found
  {
    return SYSERR;
  }
  else if (oft[fd].state == FSTATE_OPEN)
  {
    oft[fd].fileptr = offset;
    return OK;
  }
  return SYSERR;
}

int fs_read(int fd, void *buf, int nbytes)
{
  // filetable flags "check"
  if ((oft[fd].flag != O_RDWR && oft[fd].flag != O_RDONLY) || oft[fd].state == FSTATE_CLOSED || isbadfd(fd))
  {
    return SYSERR;
  }
  // given bytes <0
  if (nbytes < 0 || !buf)
  {
    return SYSERR;
  }
  // // num of bytes left to read
  // if ((oft[fd].in.size - oft[fd].fileptr) < nbytes)
  // {
  //   nbytes = oft[fd].in.size - oft[fd].fileptr;
  // }

  // int bytes_to_copy = nbytes;

  int strt_block = (oft[fd].fileptr / fsd.blocksz); // starting block
  int blk_width = oft[fd].fileptr % fsd.blocksz;    // block id to start

  int blkr_to_read = fsd.blocksz - blk_width; // inblock rem to read

  int r_bytes = 0;
  void *temp_buf = buf;
  for (int i = 0; i < nbytes; i++)
  {

    strt_block = (oft[fd].fileptr / fsd.blocksz); // starting block
    blk_width = oft[fd].fileptr % fsd.blocksz;    // block id to start
    blkr_to_read = fsd.blocksz - blk_width;       // inblock rem to read

    if (oft[fd].fileptr < oft[fd].in.size) // bounds of block
    {
      bs_bread(dev0, oft[fd].in.blocks[strt_block], blk_width, temp_buf, 1); // reading byte by byte

      oft[fd].fileptr += 1; // moving pointer

      temp_buf = (char *)temp_buf + 1; // allocating new byte
      r_bytes += 1;
    }
    else
    {
      // break;
      return r_bytes;
    }
  }

  return r_bytes;
}

// free block
int find_free_block()
{
  // searching for free block
  for (int i = 18; i < fsd.nblocks; i++)
  {

    if (fs_getmaskbit(i) == 0)
    {
      return i;
    }
  }
  return SYSERR;
}

int fs_write(int fd, void *buf, int nbytes)
{
  if ((oft[fd].flag == O_RDONLY) || oft[fd].state == FSTATE_CLOSED || isbadfd(fd))
  {
    return SYSERR;
  }
  // given bytes <0
  if (nbytes < 0 || !buf)
  {
    return SYSERR;
  }

  int strt_block = (oft[fd].fileptr / fsd.blocksz); // starting block
  int blk_width = oft[fd].fileptr % fsd.blocksz;    // block id to start

  int blkr_to_read = fsd.blocksz - blk_width; // inblock rem to read

  int r_bytes = 0;
  void *temp_buf = buf;
  for (int i = 0; i < nbytes; i++)
  {

    if (oft[fd].fileptr >= oft[fd].in.size) // if the cur block reaching limit of block size
    {
      int free_block_num = find_free_block();

      // if no free block found
      if (free_block_num == -1)
      {
        return r_bytes;
      }

      // updating inodes in OFT
      int free_inode_block = -1;
      // finding a free inode
      // int j = 0;
      for (int j = 0; j < INODEDIRECTBLOCKS; j++)
      {

        if (oft[fd].in.blocks[j] == 0)
        {
          // printf("Im here in write free node%d", j);
          free_inode_block = j;
          break;
        }
      }
      if (free_inode_block == -1)
      {
        // printf("here in the free inode block");
        return r_bytes;
      }
      // setting given block as used
      fs_setmaskbit(free_block_num);

      oft[fd].in.blocks[free_inode_block] = free_block_num;
      oft[fd].in.size += fsd.blocksz;

      _fs_put_inode_by_num(dev0, oft[fd].in.id, &oft[fd].in); // update the inode in memory
    }

    // block id to start
    blkr_to_read = fsd.blocksz - blk_width; // inblock rem bytes to read
    bs_bwrite(dev0, oft[fd].in.blocks[strt_block], blk_width, temp_buf, 1);

    oft[fd].fileptr += 1;
    strt_block = (oft[fd].fileptr / fsd.blocksz); // starting block
    blk_width = oft[fd].fileptr % fsd.blocksz;
    temp_buf = (char *)temp_buf + 1; // allocating new byte
    r_bytes += 1;
  }

  return r_bytes;
}

int fs_link(char *src_filename, char *dst_filename)
{
  if (!src_filename)
  {
    return SYSERR;
  }
  if (!dst_filename)
  {
    return SYSERR;
  }
  if (strlen(src_filename) < 0 || strlen(src_filename) > FILENAMELEN)
  {
    return SYSERR;
  }
  if (strlen(dst_filename) < 0 || strlen(dst_filename) > FILENAMELEN)
  {
    return SYSERR;
  }
  int find_src_id = -1;
  // search the source exists or not
  for (int i = 0; i < DIRECTORY_SIZE; i++)
  {
    if (strcmp(fsd.root_dir.entry[i].name, src_filename) == 0)
    {
      find_src_id = fsd.root_dir.entry[i].inode_num;
    }
    if (strcmp(fsd.root_dir.entry[i].name, dst_filename) == 0)
    {
      return SYSERR;
    }
  }

  if (find_src_id == -1)
  {
    return SYSERR;
  }
  int dest_id = -1;
  // finding a empty dir entry
  for (int i = 0; i < fsd.root_dir.numentries; i++)
  {
    if (fsd.root_dir.entry[i].inode_num == -1)
    {
      dest_id = i;
      break;
    }
  }

  if (dest_id == -1)
  {
    return SYSERR;
  }
  struct inode cur_inode;
  //
  if (fsd.root_dir.numentries <= DIRECTORY_SIZE)
  {
    _fs_get_inode_by_num(dev0, find_src_id, &cur_inode);
    fsd.root_dir.entry[dest_id].inode_num = cur_inode.id;
    strcpy(fsd.root_dir.entry[dest_id].name, dst_filename);
    cur_inode.nlink++;
    _fs_put_inode_by_num(dev0, cur_inode.id, &cur_inode); // keeping
    return OK;
  }
  return SYSERR;
}

int fs_unlink(char *filename)
{

  int find_src_id = -1;
  // search the source exists or not
  for (int i = 0; i < DIRECTORY_SIZE; i++)
  {
    if (strcmp(fsd.root_dir.entry[i].name, filename) == 0)
    {
      find_src_id = fsd.root_dir.entry[i].inode_num;
      break;
    }
  }

  if (!filename)
  {
    return SYSERR;
  }
  if (strlen(filename) < 0 || strlen(filename) > FILENAMELEN)
  {
    return SYSERR;
  }
  if (find_src_id == -1)
  {
    return SYSERR;
  }

  struct inode cur_inode;
  _fs_get_inode_by_num(dev0, find_src_id, &cur_inode);

  // nlinks > 1
  if (cur_inode.nlink > 1)
  {
    int dest_id = -1;
    for (int i = 0; i < fsd.root_dir.numentries; i++)
    {
      if (fsd.root_dir.entry[i].inode_num == cur_inode.id)
      {
        fsd.root_dir.entry[i].inode_num = -1;
        cur_inode.nlink -= 1;
        _fs_put_inode_by_num(0, cur_inode.id, &cur_inode);
        strcpy(fsd.root_dir.entry[i].name, ""); // resetting the file name
      }
    }
    for (int i = 0; i < NUM_FD; i++)
    {
      if (oft[i].in.id == -1)
      {
        oft[i].in = cur_inode;
      }
    }
    fsd.root_dir.numentries--;
    return OK;
  }

  else if (cur_inode.nlink == 1)
  {

    for (int i = 0; i < INODEDIRECTBLOCKS; i++)
    {
      fs_clearmaskbit(cur_inode.blocks[i]); // clearing data block
      memset(cur_inode.blocks, -1, sizeof(cur_inode.blocks));
    }
    cur_inode.size = 0;
    cur_inode.device = 0;
    cur_inode.id = -1;
    fsd.inodes_used--;
    
    // deleting inode
    for (int i = 0; i < fsd.root_dir.numentries; i++)
    {
      if (fsd.root_dir.entry[i].inode_num == cur_inode.id)
      {
        fsd.root_dir.entry[i].inode_num = -1;
        cur_inode.nlink -= 1;
        strcpy(fsd.root_dir.entry[i].name, ""); // resetting the file name
      }
    }
    for (int i = 0; i < NUM_FD; i++)
    {
      if (oft[i].in.id == -1)
      {
        oft[i].in = cur_inode;
      }
    }
    fsd.root_dir.numentries--;
    return OK;
  }
  return SYSERR;
}

// #endif /* FS */