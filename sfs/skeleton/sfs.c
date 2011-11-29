/* -*-C-*-
 *******************************************************************************
 *
 * File:         sfs.c
 * RCS:          $Id: sfs.c,v 1.2 2009/11/10 21:17:25 npb853 Exp $
 * Description:  Simple File System
 * Author:       Fabian E. Bustamante
 *               Northwestern Systems Research Group
 *               Department of Computer Science
 *               Northwestern University
 * Created:      Tue Nov 05, 2002 at 07:40:42
 * Modified:     Fri Nov 19, 2004 at 15:45:57 fabianb@cs.northwestern.edu
 * Language:     C
 * Package:      N/A
 * Status:       Experimental (Do Not Distribute)
 *
 * (C) Copyright 2003, Northwestern University, all rights reserved.
 *
 *******************************************************************************
 */

#include <string.h>

#include "sfs.h"
#include "sdisk.h"


#define INODES_IN_SECTOR 16
#define BITMAP_SIZE 250
#define MAX_FILES 900

static open_file* file_table[MAX_FILES];
static int cwd_index;
const int DENTRIES_PER_BLOCK = SD_SECTORSIZE / sizeof(dentry);


int dirrific(inode* parent_node, char* name);
int
write_ls(FILE* f, inode* parent);
int
next_index(void);
void
free_file(open_file* f);
int
flip_bm(int sector);
int
safe_write(int sector, void* buf);
int
safe_read(int sector, void* buf);
int
write_inode(int index, void* buf);
int
read_inode(int index, void* buf);
int
write_to_offset(int sector, int offset, void* buf, int buf_size);
inode*
pop_free_inode(void);
int
resolve_path(char* path, void* inode_buf);
int
inode_index(char* name, inode* parent);
int
sector_for_block(int block_index, inode* node);
int
add_dentry(inode* parent, inode* child, char* name);


/*
 * Calls SD_write until SD_write doesn't fail
 * because of the random errors.
 */
int
safe_write(int sector, void* buf)
{
    int success = SD_write(sector, buf);
    if (success == -1 && sderrno == E_WRITING_FILE)
        success = safe_write(sector, buf);
    return success;
}

/*
 * Calls SD_read until SD_read doesn't fail
 * because of the random errors.
 */
int
safe_read(int sector, void* buf)
{
    int success = SD_read(sector, buf);
    if (success == -1 && sderrno == E_READING_FILE)
        success = safe_read(sector, buf);
    return success;
}


int
write_to_offset(int sector, int offset, void* buf, int buf_size)
{
    void* old_buf = malloc(SD_SECTORSIZE);
    int success = safe_read(sector, old_buf);
    if(success == -1)
    {
        free(old_buf);
        return -1;
    }
    memcpy(old_buf+offset, buf, buf_size);
    success = safe_write(sector, old_buf);
    free(old_buf);
    return success;
}

int
write_inode(int index, void* buf)
{
    int byte_num = index * sizeof(inode);
    int offset = byte_num % SD_SECTORSIZE;
    int sector = (byte_num / SD_SECTORSIZE) + 1;
    return write_to_offset(sector, offset, buf, sizeof(inode));
}

int
read_inode(int index, void* buf)
{
    int byte_num = index * sizeof(inode);
    int offset = byte_num % SD_SECTORSIZE;
    int sector = (byte_num / SD_SECTORSIZE) + 1;

    void* inode_sector = malloc(SD_SECTORSIZE);
    int succcess = safe_read(sector, inode_sector);
    memcpy(buf, (inode_sector+offset), sizeof(inode));
    free(inode_sector);
    return succcess;
}

/*
 * returns 0 unless error
 * NOTE: this assumes everything works perfectly
 */
int
flip_bm(int sector)
{
    unsigned char value = 1;
    int sector_bit = sector %  8; //bit's offset in the byte of bitmap
    value = value << sector_bit; //create  a mask with a 1 at that offset
    int sector_offset = sector / 8; //byte's offset in bitmap

    void* bm = malloc(SD_SECTORSIZE);
    int success = safe_read(0, bm);

    if(success == -1)
        return -1;

    char* temp_bm = (char*)bm;
    char bm_char = temp_bm[8 + sector_offset];
    char flipped_bit =  bm_char ^ value;
    write_to_offset(0, 8+sector_offset, (void*)(&flipped_bit), 1);
    free(bm);
    return 0;
}


