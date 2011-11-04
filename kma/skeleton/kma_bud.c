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

/************Private include**********************************************/
#include "kpage.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define LEFT 10
#define RIGHT 11

#define FALSE 0
#define TRUE 1

#define MINBUFFERSIZE 64

/*
* All size fields indicate size of remaining free space after overhead of
* buffer_t and buddy_page_t control structs.
*
* Maintains a free list with list of pages and the total amount of space they
* have available.  Checking this free list will avoid traversing some trees
* that don't have space, but not all.  Not sure if worth it.  Not going to make
* trees higher up than a single page because having some buffer_t*s that don't
* have actual memory associated with them will make coalescing more
* complicated.
*
* The full field will only be true if the node is a leaf that's already been
* malloc'd
*/

struct buddy_pageT;

typedef struct bufferT
{
    kma_size_t size;
    struct bufferT* parent;
    struct bufferT* sibling;
    struct bufferT* child;
    struct buddy_pageT* page;
    int full;
    void* ptr;

} buffer_t;


typedef struct buddy_pageT
{
    buffer_t* top_node;
    kma_size_t free_space;
    kpage_t* raw_page;
    struct buddy_pageT* next;
    struct buddy_pageT* prev;

} buddy_page_t;


/************Global Variables*********************************************/

static buddy_page_t* buddy_page_list = NULL;

/************Function Prototypes******************************************/

void* alloc_to_page(kma_size_t size, buddy_page_t* page);
buddy_page_t* add_new_page(void);
int page_has_space(kma_size_t size, buddy_page_t* page);
void* search_and_alloc(kma_size_t need_size, buffer_t* node);
void* split_to_size(kma_size_t need_size, buffer_t* node);
buffer_t* init_child(int side, buffer_t* parent);
void init_top_node(buffer_t* node, kpage_t* page, buddy_page_t* buddy_page);
void coalesce(buffer_t* node);
int totally_free(buffer_t* node);
void free_node(buffer_t* node);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

/*
* Malloc: Check free list to find first page with enough space available.
* Search tree on page to find free space.  If no free space on page tree move
* onto next page with enough space.  Else if successful at finding space on
* tree then decrement space from free list for page and return found space.  If
* no pages with enough space then get a new page, make a buffer_t on it, resume
* search and find it.
*/

void*
kma_malloc(kma_size_t size)
{
    buddy_page_t* page = buddy_page_list;
    return alloc_to_page(size, page);
}

void*
alloc_to_page(kma_size_t size, buddy_page_t* page)
{
    if(page == NULL)
    {
        buddy_page_t* new_page = add_new_page();
        if(new_page == NULL)
        {
            // This only occurs if get_page() fails
            return NULL;
        }
        return alloc_to_page(size, new_page);
    }
    else if(page_has_space(size, page) == TRUE)
    {
        void* mem_loc = search_and_alloc(size, page->top_node);
        if(mem_loc != NULL)
        {
            page->free_space -= size;
            return mem_loc;
        }
    }
    // If page doesn't have space (which can be discovered in page_has_space()
    // or in search_and_alloc()) then try next page.
    return alloc_to_page(size, page->next);
}

buddy_page_t*
add_new_page(void)
{
    buddy_page_t* prev = NULL;
    buddy_page_t* top = buddy_page_list;
    while(top != NULL)
    {
        prev = top;
        top = top->next;
    }
    kpage_t* raw_page = get_page();
    if(raw_page == NULL)
        return NULL;
    // Put control structs on new page
    buddy_page_t* new = (buddy_page_t*)raw_page->ptr;
    buffer_t* top_node = (buffer_t*)(raw_page->ptr + sizeof(buddy_page_t));
    init_top_node(top_node, raw_page, new);
    new->top_node = top_node;
    new->raw_page = raw_page;
    new->free_space = raw_page->size - (kma_size_t)sizeof(buddy_page_t);
    new->next = NULL;
    new->prev = top;
    if(top != NULL)
        top->next = new;
    else
        buddy_page_list = new;
    return new;
}


int
page_has_space(kma_size_t size, buddy_page_t* page)
{
    if(page->free_space >= size)
        return TRUE;
    return FALSE;
}


