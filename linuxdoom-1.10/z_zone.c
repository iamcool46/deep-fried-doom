// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Zone Memory Allocation. Neat.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: z_zone.c,v 1.4 1997/02/03 16:47:58 b1 Exp $";

#include <stdint.h>
#include "z_zone.h"
#include "i_system.h"
#include "doomdef.h"


//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of no value to free a cachable block,
//  because it will get overwritten automatically if needed.
// 

#define ZONEID	0x1d4a11


typedef struct
{
    // total bytes malloced, including header
    int		size;

    // start / end cap for linked list
    memblock_t	blocklist;

    memblock_t* rover;

} memzone_t;



memzone_t* mainzone;



//
// Z_ClearZone
//
void Z_ClearZone(memzone_t* zone)
{
    memblock_t* block;

    // set the entire zone to one free block
    zone->blocklist.next =
        zone->blocklist.prev =
        block = (memblock_t*)((byte*)zone + sizeof(memzone_t));

    zone->blocklist.user = (void*)zone;
    zone->blocklist.tag = PU_STATIC;
    zone->rover = block;

    block->prev = block->next = &zone->blocklist;

    // NULL indicates a free block.
    block->user = NULL;

    block->size = zone->size - sizeof(memzone_t);
}



//
// Z_Init
//
void Z_Init(void)
{
    memblock_t* block;
    int		size;

    mainzone = (memzone_t*)I_ZoneBase(&size);
    mainzone->size = size;

    // set the entire zone to one free block
    mainzone->blocklist.next =
        mainzone->blocklist.prev =
        block = (memblock_t*)((byte*)mainzone + sizeof(memzone_t));

    mainzone->blocklist.user = (void*)mainzone;
    mainzone->blocklist.tag = PU_STATIC;
    mainzone->rover = block;

    block->prev = block->next = &mainzone->blocklist;

    // NULL indicates a free block.
    block->user = NULL;

    block->size = mainzone->size - sizeof(memzone_t);

    // Verify initialization
    printf("Z_Init: Zone initialized\n");
    printf("  mainzone = %p\n", mainzone);
    printf("  mainzone->size = %d bytes\n", mainzone->size);
    printf("  mainzone->blocklist.next = %p\n", mainzone->blocklist.next);
    printf("  mainzone->blocklist.prev = %p\n", mainzone->blocklist.prev);
    printf("  sizeof(memblock_t) = %zu\n", sizeof(memblock_t));
    printf("  sizeof(memzone_t) = %zu\n", sizeof(memzone_t));
    printf("  initial block size = %d bytes\n", block->size);
}


//
// Z_Free
//
void Z_Free(void* ptr)
{
    memblock_t* block;
    memblock_t* other;

    if (!ptr)
    {
        printf("Z_Free: Warning - attempt to free NULL pointer\n");
        return;
    }

    block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));

    if (block->user > (void**)0x100)
    {
        // smaller values are not pointers
        // Note: OS-dependend?

        // clear the user's mark
        *block->user = 0;
    }

    // mark as free
    block->user = NULL;
    block->tag = 0;

    other = block->prev;

    if (!other->user)
    {
        // merge with previous free block
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;

        if (block == mainzone->rover)
            mainzone->rover = other;

        block = other;
    }

    other = block->next;
    if (!other->user)
    {
        // merge the next free block onto the end
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;

        if (other == mainzone->rover)
            mainzone->rover = block;
    }
}



//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#define MINFRAGMENT		64


