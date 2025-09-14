#include <mymalloc.h>
#include <stdio.h>
#include <stdint.h>

// Function declarations
freeBlock *find_previous_block(freeBlock *block);
freeBlock *find_next_block(freeBlock *block);
int can_merge(freeBlock *block1, freeBlock *block2);
int is_valid_block(freeBlock *block);
int is_block_in_free_list(freeBlock *block);
void merge_blocks(freeBlock *block1, freeBlock *block2);
void insert_into_free_list(freeBlock *block);

spinlock_t big_lock = {ATOMIC_VAR_INIT(0)};
static spinlock_t init_lock = {ATOMIC_VAR_INIT(0)};
static atomic_int initialized = ATOMIC_VAR_INIT(0);
size_t heap_size = 4096*10;
static freeBlock *free_list_head = NULL;
static void *heap_start = NULL;
atomic_long malloc_count = ATOMIC_VAR_INIT(0);

freeBlock *find_free_block(size_t size) {
    freeBlock *current = free_list_head;
    while (current) {
        if (current->size >= size + sizeof(freeBlock)) {
            break;
        }
        current = current->next;
    }
    return current;
}

void split_block(freeBlock *block, size_t size) {
    // If the block is larger than needed, split it
    if (block->size > size + 2 * sizeof(freeBlock)) {
        // Create a new free block for the remaining space
        freeBlock *new_block = (freeBlock *)((char *)block + sizeof(freeBlock) + size);
        new_block->size = block->size - size - sizeof(freeBlock);
        new_block->prev = block->prev;
        new_block->next = block->next;
        
        // Replace the original block with the new block in the free list
        if (block->prev) {
            block->prev->next = new_block;
        } else {
            free_list_head = new_block;
        }
        if (block->next) {
            block->next->prev = new_block;
        }
        
        // Update the original block (now allocated)
        block->size = size + sizeof(freeBlock);
        block->prev = NULL;
        block->next = NULL;
    } else {
        // Remove the entire block from the free list (no splitting)
        if (block->prev) {
            block->prev->next = block->next;
        } else {
            free_list_head = block->next;
        }
        if (block->next) {
            block->next->prev = block->prev;
        }
        
        // Clear the block's pointers (now allocated)
        block->prev = NULL;
        block->next = NULL;
    }
}

void *find_address(size_t size) {
    freeBlock *block = find_free_block(size);
    if (block) {
        split_block(block, size);
        void *result = (void *)((char *)block + sizeof(freeBlock));
        // Ensure 8-byte alignment of returned address
        uintptr_t addr = (uintptr_t)result;
        if (addr & 7) {
            // This should not happen if our heap and structures are properly aligned
            return NULL;
        }
        return result;
    }
    return NULL;
}

void *mymalloc(size_t size) {
    // Count all malloc attempts (including size 0)
    atomic_fetch_add(&malloc_count, 1);
    
    // Ensure 8-byte alignment
    if (size == 0) return NULL;
    
    // Check for overflow and unreasonably large sizes
    if (size > heap_size || size > SIZE_MAX - sizeof(freeBlock) - 8) {
        return NULL;
    }
    
    size = (size + 7) & ~7;  // Round up to 8-byte boundary
    
    // Thread-safe initialization using double-checked locking
    if (atomic_load(&initialized) == 0) {
        spin_lock(&init_lock);
        if (atomic_load(&initialized) == 0) {
#ifndef FREESTANDING
            heap_start = vmalloc(NULL, heap_size);
            if (heap_start == NULL) {
                spin_unlock(&init_lock);
                return NULL;
            }
            // printf("initial address=%p\n", heap_start);
            free_list_head = (freeBlock *)heap_start;
            free_list_head->size = heap_size;
            free_list_head->prev = NULL;
            free_list_head->next = NULL;
#else
            // In freestanding mode, use static memory
            static char static_heap[4096 * 10] __attribute__((aligned(8)));
            heap_start = static_heap;
            free_list_head = (freeBlock *)static_heap;
            free_list_head->size = sizeof(static_heap);
            free_list_head->prev = NULL;
            free_list_head->next = NULL;
#endif
            atomic_store(&initialized, 1);
        }
        spin_unlock(&init_lock);
    }

    spin_lock(&big_lock);
    void *addr = find_address(size);
    // printf("malloc_count=%ld\n", atomic_load(&malloc_count));
    spin_unlock(&big_lock);

    return addr;
}