void
push_free_inode(inode* free_inode)
{
    void* superblock = malloc(SD_SECTORSIZE); //gets superblock
    safe_read(0, superblock);
    int free_inode_num = *((int*)superblock);
    free(superblock);

    // advance free inode list
    int free_list_buf = free_inode->next_inode_num;
    write_to_offset(0, 0, (void*)&free_list_buf, sizeof(int));

    free_inode->next_inode_num = free_inode_num;

    write_inode(free_inode_num, (void*)free_inode);
    free(free_inode);
}

inode*
pop_free_inode(void)
{
    void* superblock = malloc(SD_SECTORSIZE);
    safe_read(0, superblock);
    int free_inode_num = *((int*)superblock);
    free(superblock);

    if(free_inode_num == -1)
        return NULL; // out of inodes

    inode* free_inode = malloc(sizeof(inode));
    read_inode(free_inode_num, free_inode);

    // advance free inode list
    write_to_offset(0, 0, (void*)&free_inode->next_inode_num, sizeof(int));

    // now set the inode's next inode pointers to point to its own address on
    // disk
    free_inode->next_inode_num = free_inode_num; // NEED TO FREE INODES AFTER WE PUT IN USED INODE TABLE -- WE DO IT IN PUSH
    return free_inode;
}

/*
 * writes address of free sector into sector_addr
 * returns 0 on success, -1 otherwise
 *
 */
int
get_block(int* sector_addr)
{
    void* bm_sector = malloc(SD_SECTORSIZE);
    int success = safe_read(0, bm_sector);
    if(success)
    {
        free(bm_sector);
        return -1;
    }
    char *bitmap = (char *)(bm_sector + 8);
    int i;
    char byte;
    for(i = 0; i < BITMAP_SIZE; i++)
    {
        if(bitmap[i] != 255)
        {
            byte = bitmap[i];
            break;
        }
        if(i == (BITMAP_SIZE - 1))
        {
            free(bm_sector);
            return -1;
        }
    }
    int j;
    for(j = 0; j < 8; j++)
    {
        if(byte & 1)
            byte >>= 1;
        else
            break;
    }

    *sector_addr =  i*8+j;
    flip_bm(*sector_addr);
    free(bm_sector);
    int* zero_buf = malloc(SD_SECTORSIZE);
    memset(zero_buf, 0, SD_SECTORSIZE);
    safe_write(*sector_addr, zero_buf);
    free(zero_buf);
    return 0;
}

int
init_dir(int is_root)
{
    // pop free inode off list
    inode* node = pop_free_inode();
    if(is_root == 1)
    {
        cwd_index = 0;
        open_file* f = malloc(sizeof(open_file));
        f->node = node;
        f->rw_ptr = 0;
        file_table[cwd_index] = f;
    }
    // allocate it one direct block
    int block_addr;
    get_block(&block_addr);
    node->direct[0] = block_addr;
    // put two dentries in the direct block
    // first dentry has . (self pointer)
    // second dentry has .. (parent pointer, which is self for root)
    // inode->next_inode_num refers to self for in use inodes
    dentry* dentries = (dentry*)malloc(2*sizeof(dentry));
    strcpy(dentries[0].f_name, ".");
    dentries[0].inode_num = node->next_inode_num;
    strcpy(dentries[1].f_name, "..");
    dentries[1].inode_num = file_table[cwd_index]->node->next_inode_num;
    // set size_count negative to indicate directory inode
    node->size_count = -2;
    write_inode(node->next_inode_num, (void*)node);
    int success = write_to_offset(node->direct[0], 0, dentries, (2*sizeof(dentry)));
    if(success == -1)
    {
        free(node);
        free(dentries);
        return success;
    }
    int node_index = node->next_inode_num;
    if(is_root != 1)
        free(node);
    free(dentries);
    return node_index;
}


void
free_file(open_file* f)
{
    push_free_inode(f->node);
    free(f);
}


/*
 * sfs_mkfs: use to build your filesystem
 *
 * Parameters: -
 *
 * Returns: 0 on success, or -1 if an error occurred
 *
 */
