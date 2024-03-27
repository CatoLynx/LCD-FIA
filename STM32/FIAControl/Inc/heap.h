#pragma once

#include <stddef.h>
#include <stdint.h>

#define HEAP_SIZE (200 * 1024)

void heapInit();
void* _malloc(size_t size);
void _free(void* ptr);
// void heapDump();