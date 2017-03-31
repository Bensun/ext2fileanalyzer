#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

int powerOfTwoTest(unsigned int x);

int main(int argc, char *argv[]) {
  int disk_img;
  ssize_t bytes_read;

  //open and read disk-image
  disk_img = open(argv[1], O_RDONLY);
  if (disk_img < 0) {
    fprintf(stderr, "Couldn't open disk image\n");
    return 1;
  }

  //super block
  //read into super buffer
  void *super;
  FILE *super_csv;

  super = (void *)malloc(264);
  if (super == NULL) {
    fprintf(stderr, "Couldn't allocate memory\n");
    return 1;
  }
  bytes_read = pread(disk_img, super, 264, 1024);
  if (bytes_read < 0) {
    fprintf(stderr, "Couldn't read disk image\n");
    return 1;
  }
  super_csv = fopen("super.csv", "w");
  if (super_csv < 0) {
    fprintf(stderr, "Couldn't open super.csv\n");
    return 1;
  }

  //assign super variables
  uint16_t magic;
  uint32_t total_inodes, total_blocks, block_size, frag_size, blocks_per_group,
  inodes_per_group, frags_per_group, first_data_block;

  magic = *(uint16_t *)(super + 56); //printf("magic number: %x\n", magic);
  total_inodes = *(uint32_t *)super; //printf("total number of inodes: %u\n", total_inodes);
  total_blocks = *(uint32_t *)(super + 4); //printf("total number of blocks: %u\n", total_blocks);
  block_size = 1024 << *(uint32_t *)(super + 24); //printf("block size: %u\n", block_size);
  if (*(uint32_t *)(super + 31) >= 0) {
    frag_size = 1024 << *(uint32_t *)(super + 28); //printf("frag size: %d\n", frag_size);
  }
  else {
    frag_size = 1024 >> *(uint32_t *)(super + 28); //printf("frag size: %d\n", frag_size);
  }
  blocks_per_group = *(uint32_t *)(super + 32); //printf("blocks per group: %u\n", blocks_per_group);
  inodes_per_group = *(uint32_t *)(super + 40); //printf("inodes per group: %u\n", inodes_per_group);
  frags_per_group = *(uint32_t *)(super + 36); //printf("frags per group: %u\n", frags_per_group);
  first_data_block = *(uint32_t *)(super + 20); //printf("first_data_block: %u\n", first_data_block);

  //super sanity check
  if (magic != 0xef53) {
    fprintf(stderr, "Superblock - invalid magic: %x\n", magic);
    return 1;
  }
  if (!powerOfTwoTest(block_size)) fprintf(stderr, "Superblock - invalid block size: %u\n", block_size);
  if (total_blocks % blocks_per_group != 0) {
    fprintf(stderr, "Superblock - %u blocks, %u blocks/group\n", total_blocks, blocks_per_group);
    return 1;
  }
  if (total_inodes % inodes_per_group != 0) {
    fprintf(stderr, "Superblock - %u inodes, %u inodes/group\n", total_inodes, inodes_per_group);
    return 1;
  }

  //write to super.csv
  fprintf(super_csv, "%x,%u,%u,%u,%d,%u,%u,%u,%u\n", magic, total_inodes, total_blocks,
  block_size, frag_size, blocks_per_group, inodes_per_group, frags_per_group, first_data_block);

  //group descriptor block
  //allocate memory for group buffer and open group csv
  void *group;
  FILE *group_csv;
  group = (void *)malloc(32);
  if (group == NULL) {
    fprintf(stderr, "Couldn't allocate memory for group\n");
    return 1;
  }
  group_csv = fopen("group.csv", "w");
  if (group_csv < 0) {
    fprintf(stderr, "Couldn't open group.csv\n");
    return 1;
  }

  //calculate number of groups
  int group_number = total_blocks / blocks_per_group; //printf("group number: %d\n", group_number);

  //initialize group array
  uint16_t free_blocks[group_number], free_inodes[group_number], used_dir[group_number];
  uint32_t inode_bitmap[group_number], block_bitmap[group_number], inode_table[group_number];

  //iterate through groups
  for (int i = 0; i < group_number; i++) {
    bytes_read = pread(disk_img, group, 32, 2048 + (32 * i));
    if (bytes_read < 0) {
      fprintf(stderr, "Couldn't read disk image for group\n");
      return 1;
    }

    //assign group variables
    free_blocks[i] = *(uint16_t *)(group + 12);
    free_inodes[i] = *(uint16_t *)(group + 14);
    used_dir[i] = *(uint16_t *)(group + 16);
    inode_bitmap[i] = *(uint32_t *)(group + 4);
    block_bitmap[i] = *(uint32_t *)group;
    inode_table[i] = *(uint32_t *)(group + 8);

    //group sanity check
    uint32_t start_block = blocks_per_group * i;
    uint32_t end_block = start_block + blocks_per_group;
    if (inode_bitmap[i] < start_block || inode_bitmap[i] > end_block) {
      fprintf(stderr, "Group %i: blocks %u-%u, free Inode map starts at %u\n", i + 1,
              start_block, end_block, inode_bitmap[i]);
      return 1;
    }
    if (block_bitmap[i] < start_block || block_bitmap[i] > end_block) {
      fprintf(stderr, "Group %i: blocks %u-%u, free Inode map starts at %u\n", i + 1,
              start_block, end_block, block_bitmap[i]);
      return 1;
    }

    //write to group.csv
    fprintf(group_csv, "%u,%u,%u,%u,%x,%x,%x\n", blocks_per_group, free_blocks[i],
    free_inodes[i], used_dir[i], inode_bitmap[i], block_bitmap[i], inode_table[i]);
  }

  //bitmap entry block
  //allocate memory for bitmap buffer and open bitmap csv
  void *bitmap;
  FILE *bitmap_csv;
  bitmap = (void *)malloc(block_size);
  if (bitmap == NULL) {
    fprintf(stderr, "Couldn't allocate memory for bitmap\n");
    return 1;
  }
  bitmap_csv = fopen("bitmap.csv", "w");
  if (bitmap_csv < 0) {
    fprintf(stderr, "Couldn't open bitmap.csv\n");
    return 1;
  }

  //inode entry block
  //allocate memory for bitmap buffer and open bitmap csv
  void *inode;
  FILE *inode_csv;
  inode_csv = fopen("inode.csv", "w");
  if (inode_csv < 0) {
    fprintf(stderr, "Couldn't open inode.csv\n");
    return 1;
  }
  uint16_t inode_number[total_inodes], inode_file_type[total_inodes], inode_owner[total_inodes],
  inode_group[total_inodes], inode_link_count[total_inodes];
  uint32_t inode_creation_time[total_inodes], inode_modification_time[total_inodes],
  inode_access_time[total_inodes], inode_file_size[total_inodes], inode_number_blocks[total_inodes],
  inode_block_ptrs[total_inodes][15];
  int allocated_inodes = 0;
  int directories[total_inodes];
  int directories_count = 0;

  //calculate blocks per inode table
  int inode_table_blocks = inodes_per_group / (block_size / 128);

  //iterate through groups
  for (int k = 0; k < group_number; k++) {
    //read from bitmap buffer
    bytes_read = pread(disk_img, bitmap, block_size, block_bitmap[k] * block_size);
    if (bytes_read < 0) {
      fprintf(stderr, "Couldn't read disk image for bitmap\n");
      return 1;
    }

    //iterate through block/inode bitmap
    for (int i = 0; i < blocks_per_group / 8; i++) {
      for (int j = 0; j < 8; j++) {
        if (!((*(uint8_t *)(bitmap + i) >> j) & 00000001)) {
          fprintf(bitmap_csv, "%x,%u\n", block_bitmap[k], k * blocks_per_group + ((8 * i) + j + 1));
        }
      }
    }

    //read from bitmap buffer
    bytes_read = pread(disk_img, bitmap, block_size, inode_bitmap[k] * block_size);
    if (bytes_read < 0) {
      fprintf(stderr, "Couldn't read disk image for bitmap\n");
      return 1;
    }

    //iterate through block/inode bitmap
    for (int i = 0; i < inodes_per_group / 8; i++) {
      for (int j = 0; j < 8; j++) {
        if (!((*(uint8_t *)(bitmap + i) >> j) & 00000001)) {
          fprintf(bitmap_csv, "%x,%u\n", inode_bitmap[k], k * inodes_per_group + ((8 * i) + j + 1));
        }
        else {
          //initialize inode variables
          uint64_t inode_offset = block_size * inode_table[k] + 128 * ((8 * i) + j);
          inode = (void *)malloc(inode_table_blocks * block_size);
          if (inode == NULL) {
            fprintf(stderr, "Couldn't allocate memory for inode\n");
            return 1;
          }

          //read from inode table
          bytes_read = pread(disk_img, inode, block_size, inode_offset);
          if (bytes_read < 0) {
            fprintf(stderr, "Couldn't read disk image for inode\n");
            return 1;
          }

          //write to inode.csv
          //inode number
          inode_number[allocated_inodes] = k * inodes_per_group + ((8 * i) + j + 1);
          fprintf(inode_csv, "%d,", inode_number[allocated_inodes]);

          //file type
          inode_file_type[allocated_inodes] = *(uint16_t *)inode; //printf("%x\n", inode_file_type);
          if ((inode_file_type[allocated_inodes] & 0x8000) && (inode_file_type[allocated_inodes] & 0x2000)) {
            fprintf(inode_csv, "s,");
          }
          else if (inode_file_type[allocated_inodes] & 0x8000) {
            fprintf(inode_csv, "f,");
          }
          else if (inode_file_type[allocated_inodes] & 0x4000) {
            fprintf(inode_csv, "d,");
            directories[directories_count] = allocated_inodes;
            directories_count++;
          }
          else {
            fprintf(inode_csv, "?,");
          }

          //mode
          fprintf(inode_csv, "%o,", inode_file_type[allocated_inodes]);

          //owner
          inode_owner[allocated_inodes] = *(uint16_t *)(inode + 2);
          fprintf(inode_csv, "%u,", inode_owner[allocated_inodes]);

          //group
          inode_group[allocated_inodes] = *(uint16_t *)(inode + 24);
          fprintf(inode_csv, "%u,", inode_group[allocated_inodes]);

          //link count
          inode_link_count[allocated_inodes] = *(uint16_t *)(inode + 26);
          fprintf(inode_csv, "%u,", inode_link_count[allocated_inodes]);

          //creation time
          inode_creation_time[allocated_inodes] = *(uint32_t *)(inode + 12);
          fprintf(inode_csv, "%x,", inode_creation_time[allocated_inodes]);

          //modification time
          inode_modification_time[allocated_inodes] = *(uint32_t *)(inode + 16);
          fprintf(inode_csv, "%x,", inode_modification_time[allocated_inodes]);

          //access time
          inode_access_time[allocated_inodes] = *(uint32_t *)(inode + 8);
          fprintf(inode_csv, "%x,", inode_access_time[allocated_inodes]);

          //file size
          inode_file_size[allocated_inodes] = *(uint32_t *)(inode + 4);
          fprintf(inode_csv, "%u,", inode_file_size[allocated_inodes]);

          //number of blocks
          inode_number_blocks[allocated_inodes] = *(uint32_t *)(inode + 28) / (2 << block_size);
          fprintf(inode_csv, "%u,", inode_number_blocks[allocated_inodes]);

          for (int l = 0; l < 15; l++) {
            inode_block_ptrs[allocated_inodes][l] = *(uint32_t *)(inode + 40 + (4 * l));
            if (inode_block_ptrs[allocated_inodes][l] != 0) {
              if (inode_block_ptrs[allocated_inodes][l] < first_data_block ||
                  inode_block_ptrs[allocated_inodes][l] > total_blocks) {
                    fprintf(stderr, "Inode %d - invalid block pointer[%d]: %u\n", inode_number[allocated_inodes], l, inode_block_ptrs[allocated_inodes][l]);
                  }
            }
            if (l != 14) {
              fprintf(inode_csv, "%x,", inode_block_ptrs[allocated_inodes][l]);
            }
            else {
              fprintf(inode_csv, "%x\n", inode_block_ptrs[allocated_inodes][l]);
            }
          }
          allocated_inodes++;
        }
      }
    }
  }

  //directory entry block_size
  //open directory csv
  void *directory;
  directory = (void *)malloc(block_size);
  if (directory == NULL) {
    fprintf(stderr, "Couldn't allocate memory for directory\n");
    return 1;
  }
  FILE *directory_csv;
  directory_csv = fopen("directory.csv", "w");
  if (directory_csv < 0) {
    fprintf(stderr, "Couldn't open directory.csv\n");
    return 1;
  }
  void *indirect;
  indirect = (void *)malloc(block_size);
  if (indirect == NULL) {
    fprintf(stderr, "Couldn't allocate memory for indirect\n");
    return 1;
  }
  void *indirect_2;
  indirect = (void *)malloc(block_size);
  if (indirect == NULL) {
    fprintf(stderr, "Couldn't allocate memory for indirect\n");
    return 1;
  }
  void *indirect_3;
  indirect = (void *)malloc(block_size);
  if (indirect == NULL) {
    fprintf(stderr, "Couldn't allocate memory for indirect\n");
    return 1;
  }
  FILE *indirect_csv;
  indirect_csv = fopen("indirect.csv", "w");
  if (indirect_csv < 0) {
    fprintf(stderr, "Couldn't open indirect.csv\n");
    return 1;
  }

  for (int h = 0; h < directories_count; h++) {
    int entry_number = 0;
    //iterate through block pointers
    for (int i = 0; i < 12; i++) {
      //check if block is in use
      if (inode_block_ptrs[directories[h]][i] != 0) {
        //iterate through block
        int block_iterator = 0;
        while (block_iterator < 1024) {
          //read into directory
          bytes_read = pread(disk_img, directory, block_size, inode_block_ptrs[directories[h]][i] * block_size + block_iterator);
          if (bytes_read < 0) {
            fprintf(stderr, "Couldn't read disk image for directory\n");
            return 1;
          }
          uint16_t entry_length = *(uint16_t *)(directory + 4);
          uint8_t name_length = *(uint8_t *)(directory + 6);
          uint32_t entry_inode = *(uint32_t *)(directory); //printf("entry inode: %d\n", entry_inode);
          char *name = (char *)malloc(name_length * sizeof(char));
          name = (directory + 8);
          name[name_length] = 0;
          if (entry_inode != 0) {
            if (entry_length < 8 || entry_length > 1024) {
              fprintf(stderr, "Inode %u, block %x - bad dirent: entrylen = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_length);
            }
            if (name_length > entry_length) {
              fprintf(stderr, "Inode %u, block %x - bad dirent: len = %u, namelen = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_length, name_length);
            }
            if (entry_inode < first_data_block || entry_inode > total_blocks) {
              fprintf(stderr, "Inode %u, block %x - bad dirent: Inode = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_inode);
            }
            fprintf(directory_csv, "%d,%d,%d,%d,%d,\"%s\"\n", inode_number[directories[h]],
            entry_number, entry_length, name_length, entry_inode, name);
          }
          entry_number++;
          block_iterator += entry_length; //printf("block iterator: %d\n", block_iterator);
        }
      }
    }

    if (inode_block_ptrs[directories[h]][12] != 0) {
      //read indirect block array
      //printf("indirect inode block pointer: %x\n", inode_block_ptrs[directories[h]][12]);
      bytes_read = pread(disk_img, directory, block_size, inode_block_ptrs[directories[h]][12] * block_size);
      if (bytes_read < 0) {
        fprintf(stderr, "Couldn't read disk image for directory\n");
        return 1;
      }

      int indirect_entry_number = 0;

      for (int i = 0; i < 256; i++) {
        //record indirect block pointers
        uint32_t indirect_block_ptr = *(uint32_t *)(directory + (4 * i)); //printf("indirect block number: %x\n", indirect_block_ptr);
        if (indirect_block_ptr != 0) {
          if (indirect_block_ptr < first_data_block || indirect_block_ptr > total_blocks) {
            fprintf(stderr, "Indirect block %x - invalid entry[%d] = %x\n", inode_block_ptrs[directories[h]][12],
            indirect_entry_number++, indirect_block_ptr);
          }
          //write to indirect
          fprintf(indirect_csv, "%x,%d,%x\n", inode_block_ptrs[directories[h]][12],
          indirect_entry_number++, indirect_block_ptr);

          //iterate through block
          int block_iterator = 0;
          while (block_iterator < 1024) {
            //read into indirect
            bytes_read = pread(disk_img, indirect, block_size, indirect_block_ptr * block_size + block_iterator);
            if (bytes_read < 0) {
              fprintf(stderr, "Couldn't read disk image for directory\n");
              return 1;
            }
            uint16_t entry_length = *(uint16_t *)(indirect + 4);
            uint8_t name_length = *(uint8_t *)(indirect + 6);
            uint32_t entry_inode = *(uint32_t *)(indirect); //printf("entry inode: %d\n", entry_inode);
            char *name = (char *)malloc(name_length * sizeof(char));
            name = (indirect + 8);
            name[name_length] = 0;
            if (entry_inode != 0) {
              fprintf(directory_csv, "%d,%d,%d,%d,%d,\"%s\"\n", inode_number[directories[h]],
              entry_number, entry_length, name_length, entry_inode, name);
            }
            entry_number++;
            block_iterator += entry_length; //printf("block iterator: %d\n", block_iterator);
          }
        }
      }
    }

    if (inode_block_ptrs[directories[h]][13] != 0) {
      //read indirect block array
      //printf("indirect inode block pointer: %x\n", inode_block_ptrs[directories[h]][12]);
      bytes_read = pread(disk_img, directory, block_size, inode_block_ptrs[directories[h]][12] * block_size);
      if (bytes_read < 0) {
        fprintf(stderr, "Couldn't read disk image for directory\n");
        return 1;
      }

      int indirect_entry_number = 0;

      for (int i = 0; i < 256; i++) {
        //record indirect block pointers
        uint32_t indirect_block_ptr = *(uint32_t *)(directory + (4 * i)); //printf("indirect block number: %x\n", indirect_block_ptr);
        if (indirect_block_ptr != 0) {
          if (indirect_block_ptr < first_data_block || indirect_block_ptr > total_blocks) {
            fprintf(stderr, "Indirect block %x - invalid entry[%d] = %x\n", inode_block_ptrs[directories[h]][12],
            indirect_entry_number++, indirect_block_ptr);
          }
          //write to indirect
          fprintf(indirect_csv, "%x,%d,%x\n", inode_block_ptrs[directories[h]][12],
          indirect_entry_number++, indirect_block_ptr);

          //read indirect block array
          //printf("indirect inode block pointer: %x\n", inode_block_ptrs[directories[h]][12]);
          bytes_read = pread(disk_img, indirect, block_size, indirect_block_ptr * block_size);
          if (bytes_read < 0) {
            fprintf(stderr, "Couldn't read disk image for directory\n");
            return 1;
          }

          int indirect_entry_number_2 = 0;

          for (int j = 0; j < 256; j++) {
            //record indirect block pointers
            uint32_t indirect_block_ptr_2 = *(uint32_t *)(indirect + (4 * j)); //printf("indirect block number: %x\n", indirect_block_ptr);
            if (indirect_block_ptr_2 != 0) {
              if (indirect_block_ptr_2 < first_data_block || indirect_block_ptr_2 > total_blocks) {
                fprintf(stderr, "Indirect block %x - invalid entry[%d] = %x\n", indirect_block_ptr,
                indirect_entry_number_2++, indirect_block_ptr_2);
              }
              //write to indirect
              fprintf(indirect_csv, "%x,%d,%x\n", indirect_block_ptr,
              indirect_entry_number_2++, indirect_block_ptr_2);

              //iterate through block
              int block_iterator = 0;
              while (block_iterator < 1024) {
                //read into indirect
                bytes_read = pread(disk_img, indirect_2, block_size, indirect_block_ptr_2 * block_size + block_iterator);
                if (bytes_read < 0) {
                  fprintf(stderr, "Couldn't read disk image for directory\n");
                  return 1;
                }
                uint16_t entry_length = *(uint16_t *)(indirect_2 + 4);
                uint8_t name_length = *(uint8_t *)(indirect_2 + 6);
                uint32_t entry_inode = *(uint32_t *)(indirect_2); //printf("entry inode: %d\n", entry_inode);
                char *name = (char *)malloc(name_length * sizeof(char));
                name = (indirect_2 + 8);
                name[name_length] = 0;
                if (entry_inode != 0) {
                  if (entry_length < 8 || entry_length > 1024) {
                    fprintf(stderr, "Inode %u, block %x - bad dirent: entrylen = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_length);
                  }
                  if (name_length > entry_length) {
                    fprintf(stderr, "Inode %u, block %x - bad dirent: len = %u, namelen = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_length, name_length);
                  }
                  if (entry_inode < first_data_block || entry_inode > total_blocks) {
                    fprintf(stderr, "Inode %u, block %x - bad dirent: Inode = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_inode);
                  }
                  fprintf(directory_csv, "%d,%d,%d,%d,%d,\"%s\"\n", inode_number[directories[h]],
                  entry_number, entry_length, name_length, entry_inode, name);
                }
                entry_number++;
                block_iterator += entry_length; //printf("block iterator: %d\n", block_iterator);
              }
            }
          }
        }
      }
    }

    if (inode_block_ptrs[directories[h]][14] != 0) {
      //read indirect block array
      //printf("indirect inode block pointer: %x\n", inode_block_ptrs[directories[h]][12]);
      bytes_read = pread(disk_img, directory, block_size, inode_block_ptrs[directories[h]][12] * block_size);
      if (bytes_read < 0) {
        fprintf(stderr, "Couldn't read disk image for directory\n");
        return 1;
      }

      int indirect_entry_number = 0;

      for (int i = 0; i < 256; i++) {
        //record indirect block pointers
        uint32_t indirect_block_ptr = *(uint32_t *)(directory + (4 * i)); //printf("indirect block number: %x\n", indirect_block_ptr);
        if (indirect_block_ptr != 0) {
          if (indirect_block_ptr < first_data_block || indirect_block_ptr > total_blocks) {
            fprintf(stderr, "Indirect block %x - invalid entry[%d] = %x\n", inode_block_ptrs[directories[h]][12],
            indirect_entry_number++, indirect_block_ptr);
          }
          //write to indirect
          fprintf(indirect_csv, "%x,%d,%x\n", inode_block_ptrs[directories[h]][12],
          indirect_entry_number++, indirect_block_ptr);

          //read indirect block array
          //printf("indirect inode block pointer: %x\n", inode_block_ptrs[directories[h]][12]);
          bytes_read = pread(disk_img, indirect, block_size, indirect_block_ptr * block_size);
          if (bytes_read < 0) {
            fprintf(stderr, "Couldn't read disk image for directory\n");
            return 1;
          }

          int indirect_entry_number_2 = 0;

          for (int j = 0; j < 256; j++) {
            //record indirect block pointers
            uint32_t indirect_block_ptr_2 = *(uint32_t *)(indirect + (4 * j)); //printf("indirect block number: %x\n", indirect_block_ptr);
            if (indirect_block_ptr_2 != 0) {
              if (indirect_block_ptr_2 < first_data_block || indirect_block_ptr_2 > total_blocks) {
                fprintf(stderr, "Indirect block %x - invalid entry[%d] = %x\n", indirect_block_ptr,
                indirect_entry_number_2++, indirect_block_ptr_2);
              }
              //write to indirect
              fprintf(indirect_csv, "%x,%d,%x\n", indirect_block_ptr,
              indirect_entry_number_2++, indirect_block_ptr_2);
              //read indirect block array
              //printf("indirect inode block pointer: %x\n", inode_block_ptrs[directories[h]][12]);
              bytes_read = pread(disk_img, indirect_2, block_size, indirect_block_ptr_2 * block_size);
              if (bytes_read < 0) {
                fprintf(stderr, "Couldn't read disk image for directory\n");
                return 1;
              }

              int indirect_entry_number_3 = 0;

              for (int k = 0; k < 256; k++) {
                //record indirect block pointers
                uint32_t indirect_block_ptr_3 = *(uint32_t *)(indirect_2 + (4 * j)); //printf("indirect block number: %x\n", indirect_block_ptr);
                if (indirect_block_ptr_3 != 0) {
                  if (indirect_block_ptr_3 < first_data_block || indirect_block_ptr_3 > total_blocks) {
                    fprintf(stderr, "Indirect block %x - invalid entry[%d] = %x\n", indirect_block_ptr_3,
                    indirect_entry_number_3++, indirect_block_ptr_3);
                  }
                  //write to indirect
                  fprintf(indirect_csv, "%x,%d,%x\n", indirect_block_ptr_3,
                  indirect_entry_number_3++, indirect_block_ptr_3);
                  //iterate through block
                  int block_iterator = 0;
                  while (block_iterator < 1024) {
                    //read into indirect
                    bytes_read = pread(disk_img, indirect_3, block_size, indirect_block_ptr_3 * block_size + block_iterator);
                    if (bytes_read < 0) {
                      fprintf(stderr, "Couldn't read disk image for directory\n");
                      return 1;
                    }
                    uint16_t entry_length = *(uint16_t *)(indirect_3 + 4);
                    uint8_t name_length = *(uint8_t *)(indirect_3 + 6);
                    uint32_t entry_inode = *(uint32_t *)(indirect_3); //printf("entry inode: %d\n", entry_inode);
                    char *name = (char *)malloc(name_length * sizeof(char));
                    name = (indirect_3 + 8);
                    name[name_length] = 0;
                    if (entry_inode != 0) {
                      if (entry_length < 8 || entry_length > 1024) {
                        fprintf(stderr, "Inode %u, block %x - bad dirent: entrylen = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_length);
                      }
                      if (name_length > entry_length) {
                        fprintf(stderr, "Inode %u, block %x - bad dirent: len = %u, namelen = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_length, name_length);
                      }
                      if (entry_inode < first_data_block || entry_inode > total_blocks) {
                        fprintf(stderr, "Inode %u, block %x - bad dirent: Inode = %u\n", entry_inode, inode_block_ptrs[directories[h]][i], entry_inode);
                      }
                      fprintf(directory_csv, "%d,%d,%d,%d,%d,\"%s\"\n", inode_number[directories[h]],
                      entry_number, entry_length, name_length, entry_inode, name);
                    }
                    entry_number++;
                    block_iterator += entry_length; //printf("block iterator: %d\n", block_iterator);
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return 0;
}

int powerOfTwoTest(unsigned int x) {
  while (((x % 2) == 0) && x > 1) x/= 2;
  return (x == 1);
}
