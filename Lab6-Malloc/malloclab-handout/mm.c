/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * TODO
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE - 1)) / DSIZE)

/* sizeof(size_t) rounds up to alignment (8 in 64-bit machine) */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

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

/* static pointer that points to prologue header */
static char *heap_listp;

/* static functions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

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

    /* coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * coalesce - coalesce (previous) free blocks, return current or new bp
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        bp = PREV_BLKP(bp);
        size += GET_SIZE(FTRP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
        size += GET_SIZE(FTRP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* adjusted block size (in bytes) */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;

    /* ignore spuriouorts to be; fals requests */
    if (size == 0)
        return NULL;

    /* adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ALIGN(size + DSIZE);

    /* search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* no fit found. get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * find_fit - iterate the implicit list to see if there is any free block big enough
 */
static void *find_fit(size_t asize)
{
    char *bp = heap_listp;

    while (GET_ALLOC(HDRP(bp)) || asize > GET_SIZE(HDRP(bp)))
        if (GET_SIZE(HDRP(bp)) == 0)
            return NULL;
        else
            bp = NEXT_BLKP(bp);

    return bp;
}

/*
 * place - place asize bytes in the free block and split
 *       - block ptr passed in must be guaranteed to be free
 */
static void place(void *bp, size_t asize)
{
    size_t fsize = GET_SIZE(HDRP(bp));

    /* split the free block if its left space greater than DSIZE(for head and foot) */
    if ((fsize - asize) > DSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        fsize -= asize;
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(fsize, 0));
        PUT(FTRP(bp), PACK(fsize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(fsize, 1));
        PUT(FTRP(bp), PACK(fsize, 1));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    char *bp = ptr;
    char *oldbp;
    size_t oldsize = GET_SIZE(HDRP(bp));
    size_t newsize = DSIZE * ALIGN(size + DSIZE);

    /* no need to call malloc if newsize <= oldsize */
    if (newsize <= oldsize)
    {
        /* split the block */
        if (oldsize - newsize > DSIZE)
        {
            PUT(HDRP(bp), PACK(newsize, 1));
            PUT(FTRP(bp), PACK(newsize, 1));
            oldbp = bp;
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(oldsize - newsize, 0));
            PUT(FTRP(bp), PACK(oldsize - newsize, 0));
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
    if (prev_size + oldsize + next_size >= newsize)
    {
        /* use prev free space as much as possible */
        if (prev_size)
        {
            memmove(prev_bp, bp, oldsize - DSIZE);
            PUT(HDRP(prev_bp), PACK(newsize, 1));
            PUT(FTRP(prev_bp), PACK(newsize, 1));
            next_bp = NEXT_BLKP(prev_bp);
            PUT(HDRP(next_bp), PACK(prev_size + oldsize + next_size - newsize, 0));
            PUT(FTRP(next_bp), PACK(prev_size + oldsize + next_size - newsize, 0));
            return prev_bp;
        }
        else
        {
            PUT(HDRP(bp), PACK(newsize, 1));
            PUT(FTRP(bp), PACK(newsize, 1));
            next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK(oldsize + next_size - newsize, 0));
            PUT(FTRP(next_bp), PACK(oldsize + next_size - newsize, 0));
            return bp;
        }
    }

    /* must call malloc for new memory (newsize > oldsize) */
    oldbp = bp;
    if ((bp = mm_malloc(size)) == NULL)
        return NULL;
    memcpy(bp, oldbp, size);
    mm_free(oldbp);
    return bp;
}