void*
search_and_alloc(kma_size_t need_size, buffer_t* node)
{
    if(node->full == TRUE)
    {
        // Node is a fully malloc'd leaf
        return NULL;
    }
    else if(node->child == NULL)
    {
        // Free leaf
        return split_to_size(need_size, node);
    }
    else
    {
        // Not a leaf node, check if space in children
        // First check left child
        void* found_space = search_and_alloc(need_size, node->child);
        if(found_space)
            return found_space;
        else
        {
            // Then check right child, left child's sibling
            return search_and_alloc(need_size, node->child->sibling);
        }
    }
}

/* This function is only called on leaf nodes */
void*
split_to_size(kma_size_t need_size, buffer_t* node)
{
    if(node->size >= need_size && \
      (node->size/2 - sizeof(buffer_t)) < need_size)
    {
        // Node is right size leaf node, mark as full and return ptr
        node->full = TRUE;
        return node->ptr;
    }
    else if((node->size/2 - sizeof(buffer_t)) < MINBUFFERSIZE || \
      node->size < need_size)
    {
        // No space to be found
        return NULL;
    }
    else
    {
        // Node is too big, make children and recur on on left child
        // (left or right is arbitrary choice)
        buffer_t* l_child = init_child(LEFT, node);
        init_child(RIGHT, node);
        return split_to_size(need_size, l_child);
    }
}

buffer_t*
init_child(int side, buffer_t* parent)
{
    buffer_t* child;
    if(side == LEFT)
    {
        child = (buffer_t*)parent->ptr;
        parent->child = child;
        child->sibling = (buffer_t*)((parent->size/2) + parent->ptr); // Right sibling
    }
    else if(side == RIGHT)
    {
        child = (buffer_t*)((parent->size/2) + parent->ptr);
        child->sibling = (buffer_t*)parent->ptr; // Left sibling
    }
    child->full = FALSE;
    child->size = (parent->size/2) - sizeof(buffer_t);
    child->parent = parent;
    child->child = NULL;
    child->ptr = child + sizeof(buffer_t);
    child->page = parent->page;
    child->page->free_space -= sizeof(buffer_t);
    return child;
}

/* First node on tree ie first node on page */
void
init_top_node(buffer_t* node, kpage_t* raw_page, buddy_page_t* buddy_page)
{
    node->parent = NULL;
    node->child = NULL;
    node->sibling = NULL;
    // The buddy_page_t struct is the only thing already on the page
    size_t overhead_size = sizeof(buddy_page_t) + sizeof(buffer_t);
    node->ptr = (void*)(raw_page->ptr + overhead_size);
    node->size = (kma_size_t)(raw_page->size - overhead_size);
    node->full = FALSE;
    node->page = buddy_page;
    buddy_page->free_space -= sizeof(buffer_t);
}

/*
Free:
Get buffer_t from ptr. Check if sibling is free, if so start coalescing.
If not then just mark buffer_t as free.
*/
void
kma_free(void* ptr, kma_size_t size)
{
    buffer_t* buf;
    buf = (buffer_t*)(ptr - sizeof(buffer_t)); //- 3080);
    coalesce(buf);
}

void
coalesce(buffer_t* node)
{
    if(node->parent == NULL)
    {
        // If top node of tree ie first node on page then free page and remove
        // from list
        if(node->page->prev)
            node->page->prev = node->page->next;
        if(node->page->next)
            node->page->next->prev = node->page->prev;
        if(buddy_page_list == node->page)
            buddy_page_list = NULL;
        free_page(node->page->raw_page);
    }
    else if(totally_free(node->sibling) == TRUE)
    {
        // Destroy parent's pointer to child, effectively deleting both
        // children, and try to coalesce parent
        node->parent->child = NULL;
        coalesce(node->parent);
    }
    else
        free_node(node);
}

/* Checks if node is empty and all of its children are empty */
int
totally_free(buffer_t* node)
{
    if(node == NULL)
        return TRUE;
    else if(node->full == TRUE)
        return FALSE;
    else if(node->child == NULL)
        return TRUE;
    else
        return ((totally_free(node->child) == TRUE) && \
                (totally_free(node->child->sibling) == TRUE));
}

void
free_node(buffer_t* node)
{
    node->full = FALSE;
    node->page->free_space += node->size;

}


#endif // KMA_BUD

