/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.2 $
 *    Last Modification: $Date: 2009/10/31 21:28:52 $
 *    File: $RCSfile: kma_p2fl.c,v $
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: kma_p2fl.c,v $
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_P2FL
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>


/************Private include**********************************************/
#include "kpage.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

typedef struct bufferT
{
    kma_size_t size;
    void* ptr;
    struct bufferT* next_size;
    struct bufferT* next_buffer;
    kpage_t* page;
} buffer_t;

/************Global Variables*********************************************/
static buffer_t* size_table = NULL;
const int MINBLOCKSIZE = 64;
/************Function Prototypes******************************************/
kma_size_t choose_block_size(kma_size_t);
void deinit_size_table(void);
void init_size_table(void);
void* alloc_block(kma_size_t);
kma_size_t choose_block_size(kma_size_t);
buffer_t* make_buffers(kma_size_t);
int last_buf_in_page(buffer_t* size_buf, kpage_t* page);
void free_page_from_list(buffer_t* size_buf, kpage_t* page);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
    if(size_table == NULL)
        init_size_table();
    kma_size_t block_size = choose_block_size(size);
    if(block_size != -1)
        return alloc_block(block_size);
    return NULL;
}

/* Go through size_table until block_size is found. Then find first buffer_t
 * on free list, take it off free list and return its ptr + sizeof(buffer_t*)
 */
void*
alloc_block(kma_size_t block_size)
{
    buffer_t* top = size_table;
    while(top->size < block_size)
        top = top->next_size;
    // found the right size
    buffer_t* buf = top->next_buffer;
    if(buf == NULL)
    {
        top->next_buffer = make_buffers(top->size);
        buf = top->next_buffer;
    }

    if(buf == NULL)
        return NULL;

    top->next_buffer = buf->next_buffer;
    buf->next_buffer = top; // set ptr back to size
    return (buf->ptr + sizeof(buffer_t));
}


kma_size_t
choose_block_size(kma_size_t size)
{
    int test_size = MINBLOCKSIZE;
    while(test_size <= PAGESIZE)
    {
        if(test_size >= (size + sizeof(buffer_t)))
            return test_size;
        test_size *= 2;
    }
    return -1;
}

void
deinit_size_table(void)
{
    free_page(size_table->page);
    size_table = NULL;
}

void
init_size_table(void)
{
    int size = 0;
    int offset = 0;
    //this is where we should be bringing down the initial control page    
    kpage_t* page = get_page();
    buffer_t* top = page->ptr + offset; 
    offset += sizeof(buffer_t);
    top->next_buffer = page->ptr + offset; 
    top->page = page;
    top->size = 0;
    top->ptr = NULL;
    offset += sizeof(buffer_t);
    buffer_t* counter = top->next_buffer;
    counter->size = 0;
    counter->next_buffer = NULL;
    counter->next_size = NULL;
    counter->ptr = NULL;
    counter->page = page;
    size = MINBLOCKSIZE;
    size_table = top;
    while(size <= PAGESIZE)
    {
        top->next_size = page->ptr + offset; 
        offset += sizeof(buffer_t);
        top = top->next_size;
        top->next_buffer = NULL;
        top->size = size;
        top->page = page;
        size *= 2;
        top->ptr = NULL;
    }
    top->next_size = NULL;
}

buffer_t*
make_buffers(kma_size_t size)
{
    kpage_t* page = get_page();
    if(page == NULL)
        return NULL;
    //increment number of pages in use
    size_table->next_buffer->size++;    
    kma_size_t offset = 0;
    buffer_t* top = page->ptr + offset;
    top->ptr = page->ptr + offset;
    buffer_t* first = top;
    top->next_size = NULL;
    top->size = size;
    top->page = page;
    offset += size;
    buffer_t* buf;
    while(offset < PAGESIZE)
    {
        buf = page->ptr + offset;
        buf->ptr = page->ptr + offset;
        top->next_buffer = buf;
        top = buf;
        offset += size;
        buf->next_size = NULL;
        buf->size = size;
        buf->page = page;
    }
    top->next_buffer = NULL;
    return first;
}



void
kma_free(void* ptr, kma_size_t size)
{
    buffer_t* buf;
    buf = (buffer_t*)(ptr - sizeof(buffer_t));
    buffer_t* size_buf = buf->next_buffer;
    buf->next_buffer = size_buf->next_buffer;
    size_buf->next_buffer = buf;
    if(last_buf_in_page(size_buf, buf->page) == 1)
        free_page_from_list(size_buf, buf->page);
    if(size_table->next_buffer->size == 0)
        deinit_size_table();
}

//TODO: free memory here
void
free_page_from_list(buffer_t* size_buf, kpage_t* page)
{
    buffer_t* prev = size_buf;
    buffer_t* top = size_buf->next_buffer;
    while(top != NULL)
    {
        if(top->page == page)
        {
            while(top != NULL && top->page == page)
            {
                top = top->next_buffer;
            }
            if(top != NULL)
            {
                prev->next_buffer = top;
                prev = top;
                top = top->next_buffer;
            }
            else
                prev->next_buffer = NULL;
        }
        else
        {
            prev = top;
            top = top->next_buffer;
        }
    }
    size_table->next_buffer->size--;
    free_page(page);
}

int
last_buf_in_page(buffer_t* size_buf, kpage_t* page)
{
    buffer_t* top = size_buf->next_buffer;
    kma_size_t used_buffs_size = 0;
    while(top != NULL)
    {
        if(top->page == page)
            used_buffs_size += size_buf->size;
        top = top->next_buffer;
    }
    if (used_buffs_size >= PAGESIZE)
        return 1;
    return 0;
}


#endif // KMA_P2FL