int sfs_mkfs() {
    if(SD_initDisk() == -1)
        return -1;
    // Superblock
    int* buf = (int*)malloc(SD_SECTORSIZE);
    memset((void*)buf, 0, SD_SECTORSIZE);
    // inode free list points to the second inode in our inode index
    buf[0] = 0; // sector 1
    safe_write(0, (void*)buf);
    free(buf);
    // mark pool of inodes (57 blocks) and super block as not free in bitmap
    int success;
    int i = 0;
    for(i=0; i<58; i++)
    {
        success = flip_bm(i);
        if(success == -1)
            printf("BAD FLIP\n");
    }
    // allocate inodes
    void* inode_buf;
    inode* node;
    for(i = 1; i<58; i++)
    {
        inode_buf = malloc(SD_SECTORSIZE);
        int j = 0;
        for(j=0; j < INODES_IN_SECTOR; j++)
        {
            node = (inode*)(inode_buf + (j*sizeof(inode)));
            node->size_count = 0;
            if (((i+1) > 57) && ((j+1) > INODES_IN_SECTOR))
            {
                node->next_inode_num = -1;
            } else
            {
                node->next_inode_num = (((i - 1) * INODES_IN_SECTOR) + (j + 1));
            }
        }
        int success = safe_write(i, inode_buf);
        if(success == -1)
            printf("WRITE ERROR\n");
        free(inode_buf);
    }
    // make root dir
    int is_root = 1;
    success =  init_dir(is_root);
    if(success != -1)
        return 0;
    return -1;
} /* !sfs_mkfs */

/*
 * sfs_mkdir: attempts to create the name directory
 *
 * Parameters: directory name
 *
 * Returns: 0 on success, or -1 if an error occurred
 *
 */
int sfs_mkdir(char *name)
{
    int name_len = strlen(name);
    char* parent_path = malloc(name_len);
    if(name[0] == '/')
        parent_path[0] = '/';
    char* copy_path = memcpy(malloc(name_len), name, name_len); // don't mangle input
    char* cur_seg = strtok(copy_path, "/");
    inode* parent_node = malloc(sizeof(inode));
    strcat(parent_path, cur_seg);
    while(resolve_path(parent_path, parent_node) >= 0)
    {
        cur_seg = strtok(NULL, "/");
        if(cur_seg == NULL)
        {
            // base case, made it all the way through name
            free(parent_path);
            free(copy_path);
            free(parent_node);
            return 0;
        }
        strcat(parent_path, cur_seg);
    }
    int success = dirrific(parent_node, cur_seg);
    if(success == -1)
    {
        free(parent_path);
        free(copy_path);
        free(parent_node);
        return -1;
    }
    free(parent_node);
    free(copy_path);
    free(parent_path);
    return sfs_mkdir(name);
} /* !sfs_mkdir */


/*
 * Adds a dentry to parent node for directory with the name
 */
int dirrific(inode* parent_node, char* name)
{
    int node_num =  init_dir(0);
    if(node_num == -1)
        return -1;
    inode* node = malloc(sizeof(inode));
    int success = read_inode(node_num, node);
    if(success == -1)
    {
        free(node);
        return -1;
    }
    success = add_dentry(parent_node, node, name);
    if(success == -1)
    {
        free(node);
        return -1;
    }
    return 0;
}







/*
 * sfs_fcd: attempts to change current directory to named directory
 *
 * Parameters: new directory name
 *
 * Returns: 0 on success, or -1 if an error occurred
 *
 */
int sfs_fcd(char* name) {
    /* Either change cwd_index pointer into open file table
     * or (if we use this representation) set the 0th entry to the file table
     * to point to the dir indicated by name
     */
    // TODO: Implement
    return -1;
} /* !sfs_fcd */

/*
 * sfs_ls: output the information of all existing files in
 *   current directory
 *
 * Parameters: -
 *
 * Returns: 0 on success, or -1 if an error occurred
 *
 */
int sfs_ls(FILE* f) {
    inode* cwd_node = file_table[cwd_index]->node;
    return write_ls(f, cwd_node);
} /* !sfs_ls */



/*
 * Writes dentry names of parent to file ptr
 */