void myfree(void *ptr) {
    if(ptr == NULL) {
        return;
    }
    freeBlock *block = (freeBlock *)((char *)ptr - sizeof(freeBlock));
    if(!is_valid_block(block)) {
        return;
    }

    spin_lock(&big_lock);

    // Check for double free - if block is already in free list, just return
    if (is_block_in_free_list(block)) {
        spin_unlock(&big_lock);
        return; // Silent return on double free
    }

    // First, add the block to the free list
    insert_into_free_list(block);
    
    // Then try to coalesce with adjacent blocks
    freeBlock *prev = find_previous_block(block);
    if (prev && can_merge(prev, block)) {
        merge_blocks(prev, block);
        block = prev;  // Update block to point to the merged block
    }
    
    freeBlock *next = find_next_block(block);
    if (next && can_merge(block, next)) {
        merge_blocks(block, next);
    }

    spin_unlock(&big_lock);
}

freeBlock *find_previous_block(freeBlock *block) {
    freeBlock *current = free_list_head;
    freeBlock *result = NULL;
    while (current) {
        if((char *)(current) + current->size <= (char *)(block)) {
            result = current;
        }
        current = current->next;
    }
    return result;
}

freeBlock *find_next_block(freeBlock *block) {
    char *block_end = (char *)block + block->size;
    freeBlock *current = free_list_head;
    freeBlock *closest = NULL;
    
    while (current) {
        if ((char *)current >= block_end) {
            // 如果这是第一个找到的，或者比之前找到的更接近
            if (!closest || (char *)current < (char *)closest) {
                closest = current;
            }
        }
        current = current->next;
    }
    return closest;
}

int can_merge(freeBlock *block, freeBlock *next) {
    if((char *)(block) + block->size == (char *)(next)) {
        return 1;
    }
    return 0;
}

// Check if a block is already in the free list (for double-free detection)
int is_block_in_free_list(freeBlock *block) {
    freeBlock *current = free_list_head;
    while (current) {
        if (current == block) {
            return 1; // Found the block in free list
        }
        current = current->next;
    }
    return 0; // Block not in free list
}

int is_valid_block(freeBlock *block){
    // Basic validation: check if block pointer is reasonable
    if (!block) return 0;
    
    // Check if heap has been initialized
    if (!heap_start) return 0;
    
    uintptr_t block_addr = (uintptr_t)block;
    uintptr_t heap_start_addr = (uintptr_t)heap_start;
    uintptr_t heap_end_addr = heap_start_addr + heap_size;
    
    // Check if block is within heap bounds
    if (block_addr < heap_start_addr || block_addr >= heap_end_addr) return 0;
    
    // Check alignment
    if (block_addr % 8 != 0) return 0;  // Should be 8-byte aligned
    
    // Check if block size is reasonable (not zero, not too large)
    if (block->size == 0 || block->size > heap_size) return 0;
    
    // Check if the entire block (including its size) is within heap bounds
    if (block_addr + block->size > heap_end_addr) return 0;
    
    return 1;
}

void merge_blocks(freeBlock *block, freeBlock *next) {
    if((char *)(block) + block->size == (char *)(next)) {
        // Remove 'next' from free list first
        if (next->prev) {
            next->prev->next = next->next;
        } else {
            free_list_head = next->next;
        }
        if (next->next) {
            next->next->prev = next->prev;
        }
        
        // Now merge the sizes
        block->size += next->size;
    }
}

void insert_into_free_list(freeBlock *block) {
    freeBlock *prev = find_previous_block(block);
    freeBlock *next = find_next_block(block);
    
    // Insert at the beginning if no previous block
    if (!prev) {
        block->prev = NULL;
        block->next = free_list_head;
        if (free_list_head) {
            free_list_head->prev = block;
        }
        free_list_head = block;
        return;
    }
    
    // Insert between prev and next
    prev->next = block;
    block->prev = prev;
    block->next = next;
    if (next) {
        next->prev = block;
    }
}
