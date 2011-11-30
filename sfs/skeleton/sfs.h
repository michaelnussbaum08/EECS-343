/* -*-C-*-
 *******************************************************************************
 *
 * File:         sfs.h
 * RCS:          $Id: sfs.h,v 1.3 2009/11/12 09:19:51 jot836 Exp $
 * Description:  Simple File System Interface
 * Author:       Fabian E. Bustamante
 *               Northwestern Systems Research Group
 *               Department of Computer Science
 *               Northwestern University
 * Created:      Tue Nov 05, 2002 at 07:40:42
 * Modified:     Fri Nov 19, 2004 at 15:45:16 fabianb@cs.northwestern.edu
 * Language:     C
 * Package:      N/A
 * Status:       Experimental (Do Not Distribute)
 *
 * (C) Copyright 2003, Northwestern University, all rights reserved.
 *
 *******************************************************************************
 */

#ifndef SFS_H
#define SFS_H

#include "stdio.h"

#define DIRECT_BLOCKS 3
#define INDIRECT_BLOCK_SIZE 128

extern int sfs_mkfs();
extern int sfs_mkdir(char *name);
extern int sfs_fcd(char* name);
extern int sfs_ls(FILE* f);
extern int sfs_fopen(char* name);
extern int sfs_fclose(int fileID);
extern int sfs_fread(int fileID, char *buffer, int length);
extern int sfs_fwrite(int fileID, char *buffer, int length);
extern int sfs_lseek(int fileID, int position);
extern int sfs_rm(char *file_name);


typedef struct inode_t
{
    int size_count; // negative for directories
    // inode pointers are a combination of sector num
    // and offset into that sector
    // These are pointers to the next inode in the free list
    int next_inode_num;
    // sector numbers of data
    int direct[DIRECT_BLOCKS];
    int single_indirect;
    int double_indirect;
    int byte_size;
} inode;

typedef struct dentry_t
{
    char f_name[16];
    int inode_num;
} dentry;

typedef struct file_t
{
    inode* node;
    int rw_ptr;
} open_file;



#endif /* !SFS_H */