int
write_ls(FILE* f, inode* parent)
{
    int max_dentries = -1 * parent->size_count;
    int num_dentries = 0;
    int loop_limit;
    int block_index = -1;
    // check dentries in direct inode blocks
    while(num_dentries <= max_dentries)
    {
        block_index += 1;
        int diff = max_dentries - num_dentries;
        if(diff > DENTRIES_PER_BLOCK)
            loop_limit = DENTRIES_PER_BLOCK;
        else
            loop_limit = diff;

        num_dentries += loop_limit;

        int sector_num = sector_for_block(block_index, parent);
        void* sector_buf = malloc(SD_SECTORSIZE);
        int success = safe_read(sector_num, sector_buf);
        if(success == -1)
        {
            free(sector_buf);
            return -1;
        }
        dentry* dentries = (dentry*)sector_buf;
        int i = 0;
        for(i=0; i < loop_limit; i++)
        {

            if(strcmp(dentries[i].f_name, ".") == 0 || \
                    strcmp(dentries[i].f_name, "..") == 0)
                continue;
            fprintf(f, "%s\n", dentries[i].f_name);
            free(sector_buf);
        }
    }
    return 0;
}

/*
 * sfs_fopen: convert a pathname into a file descriptor. When the call
 *   is successful, the file descriptor returned will be the lowest file
 *   descriptor not currently open for the process. If the file does not
 *   exist it will be created.
 *
 * Parameters: file name
 *
 * Returns:  return the new file descriptor, or -1 if an error occurred
 *
 */
int sfs_fopen(char* name) {
    int success;
    inode* node = malloc(sizeof(inode));
    success = resolve_path(name, node);
    if(success == -1)
    {
        free(node);
        return success;
    } else if(success == -2)
    {
        // file doesn't exist yet -- make it
        free(node);
        // get path to parent dir to update it's dentries with new file
        int name_len = strlen(name);
        char* copy_path = memcpy(malloc(name_len), name, name_len); // don't mangle input
        char* cur_seg = strtok(copy_path, "/");
        char* prev_seg = malloc(name_len);
        char* parent_path = malloc(name_len);
        while(cur_seg != NULL)
        {
            strcpy(prev_seg,cur_seg);
            cur_seg = strtok(NULL, "/");
            if(cur_seg != NULL)
                strcat(parent_path, cur_seg);
        }
        free(copy_path);
        inode* parent_node = malloc(sizeof(inode));
        int success = resolve_path(parent_path, parent_node);
        if (success == -1)
        {
            free(parent_node);
            free(prev_seg);
            free(parent_path);
            return -1;
        }
        else if(success == -2)
        {
            success = sfs_mkdir(parent_path);
            if(success == -1)
            {
                free(parent_node);
                free(prev_seg);
                free(parent_path);
                return -1;
            }
            success = resolve_path(parent_path, parent_node);
            if(success == -1)
            {
                free(parent_node);
                free(prev_seg);
                free(parent_path);
                return -1;
            }
        }


        node = pop_free_inode();
        success = add_dentry(parent_node, node, prev_seg);
        free(prev_seg);
        free(parent_path);
        free(parent_node);
        if(success == -1)
            return -1;
    }
    open_file* f = malloc(sizeof(open_file));
    f->rw_ptr = 0;
    f->node = node;
    int index = next_index();
    file_table[index] = f;
    return index;
} /* !sfs_fopen */

