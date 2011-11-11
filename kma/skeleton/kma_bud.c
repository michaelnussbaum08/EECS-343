/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.2 $
 *    Last Modification: $Date: 2009/10/31 21:28:52 $
 *    File: $RCSfile: kma_bud.c,v $
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: kma_bud.c,v $
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
#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

#define MINBUFFERSIZE 64

#define BITMAPSIZE PAGESIZE/MINBUFFERSIZE

typedef struct bufferT
{
    kma_size_t size;
    void* ptr;
    struct bufferT* next_size;
    struct bufferT* next_buffer;
    struct bufferT* prev_buffer;
    kpage_t* page;
} buffer_t;


/************Global Variables*********************************************/
static buffer_t* free_list = NULL;

/************Function Prototypes******************************************/

//REMOVE
void print_free_list(char*);

void deinit_free_list(void);
void init_free_list(void);
void alloc(buffer_t* buf);
void dealloc(buffer_t* buf);
int add_new_page(void);
void set_bitmap(buffer_t* node, int value);
buffer_t* search_for_buffer(kma_size_t);
buffer_t* init_buffer(buffer_t* size_header, void* ptr, kpage_t* page);
void remove_buf_from_free_list(buffer_t* node);
kma_size_t choose_block_size(kma_size_t);
buffer_t* split_to_size(kma_size_t, buffer_t*);
void coalesce(buffer_t*);


/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
    if(free_list == NULL)
        init_free_list();
    print_free_list("top of kma_malloc");
    kma_size_t block_size = choose_block_size(size);
    if(block_size != -1)
    {
        buffer_t* buf = search_for_buffer(block_size);
        if(buf == NULL)
        {
            int success = add_new_page();
            if(success == -1)
                return NULL;
            buf = search_for_buffer(block_size);
        }
        alloc(buf);
        return buf->ptr;
    }
    return NULL;
}

int
add_new_page(void)
{
    kpage_t* page = get_page();
    if(page == NULL)
        return -1;
    // iterate through free list headers to find size header for the page
    free_list->next_buffer->size += 1; //keeps count of number of pages in use
    buffer_t* size_header =  free_list;
    while(size_header)
    {
        if(size_header->size == PAGESIZE)
            break;
        else
            size_header = size_header->next_size;
    }

    // init buffer to store bitmap on page
    buffer_t* buf = init_buffer(size_header, page->ptr, page);
    //I think the line below is already taken car of in init_buffer
    //buf->prev_buffer = size_header;
    kma_size_t block_size = choose_block_size(BITMAPSIZE);
    buffer_t* bitmap_mem = split_to_size(block_size, buf);

    set_bitmap(bitmap_mem, 255);
    return 0;
}

int counter = 0;

void
alloc(buffer_t* node)
{
    printf("COUNTER: %d\n", counter++);
    set_bitmap(node, 255);
    remove_buf_from_free_list(node->prev_buffer);
}

void
set_bitmap(buffer_t* node, int value)
{
    // WATCH THIS!!
    int bitmap_index = ((void*)node - node->page->ptr)/MINBUFFERSIZE;
    int size = node->size/MINBUFFERSIZE;
    memset((node->page->ptr + sizeof(buffer_t) + bitmap_index), value, size);
}



void
deinit_free_list(void)
{
    free_page(free_list->page);
    free_list = NULL;
}

void
init_free_list(void)
{
    int size = 0;
    int offset = 0;
    //this is where we should be bringing down the initial control page
    kpage_t* page = get_page();
    // first buffer just points to a size list and a page counter
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
    counter->prev_buffer = top;
    size = MINBUFFERSIZE;
    free_list = top;
    while(size <= PAGESIZE)
    {
        top->next_size = page->ptr + offset;
        offset += sizeof(buffer_t);
        top->next_size->prev_buffer = top;
        top = top->next_size;
        top->next_buffer = NULL;
        top->size = size;
        top->page = page;
        size *= 2;
        top->ptr = NULL;
    }
    top->next_size = NULL;
}


kma_size_t
choose_block_size(kma_size_t size)
{
    int test_size = MINBUFFERSIZE;
    while(test_size <= PAGESIZE)
    {
        if(test_size >= (size + sizeof(buffer_t)))
            return test_size;
        test_size *= 2;
    }
    return -1;
}

buffer_t*
search_for_buffer(kma_size_t size)
{
    buffer_t* top = free_list;
    while(top != NULL)
    {
        if(top->size >= size && top->next_buffer != NULL)
        {
            if(top->size == size)
                return top->next_buffer;
            return split_to_size(size, top->next_buffer);
        }
        top = top->next_size;
    }
    return NULL;
}

buffer_t*
split_to_size(kma_size_t need_size, buffer_t* node)
{
    if((node->size/2 < need_size) || (node->size/2 <= MINBUFFERSIZE))
    {
        // found right size
        return node;
    }
    else
    {
        // Node is too big, make children and recur on on left child
        // (left or right is arbitrary choice)

        // parent's prev buffer is it's size header, the size header's prev buffer
        // is the next smaller size header
        buffer_t* parent_size_header = node->prev_buffer;
        buffer_t* child_size_header = parent_size_header->prev_buffer;
        void* right_ptr = (node->size/2) + (void*)node;
        //void* right_ptr = (node->size/2) + node->ptr; // I think node->ptr is
        //wrong because then we accumulate offsets of sizeof(buffer_t), when we
        //actuall want to eliminate the offsets. Confirmed this fix stops the
        //segfault in min.trace, but there is still one.
        remove_buf_from_free_list(parent_size_header);
        init_buffer(child_size_header, right_ptr, node->page);
        buffer_t* l_child = init_buffer(child_size_header, (void*)node, node->page);
        return split_to_size(need_size, l_child);
    }
}

buffer_t*
init_buffer(buffer_t* size_header, void* ptr, kpage_t* page)
{
    buffer_t* child = (buffer_t*)ptr;
    child->size = size_header->size;
    //child->size = (size_header->size/2);
    child->next_size = NULL;
    child->ptr = ptr + sizeof(buffer_t);

    //CHECK THAT CHILD->PTR IS IN RIGHT PLACE

    child->page = page;
    child->next_buffer = size_header->next_buffer;
    if(size_header->next_buffer)
        size_header->next_buffer->prev_buffer = child;
    size_header->next_buffer = child;
    child->prev_buffer = size_header;
    return child;
}

void
remove_buf_from_free_list(buffer_t* size_header)
{
    if(size_header->next_buffer->next_buffer)
        size_header->next_buffer->next_buffer->prev_buffer = size_header;
    size_header->next_buffer = size_header->next_buffer->next_buffer;
    /*
    node->prev_buffer->next_buffer = node->next_buffer;
    if(node->next_buffer)
        node->next_buffer->prev_buffer = node->prev_buffer;
        */
}



void
kma_free(void* ptr, kma_size_t size)
{
    buffer_t* buf;
    buf = (buffer_t*)(ptr - sizeof(buffer_t));
    //coalesce(buf);
}

void
print_free_list(char* msg)
{
    printf("%s\n", msg);
    buffer_t* top = free_list->next_size; //skip first size of 0
    while(top)
    {
        buffer_t* size_top = top->next_buffer;
        int buf_count = 0;
        while(size_top)
        {
            if(size_top->size != top->size)
                printf("Bug in buffer of size %d\n", top->size);
            buf_count++;
            size_top = size_top->next_buffer;
        }
        printf("size %d has %d buffers\n", top->size, buf_count);
        top = top->next_size;
    }
}

#endif // KMA_BUD

