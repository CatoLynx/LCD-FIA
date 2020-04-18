#pragma once

#include <stddef.h>
#include <stdint.h>

#define HEAP_SIZE (200 * 1024)

void heapInit();
void* malloc(size_t size);
void free(void* ptr);
// void heapDump();