void*
Z_Malloc
(int		size,
    int		tag,
    void* user)
{
    int		extra;
    memblock_t* start;
    memblock_t* rover;
    memblock_t* newblock;
    memblock_t* base;

    size = (size + 3) & ~3;

    // Debug output for large allocations
    if (size > 1000000) {
        printf("Z_Malloc: Large allocation request: %d bytes, tag=%d\n", size, tag);
    }

    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // account for size of block header
    size += sizeof(memblock_t);

    // if there is a free block behind the rover,
    //  back up over them
    base = mainzone->rover;

    if (!base->prev->user)
        base = base->prev;

    rover = base;
    start = base->prev;

    do
    {
        if (rover == start)
        {
            // scanned all the way around the list
            printf("Z_Malloc: About to fail - requested %d bytes, tag=%d\n", size, tag);
            I_Error("Z_Malloc: failed on allocation of %i bytes", size);
        }

        if (rover->user)
        {
            if (rover->tag < PU_PURGELEVEL)
            {
                // hit a block that can't be purged,
                //  so move base past it
                base = rover = rover->next;
            }
            else
            {
                // free the rover block (adding the size to base)

                // the rover can be the base block
                base = base->prev;
                Z_Free((byte*)rover + sizeof(memblock_t));
                base = base->next;
                rover = base->next;
            }
        }
        else
            rover = rover->next;
    } while (base->user || base->size < size);


    // found a block big enough
    extra = base->size - size;

    if (extra > MINFRAGMENT)
    {
        // there will be a free fragment after the allocated block
        newblock = (memblock_t*)((byte*)base + size);
        newblock->size = extra;

        // NULL indicates free block.
        newblock->user = NULL;
        newblock->tag = 0;
        newblock->prev = base;
        newblock->next = base->next;
        newblock->next->prev = newblock;

        base->next = newblock;
        base->size = size;
    }

    if (user)
    {
        // mark as an in use block
        base->user = user;
        *(void**)user = (void*)((byte*)base + sizeof(memblock_t));
    }
    else
    {
        if (tag >= PU_PURGELEVEL)
            I_Error("Z_Malloc: an owner is required for purgable blocks");

        // mark as in use, but unowned	
        base->user = (void*)2;
    }
    base->tag = tag;

    // next allocation will start looking here
    mainzone->rover = base->next;

    return (void*)((byte*)base + sizeof(memblock_t));
}



//
// Z_FreeTags
//
void
Z_FreeTags
(int		lowtag,
    int		hightag)
{
    memblock_t* block;
    memblock_t* next;

    // Validate heap before starting
    if (!mainzone) {
        I_Error("Z_FreeTags: mainzone is NULL!");
    }

    block = mainzone->blocklist.next;

    // Check if the first block pointer is valid
    if ((uintptr_t)block == 0xFFFFFFFFFFFFFFE7 ||
        (uintptr_t)block < 0x1000 ||
        block == (memblock_t*)0xCDCDCDCD ||
        block == (memblock_t*)0xDEADBEEF) {
        printf("Z_FreeTags: CORRUPTED HEAP DETECTED!\n");
        printf("  mainzone = %p\n", mainzone);
        printf("  blocklist.next = %p (INVALID!)\n", block);
        printf("  blocklist.prev = %p\n", mainzone->blocklist.prev);
        printf("\n");
        printf("DIAGNOSIS: The heap linked list has been corrupted.\n");
        printf("This usually means:\n");
        printf("1. Structure size mismatch (32-bit vs 64-bit compilation)\n");
        printf("2. Buffer overflow wrote over heap metadata\n");
        printf("3. sizeof(memblock_t) is different across files\n");
        printf("\n");
        printf("CRITICAL: Verify all files compiled with same architecture!\n");
        printf("sizeof(memblock_t) = %zu\n", sizeof(memblock_t));
        printf("sizeof(void*) = %zu\n", sizeof(void*));
        I_Error("Z_FreeTags: Heap corruption detected");
    }

    for (; block != &mainzone->blocklist; )
    {
        // Validate current block before accessing it
        if ((uintptr_t)block < 0x1000) {
            printf("Z_FreeTags: Invalid block pointer: %p\n", block);
            I_Error("Z_FreeTags: Corrupted block pointer");
        }

        next = block->next;

        // Validate next pointer
        if ((uintptr_t)next < 0x1000 && next != &mainzone->blocklist) {
            printf("Z_FreeTags: Invalid next pointer: %p from block %p\n", next, block);
            I_Error("Z_FreeTags: Corrupted next pointer");
        }

        // free block?
        if (!block->user) {
            block = next;
            continue;
        }
        if (block->tag >= lowtag && block->tag <= hightag)
            Z_Free((byte*)block + sizeof(memblock_t));
        block = next;
    }
}



