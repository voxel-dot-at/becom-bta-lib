#ifndef MEMORY_AREA_H_INCLUDED
#define MEMORY_AREA_H_INCLUDED

#include <bta_status.h>
#include <stdint.h>

typedef struct BTA_MemoryArea {
    void *p;
    uint32_t l;
} BTA_MemoryArea;

BTA_Status BTAinitMemoryArea(BTA_MemoryArea **memoryArea, int size);
BTA_Status BTAinitMemoryAreaPreallocated(BTA_MemoryArea **memoryArea, void *buffer, int size);
BTA_Status BTAfreeMemoryArea(BTA_MemoryArea **memoryArea);
#endif
