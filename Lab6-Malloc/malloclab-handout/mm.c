/*
 * mm.c - implement explicit list
 * 
 * In this approach, we still have header and footer for used or free block.
 * (header for size info, footer for coalescing)
 * 
 * In a comparsion to implicit list, explicit list maintains a list only 
 * contain free blocks. So mm_malloc could be faster by just seraching on
 * free lists, while it still takes linear time complexity.
 * 
 * The key point is we place 2 pointers, which point to the prev and next free 
 * block, in a free block so that it knows where the prev and next free block
 * are.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "GEL-Lyte V",
    /* First member's full name */
    "Erised",
    /* First member's email address */
    "506933131@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* Basic constants and macros */
#define WSIZE 4             /* word and header/footer size (bytes) */
#define DSIZE 8             /* double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* extend heap by this amount (bytes) */
#define HEAPCHECK 0         /* heap check option */

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE - 1)) / DSIZE)

/* Max macro function */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block prt bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-DSIZE))

/* Given block ptr bp, read and write prev and next pointer in the free block */
#define GET_PREV(bp) (*(char **)(bp))
#define GET_NEXT(bp) (*((char **)(bp) + 1))
#define PUT_PREV(bp, ptr) (GET_PREV(bp) = ptr)
#define PUT_NEXT(bp, ptr) (GET_NEXT(bp) = ptr)

/* static pointer that points to prologue header */
static char *heap_listp = NULL;

/* static pointer that points to start of free blocks */
static char *heap_freep;

/* static functions */
static void *extend_heap(size_t words);

/* explicit free list manipulation */
static inline void ex_delete(void *bp);
static void ex_insert(void *bp);
static void ex_insert_after(void *prev_bp, void *bp);
static void *ex_coalesce(void *bp);
static inline void *ex_find_fit(size_t asize);
static void ex_place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                            /* Alignment padding header */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Alignment padding footer (Epilogue header) */
    heap_listp += 2 * WSIZE;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    heap_freep = NULL;
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * extend_heap - extend heap with words, return block ptr or NULL
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* set free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    /* add the new free block from the end */
    /* coalesce if the previous block was free */
    if (heap_freep)
    {
        ex_insert_after(GET_PREV(heap_freep), bp);
        bp = ex_coalesce(bp);
    }
    else
        ex_insert_after(NULL, bp);

    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (HEAPCHECK && !mm_check())
        exit(0);

    size_t asize;      /* adjusted block size (in bytes) */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;

    /* ignore spurious requests */
    if (size == 0)
        return NULL;

    /* adjust block size to include overhead and alignment reqs */
    if (size <= 2 * DSIZE)
        asize = 3 * DSIZE;
    else
        asize = DSIZE * ALIGN(size + DSIZE);

    /* search the free list for a fit */
    if ((bp = ex_find_fit(asize)) != NULL)
    {
        ex_place(bp, asize);
        return bp;
    }

    /* no fit found. get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    ex_place(bp, asize);
    return bp;
}

/*
 * mm_free - freeing a block
 */