//
// Z_DumpHeap
// Note: TFileDumpHeap( stdout ) ?
//
void
Z_DumpHeap
(int		lowtag,
    int		hightag)
{
    memblock_t* block;

    printf("zone size: %i  location: %p\n",
        mainzone->size, mainzone);

    printf("tag range: %i to %i\n",
        lowtag, hightag);

    for (block = mainzone->blocklist.next; ; block = block->next)
    {
        if (block->tag >= lowtag && block->tag <= hightag)
            printf("block:%p    size:%7i    user:%p    tag:%3i\n",
                block, block->size, block->user, block->tag);

        if (block->next == &mainzone->blocklist)
        {
            // all blocks have been hit
            break;
        }

        if ((byte*)block + block->size != (byte*)block->next)
            printf("ERROR: block size does not touch the next block\n");

        if (block->next->prev != block)
            printf("ERROR: next block doesn't have proper back link\n");

        if (!block->user && !block->next->user)
            printf("ERROR: two consecutive free blocks\n");
    }
}


//
// Z_FileDumpHeap
//
void Z_FileDumpHeap(FILE* f)
{
    memblock_t* block;

    fprintf(f, "zone size: %i  location: %p\n", mainzone->size, mainzone);

    for (block = mainzone->blocklist.next; ; block = block->next)
    {
        fprintf(f, "block:%p    size:%7i    user:%p    tag:%3i\n",
            block, block->size, block->user, block->tag);

        if (block->next == &mainzone->blocklist)
        {
            // all blocks have been hit
            break;
        }

        if ((byte*)block + block->size != (byte*)block->next)
            fprintf(f, "ERROR: block size does not touch the next block\n");

        if (block->next->prev != block)
            fprintf(f, "ERROR: next block doesn't have proper back link\n");

        if (!block->user && !block->next->user)
            fprintf(f, "ERROR: two consecutive free blocks\n");
    }
}



//
// Z_CheckHeap
//
void Z_CheckHeap(void)
{
    memblock_t* block;

    for (block = mainzone->blocklist.next; ; block = block->next)
    {
        if (block->next == &mainzone->blocklist)
        {
            // all blocks have been hit
            break;
        }

        if ((byte*)block + block->size != (byte*)block->next)
        {
            printf("Z_CheckHeap: block size does not touch the next block\n");
            printf("  block: %p size: %d next: %p\n", block, block->size, block->next);
            I_Error("Z_CheckHeap: block size does not touch the next block\n");
        }

        if (block->next->prev != block)
        {
            printf("Z_CheckHeap: next block doesn't have proper back link\n");
            printf("  block: %p size: %d next: %p next->prev: %p\n", block, block->size, block->next, block->next->prev);
            I_Error("Z_CheckHeap: next block doesn't have proper back link\n");
        }

        if (!block->user && !block->next->user)
        {
            printf("Z_CheckHeap: two consecutive free blocks\n");
            printf("  block: %p size: %d next: %p\n", block, block->size, block->next);
            I_Error("Z_CheckHeap: two consecutive free blocks\n");
        }
    }
}




//
// Z_ChangeTag
//
void
Z_ChangeTag2
(void* ptr,
    int		tag)
{
    memblock_t* block;

    if (!ptr)
    {
        printf("Z_ChangeTag: NULL pointer passed\n");
        I_Error("Z_ChangeTag: NULL pointer");
    }

    block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));

    if (tag >= PU_PURGELEVEL && (unsigned)block->user < 0x100)
    {
        printf("Z_ChangeTag: an owner is required for purgable blocks\n");
        printf("  ptr=%p block=%p\n", ptr, block);
        printf("  tag=%d (>= PU_PURGELEVEL)\n", tag);
        printf("  block->user=%p (< 0x100)\n", block->user);
        printf("  block->size=%d\n", block->size);
        printf("  block->tag=%d\n", block->tag);
        I_Error("Z_ChangeTag: an owner is required for purgable blocks");
    }

    block->tag = tag;
}



//
// Z_FreeMemory
//
int Z_FreeMemory(void)
{
    memblock_t* block;
    int			free;

    free = 0;

    for (block = mainzone->blocklist.next;
        block != &mainzone->blocklist;
        block = block->next)
    {
        if (!block->user || block->tag >= PU_PURGELEVEL)
            free += block->size;
    }
    return free;
}