int
add_dentry(inode* parent, inode* child, char* name)
{
    int dentries_in_use = parent->size_count * -1;
    printf("%d\n", dentries_in_use);
    int block_addr;
    int success;
    // Make sure there's room in the current block
    if(dentries_in_use % DENTRIES_PER_BLOCK == 0)
    {
        success = get_block(&block_addr);
        if(success == -1)
            return -1;
        int new_block_index = dentries_in_use / DENTRIES_PER_BLOCK;
        if(new_block_index < DIRECT_BLOCKS)
        {
            parent->direct[new_block_index] = block_addr;
        } else if(new_block_index < (DIRECT_BLOCKS + DENTRIES_PER_BLOCK))
        {
            int single_indirect_block;
            if(new_block_index == DIRECT_BLOCKS)
            {
                success = get_block(&single_indirect_block);
                if(success == -1)
                    return -1;
                parent->single_indirect = single_indirect_block;
            }
            write_to_offset(parent->single_indirect, \
                    ((new_block_index - DIRECT_BLOCKS) * sizeof(int)), \
                    &block_addr, sizeof(int));
        } else
        {
            // in addition to checking whether the single indirect is a new
            // page we need to check whether the double indirect is a new page
            int single_indirect_block;
            int double_indirect_block;
            if(new_block_index == DIRECT_BLOCKS + DENTRIES_PER_BLOCK)
            {
                success = get_block(&double_indirect_block);
                if(success == -1)
                    return -1;
                parent->double_indirect = double_indirect_block;
            }


            int double_ind_offset = (new_block_index - \
                    (DIRECT_BLOCKS + DENTRIES_PER_BLOCK)) / DENTRIES_PER_BLOCK;
            int single_ind_offset = (new_block_index - \
                    (DIRECT_BLOCKS + DENTRIES_PER_BLOCK)) % DENTRIES_PER_BLOCK;

            if(((new_block_index - DIRECT_BLOCKS) % DENTRIES_PER_BLOCK) == 0)
            {
               success = get_block(&single_indirect_block);
               if(success == -1)
                   return -1;
               write_to_offset(parent->double_indirect, double_ind_offset * sizeof(int), \
                       &single_indirect_block, sizeof(int));
            } else
            {//find single_indirect_block
                void *buf = malloc(SD_SECTORSIZE);
                success = safe_read(parent->double_indirect, buf);
                if(success == -1)
                {
                    free(buf);
                    return -1;
                }
                int* double_ind_sector = (int *)buf;
                single_indirect_block = double_ind_sector[double_ind_offset];
            }
            write_to_offset(single_indirect_block, \
                    single_ind_offset*sizeof(int), &block_addr, sizeof(int));
            // add a new indirect block, zero it out and add a dentry to it
        }

    }
    else
    {
        // If there's room then get cur block from existing func
        // and loop through it until you find room to add a dentry
        int block_index = dentries_in_use / DENTRIES_PER_BLOCK;
        block_addr = sector_for_block(block_index, parent);
    }
    void *buf = malloc(SD_SECTORSIZE);
    success = safe_read(block_addr, buf);
    if(success == -1)
    {
        free(buf);
        return -1;
    }

    parent->size_count--;
    write_inode(parent->next_inode_num, (void*)parent);

    dentry* dentry_list = (dentry *)buf;
    int i = 0;
    while(dentry_list[i].f_name[0] != '\0')
    {
        i++;
    }
    dentry_list[i].inode_num = child->next_inode_num;
    strcpy(dentry_list[i].f_name, name);
    safe_write(block_addr, buf);
    return 0;
}


/*
 * Returns next available index into the file_table
 */
int
next_index(void)
{
    int i = 0;
    for(i=0; ((i<MAX_FILES) && (file_table[i] != NULL)); i++)
        continue;
    if(i+1 == MAX_FILES)
        printf("Out of files!");
    return i;
}




//TODO: CHECK BEHAVIOR EMPTY PATH
/*
 * Reads the inode at path into inode_buf
 *
 * Returns o on success, -1 on failure or -2 if file doesn't exist
 */
int
resolve_path(char* path, void* inode_buf)
{
    if(strcmp(path,"") == 0)
    {
        memcpy(inode_buf,(void*)file_table[cwd_index]->node,sizeof(inode));
        return 0;
    }
    int success;
    inode* current = (inode*)malloc(sizeof(inode));
    if(path[0] == '/')
    {
        success = read_inode(0, current);
        if(success == -1)
        {
            free(current);
            return -1;
        }
    }else
    {
        memcpy(current, file_table[cwd_index]->node, sizeof(inode));
    }

    int path_len = strlen(path);
    char* copy_path = memcpy(malloc(path_len), path, path_len); // don't mangle input
    char* cur_seg = strtok(copy_path, "/");
    int next_inode_addr;
    while(cur_seg != NULL)
    {
        next_inode_addr = inode_index(cur_seg, current);
        if(next_inode_addr == -1)
        {
            free(current);
            return -2;
        }
        success = read_inode(next_inode_addr, current);
        if(success == -1)
        {
            free(current);
            return -1;
        }
        cur_seg = strtok(NULL, "/");
    }
    memcpy(inode_buf, current, sizeof(inode));
    free(current);
    return 0;
}

/*
 * Returns address of name if it's a dentry of parent
 */
