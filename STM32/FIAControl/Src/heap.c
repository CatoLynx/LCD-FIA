#include "heap.h"

uint8_t _heap[HEAP_SIZE] = {0};

#define OFFSET(x) ((uint32_t)((uint8_t*)x) - (uint32_t)(_heapHead))

enum _heapFlag { USED = 0x00000001 };

struct _heapEntry_s {
    struct _heapEntry_s* prev;
    struct _heapEntry_s* next;
    uint32_t flags;
    size_t size;
    uint8_t data[0];
};

struct _heapHeader_s {
    struct _heapEntry_s* firstBlock;
    struct _heapEntry_s* lastBlock;
    uint8_t data[0];
};

typedef struct _heapHeader_s _heapHeader_t;
typedef struct _heapEntry_s _heapEntry_t;

_heapHeader_t* _heapHead;

void heapInit() {
    _heapHead = (_heapHeader_t*)_heap;
    _heapHead->firstBlock = (_heapEntry_t*)_heapHead->data;
    _heapHead->lastBlock = (_heapEntry_t*)_heapHead->data;

    _heapHead->firstBlock->next = NULL;
    _heapHead->firstBlock->prev = NULL;
    _heapHead->firstBlock->flags = 0x00000000;
    _heapHead->firstBlock->size = HEAP_SIZE - sizeof(_heapHeader_t) - sizeof(_heapEntry_t);
}

void* _malloc(size_t size) {
    size_t blockSize = (size & ~0x3) + 4;
    _heapEntry_t* block = _heapHead->firstBlock;
    while (block &&
           (block->flags & USED || (block->size != blockSize && block->size < blockSize + sizeof(_heapEntry_t)))) {
        block = block->next;
    }
    if (block == NULL) {
        return NULL;
    }

    if (block->size == blockSize) // Exact match: reuse free block as is.
    {
        block->flags = USED;
    } else {
        _heapEntry_t* nextPtr = block->next;
        size_t newFreeSize = block->size - (blockSize + sizeof(_heapEntry_t));

        block->size = blockSize;
        block->next = (_heapEntry_t*)((uint8_t*)block + block->size + sizeof(_heapEntry_t));
        block->next->prev = block;
        block->next->next = nextPtr;
        block->next->size = newFreeSize;
        block->next->flags = block->flags;

        block->flags = USED;

        if (nextPtr) {
            nextPtr->prev = block->next;
        } else {
            _heapHead->lastBlock = block->next;
        }
    }
    return block->data;
}

_heapEntry_t* _heapCombineBlocks(_heapEntry_t* blockA, _heapEntry_t* blockB) {
    blockA->next = blockB->next;
    blockA->size = blockA->size + blockB->size + sizeof(_heapEntry_t);
    if (blockA->next) {
        blockA->next->prev = blockA;
    } else {
        _heapHead->lastBlock = blockA;
    }
    return blockA;
}

void _free(void* ptr) {
    if (ptr == NULL) return;
    _heapEntry_t* block = (_heapEntry_t*)(ptr - offsetof(_heapEntry_t, data));
    block->flags = 0;
    while (block->prev && block->prev->flags == 0) {
        block = _heapCombineBlocks(block->prev, block);
    }

    while (block->next && block->next->flags == 0) {
        block = _heapCombineBlocks(block, block->next);
    }
}

/*void heapDump() {
    printf("\n\rDumping Heap\n\r\n\r");
    printf("Size is %u starting at 0x%08X\n\r", HEAP_SIZE, _heapHead);
    printf("First Block is at %u\n\r", OFFSET(_heapHead->firstBlock));
    printf("Last Block is at %u\n\r", OFFSET(_heapHead->lastBlock));

    printf("\n\rBlocks:\n\r");

    _heapEntry_t* block = _heapHead->firstBlock;
    while (block) {
        printf("\t%10u -> Prev: %10u, Next: %10u, Size: %10u, Flags: %08X\n\r", OFFSET(block), OFFSET(block->prev),
               OFFSET(block->next), block->size, block->flags);
        block = block->next;
    }
}*/
