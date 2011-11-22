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


int
flip_bm(int sector);
int
safe_write(int sector, void* buf);
int
safe_read(int sector, void* buf);
int
write_to_offset(int sector, int offset, void* buf, int buf_size);
inode*
pop_free_inodes(void)

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

inode*
pop_free_inodes(void)
{
    void* inode_list_sector = malloc(SD_SECTORSIZE);
    safe_read(0, inode_list_sector);
    int free_inode_sector_addr = (int)inode_list_sector;
    int free_inode_offset_addr = (int)(inode_list_sector + sizeof(int));
    // Advance this and write it back, check if last inode while advancing
    // After advancing inode to place indicated by free_inode set free_inode's
    // inode_sector and inode_offset to -1

    void* free_inode_sector = malloc(SD_SECTORSIZE);
    safe_read(free_inode_sector_addr, free_inode_sector);
    inode free_inode = malloc(sizeof(inode));
    memcpy();
    // copy offset deep from free_inode_sector into free_inode

    // NEED TO FREE INODES AFTER WE PUT IN USED INODE TABLE

    // return address of free_inode
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
    // set the two integers that represent a pointer to the free list
    buf[0] = 1; // sector 1
    buf[1] = sizeof(inode); // offset is sizeof(inode) to get past the root inode
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
            if((j+1) < INODES_IN_SECTOR)
            {
                node->inode_sector = i;
                node->inode_offset = (j+1)*sizeof(inode);
            } else
            {
                if((i+1) < 57)
                {
                    node->inode_sector = (i+1);
                    node->inode_offset = 0;
                }
                else
                {
                    node->inode_sector = -1;
                    node->inode_offset = -1;
                }
            }
        }
        int success = safe_write(i, inode_buf);
        if(success == -1)
            printf("WRITE ERROR\n");
        free(inode_buf);
    }
    // pop free inode off list
    // allocate it two direct blocks
    // put two dentries in the direct blocks
    // first dentry has . (self pointer)
    // second dentry has .. (parent pointer, which is self for root)
    return 0;
} /* !sfs_mkfs */

/*
 * sfs_mkdir: attempts to create the name directory
 *
 * Parameters: directory name
 *
 * Returns: 0 on success, or -1 if an error occurred
 *
 */
int sfs_mkdir(char *name) {
    // TODO: Implement
    return -1;
} /* !sfs_mkdir */

/*
 * sfs_fcd: attempts to change current directory to named directory
 *
 * Parameters: new directory name
 *
 * Returns: 0 on success, or -1 if an error occurred
 *
 */
int sfs_fcd(char* name) {
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
    // TODO: Implement
    return -1;
} /* !sfs_ls */

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
    // TODO: Implement
    return -1;
} /* !sfs_fopen */

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
    // TODO: Implement
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
    // TODO: Implement
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