void mm_free(void *bp)
{
    if (HEAPCHECK && !mm_check())
        exit(0);

    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    ex_insert(bp);
    ex_coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (HEAPCHECK && !mm_check())
        exit(0);

    char *bp = ptr;
    char *oldbp;
    size_t oldsize = GET_SIZE(HDRP(bp));
    size_t newsize;

    /* adjust newsize */
    if (size <= 2 * DSIZE)
        newsize = 3 * DSIZE;
    else
        newsize = DSIZE * ALIGN(size + DSIZE);

    /* no need to call malloc if newsize <= oldsize */
    if (newsize <= oldsize)
    {
        /* split the block */
        if (oldsize - newsize >= 3 * DSIZE)
        {
            PUT(HDRP(bp), PACK(newsize, 1));
            PUT(FTRP(bp), PACK(newsize, 1));
            oldbp = bp;
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(oldsize - newsize, 0));
            PUT(FTRP(bp), PACK(oldsize - newsize, 0));

            ex_insert(bp);

            return oldbp;
        }
        else
            return bp;
    }

    /* no need to call malloc if oldsize plus prev & next free space enough for newsize */
    char *prev_bp = PREV_BLKP(bp);
    char *next_bp = NEXT_BLKP(bp);
    size_t prev_size = GET_ALLOC(FTRP(prev_bp)) ? 0 : GET_SIZE(FTRP(prev_bp));
    size_t next_size = GET_ALLOC(HDRP(next_bp)) ? 0 : GET_SIZE(HDRP(next_bp));
    size_t free_size = prev_size + oldsize + next_size - newsize;
    if (prev_size + oldsize + next_size >= newsize)
    {
        char *prev_prev_bp = NULL;
        if (prev_size)
        {
            prev_prev_bp = GET_PREV(prev_bp);
            ex_delete(prev_bp);
        }
        else
            prev_prev_bp = GET_PREV(next_bp);
        
        if (next_size)
            ex_delete(next_bp);

        if (free_size < 3 * DSIZE)
        {
            if (prev_size)
            {
                memmove(prev_bp, bp, oldsize - DSIZE);
                PUT(HDRP(prev_bp), PACK(prev_size + oldsize + next_size, 1));
                PUT(FTRP(prev_bp), PACK(prev_size + oldsize + next_size, 1));

                return prev_bp;
            }
            else
            {
                PUT(HDRP(bp), PACK(prev_size + oldsize + next_size, 1));
                PUT(FTRP(bp), PACK(prev_size + oldsize + next_size, 1));
                return bp;
            }
        }

        if (prev_size)
        {
            memmove(prev_bp, bp, oldsize - DSIZE);
            PUT(HDRP(prev_bp), PACK(newsize, 1));
            PUT(FTRP(prev_bp), PACK(newsize, 1));
            bp = NEXT_BLKP(prev_bp);
            PUT(HDRP(bp), PACK(free_size, 0));
            PUT(FTRP(bp), PACK(free_size, 0));

            if ((void *)heap_freep == NULL || (void *)bp < (void *)heap_freep)
                ex_insert_after(NULL, bp);
            else
                ex_insert_after(prev_prev_bp, bp);

            return prev_bp;
        }
        else
        {
            PUT(HDRP(bp), PACK(newsize, 1));
            PUT(FTRP(bp), PACK(newsize, 1));
            prev_bp = bp;
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(free_size, 0));
            PUT(FTRP(bp), PACK(free_size, 0));

            if ((void *)heap_freep == NULL || (void *)bp < (void *)heap_freep)
                ex_insert_after(NULL, bp);
            else
                ex_insert_after(prev_prev_bp, bp);

            return prev_bp;
        }
    }

    /* must call malloc for new memory (newsize > oldsize) */
    oldbp = bp;
    if ((bp = mm_malloc(size)) == NULL)
        return NULL;
    memcpy(bp, oldbp, oldsize - DSIZE);
    mm_free(oldbp);
    return bp;
}

/*
 * mm_check - check heap consistency
 *          - return -1 for uninitialized heap, 1 for consistent, 0 for error
 */
int mm_check(void)
{
    char *bp;
    size_t heapsize = 0;
    int free_cnt = 0;

    /* heap has not been initialized */
    if (heap_listp == NULL || heap_listp == (void *)-1)
        return -1;

    /* check first 3 words */
    bp = ((char *)heap_listp) - 2 * WSIZE;
    if (GET(bp) != 0 || GET(bp + WSIZE) != PACK(DSIZE, 1) || GET(bp + 2 * WSIZE) != PACK(DSIZE, 1))
    {
        fprintf(stderr, "Heap check error: first 3 words not consistent!\n");
        return 0;
    }
    heapsize += 3 * WSIZE;

    /* check epilogue word */
    bp = ((char *)mem_heap_hi()) + 1 - WSIZE;
    if (GET(bp) != PACK(0, 1))
    {
        fprintf(stderr, "Heap check error: last word not consistent!\n");
        return 0;
    }
    heapsize += WSIZE;

    /* loop through the heap to check consistency */
    bp = ((char *)heap_listp) + 2 * WSIZE; /* point to first block */

    while (GET(HDRP(bp)) != PACK(0, 1))
    {
        /* check consistency of head and foot */
        if (GET(HDRP(bp)) != GET(FTRP(bp)))
        {
            fprintf(stderr, "Heap check error: block's head and foot not consistent!\n");
            return 0;
        }

        /* check coalescing */
        if (!GET_ALLOC(HDRP(bp)))
        {
            if (!GET_ALLOC(FTRP(PREV_BLKP(bp))) || !GET_ALLOC(HDRP(NEXT_BLKP(bp))))
            {
                fprintf(stderr, "Heap check error: two contiguous free blocks!\n");
                return 0;
            }
            free_cnt++;
        }

        heapsize += GET_SIZE(HDRP(bp));
        bp = NEXT_BLKP(bp);
    }

    /* check heap size */
    if (heapsize != mem_heapsize())
    {
        fprintf(stderr, "Heap check error: heap size not consistent!\n");
        return 0;
    }

    /* check explicit list*/
    if (heap_freep)
    {
        bp = heap_freep;
        while (1)
        {
            if (GET_ALLOC(HDRP(bp)))
            {
                fprintf(stderr, "Heap check error: free block error!\n");
                return 0;
            }
            free_cnt--;
            bp = GET_NEXT(bp);
            if ((void *)bp == (void *)heap_freep)
                break;
        }
        if (free_cnt)
        {
            fprintf(stderr, "Heap check error: free block count(delta=%d) not consistent!\n", free_cnt);
            return 0;
        }
    }

    return 1;
}