int
inode_index(char* name, inode* parent)
{
    int num_dentries = -1 * parent->size_count;
    int loop_limit;
    int block_index = -1;
    // check dentries in direct inode blocks
    while(num_dentries > 0)
    {
        block_index += 1;
        if(num_dentries > DENTRIES_PER_BLOCK)
            loop_limit = DENTRIES_PER_BLOCK;
        else
            loop_limit = num_dentries;
        num_dentries -= loop_limit;

        int sector_num = sector_for_block(block_index, parent);
        void* sector_buf = malloc(SD_SECTORSIZE);
        int success = safe_read(sector_num, sector_buf);
        if(success == -1)
        {
            free(sector_buf);
            return -1;
        }
        dentry* dentries = (dentry*)sector_buf;
        int i = 0;
        for(i=0; i < loop_limit; i++)
        {
            if(strcmp(name, dentries[i].f_name) == 0)
            {
                int found = dentries[i].inode_num;
                free(sector_buf);
                return found;
            }
        }
    }
    return -1;
}


/*
 * block_index is which block of dentries we are looking at, ignoring the
 * indirection
 *
 * returns the actual address of the block_index
 */
int
sector_for_block(int block_index, inode* node)
{
    int found;
    if(block_index < DIRECT_BLOCKS)
        return node->direct[block_index];
    else if(block_index < (DIRECT_BLOCKS + DENTRIES_PER_BLOCK))
    {
        void* buf = malloc(SD_SECTORSIZE);
        int success = safe_read(node->single_indirect, buf);
        if(success == -1)
        {
            free(buf);
            return -1;
        }
        int* indirect_addresses = (int*)buf;
        found = indirect_addresses[block_index - 4];
        free(indirect_addresses);
        return found;
    }
    else
    {
        void* buf = malloc(SD_SECTORSIZE);
        int success = safe_read(node->double_indirect, buf);
        if(success == -1)
        {
            free(buf);
            return -1;
        }
        int* double_indirect_blocks = (int*)buf;
        int translated_index = block_index - 132;
        int pointers_per_block = SD_SECTORSIZE / sizeof(int);
        int first_index =  translated_index / pointers_per_block;
        int second_block_table = double_indirect_blocks[first_index];
        success = safe_read(second_block_table, buf);
        if(success == -1)
        {
            free(buf);
            return -1;
        }
        int* double_indirect_addresses = (int*)buf;
        int second_index = translated_index % pointers_per_block;
        found = double_indirect_addresses[second_index];
        free(buf);
        return found;
    }
    return -1;
}


/*
 * sfs_fclose: close closes a file descriptor, so that it no longer
 *   refers to any file and may be reused.
 *
 * Parameters: -
 *
 * Returns: 0 on success, or -1 if an error occurred
 *
 */
int sfs_fclose(int fileID) {
    open_file* f = file_table[fileID];
    free_file(f);
    file_table[fileID] = NULL;
    return -1;
} /* !sfs_fclose */

/*
 * sfs_fread: attempts to read up to length bytes from file
 *   descriptor fileID into the buffer starting at buffer
 *
 * Parameters: file descriptor, buffer to read and its lenght
 *
 * Returns: on success, the number of bytes read are returned. On
 *   error, -1 is returned
 *
 */
int sfs_fread(int fileID, char *buffer, int length) {
    return -1;
}

/*
 * sfs_fwrite: writes up to length bytes to the file referenced by
 *   fileID from the buffer starting at buffer
 *
 * Parameters: file descriptor, buffer to write and its lenght
 *
 * Returns: on success, the number of bytes written are returned. On
 *   error, -1 is returned
 *
 */
int sfs_fwrite(int fileID, char *buffer, int length) {
    // TODO: Implement
    return -1;
} /* !sfs_fwrite */

/*
 * sfs_lseek: reposition the offset of the file descriptor
 *   fileID to position
 *
 * Parameters: file descriptor and new position
 *
 * Returns: Upon successful completion, lseek returns the resulting
 *   offset location, otherwise the value -1 is returned
 *
 */
int sfs_lseek(int fileID, int position) {
    // TODO: Implement
    return -1;
} /* !sfs_lseek */

/*
 * sfs_rm: removes a file in the current directory by name if it exists.
 *
 * Parameters: file name
 *
 * Returns: 0 on success, or -1 if an error occurred
 */
int sfs_rm(char *file_name) {
    // TODO: Implement for extra credit
    return -1;
} /* !sfs_rm */