/*
 * ex_delete - delete a free block from the explicit list
 */
static inline void ex_delete(void *bp)
{
    if (bp == NULL)
        return;

    /* bp is the only one in explicit list */
    if (bp == (void *)GET_NEXT(bp))
    {
        heap_freep = NULL;
        return;
    }

    /* at least two free blocks in explicit list */
    if (bp == (void *)heap_freep)
        heap_freep = GET_NEXT(bp);
    PUT_NEXT(GET_PREV(bp), GET_NEXT(bp));
    PUT_PREV(GET_NEXT(bp), GET_PREV(bp));
}

/*
 * ex_insert - insert a free block according to the order of address
 */
static void ex_insert(void *bp)
{
    /* check if the explicit list is valid */
    if (heap_freep == NULL)
    {
        PUT_PREV(bp, bp);
        PUT_NEXT(bp, bp);
        heap_freep = bp;
        return;
    }

    /* check if there is nearby free block */
    if (!GET_ALLOC(FTRP(PREV_BLKP(bp))))
    {
        ex_insert_after(PREV_BLKP(bp), bp);
        return;
    }
    if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))))
    {
        if ((void *)NEXT_BLKP(bp) == (void *)heap_freep)
            ex_insert_after(NULL, bp);
        else
            ex_insert_after(GET_PREV(NEXT_BLKP(bp)), bp);
        return;
    }

    /* check if bp < heap_freep */
    if (bp < (void *)heap_freep)
    {
        ex_insert_after(NULL, bp);
        return;
    }

    /* iterate through the explicit list to find previous free block */
    char *prev_bp = heap_freep;
    while (1)
    {
        if ((void *)GET_NEXT(prev_bp) == (void *)heap_freep)
            break;
        if ((void *)GET_NEXT(prev_bp) > bp)
            break;
        prev_bp = GET_NEXT(prev_bp);
    }
    ex_insert_after(prev_bp, bp);
}

/*
 * ex_insert_after - insert a free block after an existing free block
 */
static void ex_insert_after(void *prev_bp, void *bp)
{
    /* if prev_bp is NULL then put bp in head */
    if (prev_bp == NULL)
    {
        if (heap_freep == NULL)
        {
            PUT_PREV(bp, bp);
            PUT_NEXT(bp, bp);
        }
        else
        {
            PUT_PREV(bp, GET_PREV(heap_freep));
            PUT_NEXT(bp, heap_freep);
            PUT_PREV(heap_freep, bp);
            PUT_NEXT(GET_PREV(bp), bp);
        }
        heap_freep = bp;
    }
    else
    {
        PUT_PREV(bp, prev_bp);
        PUT_NEXT(bp, GET_NEXT(prev_bp));
        PUT_PREV(GET_NEXT(prev_bp), bp);
        PUT_NEXT(prev_bp, bp);
    }
}

/*
 * ex_coalesce - coalesce free blocks, return current or new bp
 */
static void *ex_coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* delete block from free list and update header/footer */
    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(GET_NEXT(bp)));

        ex_delete(GET_NEXT(bp));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        bp = GET_PREV(bp);
        size += GET_SIZE(FTRP(bp));

        ex_delete(GET_NEXT(bp));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else
    {
        size += GET_SIZE(HDRP(GET_NEXT(bp)));
        ex_delete(GET_NEXT(bp));
        bp = GET_PREV(bp);
        size += GET_SIZE(FTRP(bp));
        ex_delete(GET_NEXT(bp));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    return bp;
}

/*
 * ex_find_fit - iterate the explicit list to see if there is any free block big enough
 */
static inline void *ex_find_fit(size_t asize)
{
    char *bp = heap_freep;
    if (bp == NULL)
        return NULL;

    while (1)
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
            break;
        bp = GET_NEXT(bp);
        if (bp == heap_freep)
            return NULL;
    }

    return bp;
}

/*
 * ex_place - place asize bytes in the free block and split
 *          - block ptr passed in must be guaranteed to be free
 */
static void ex_place(void *bp, size_t asize)
{
    size_t fsize = GET_SIZE(HDRP(bp));

    /* split the free block if its left space not less than 3*DSIZE(for head/foot and 2 pointers) */
    if ((fsize - asize) >= 3 * DSIZE)
    {
        char *next_bp;

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        fsize -= asize;
        next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(fsize, 0));
        PUT(FTRP(next_bp), PACK(fsize, 0));

        if ((void *)GET_NEXT(bp) == bp)
        {
            PUT_PREV(next_bp, next_bp);
            PUT_NEXT(next_bp, next_bp);
            heap_freep = next_bp;
        }
        else
        {
            ex_insert_after(bp, next_bp);
            ex_delete(bp);
        }
    }
    else
    {
        ex_delete(bp);

        PUT(HDRP(bp), PACK(fsize, 1));
        PUT(FTRP(bp), PACK(fsize, 1));
    }
}