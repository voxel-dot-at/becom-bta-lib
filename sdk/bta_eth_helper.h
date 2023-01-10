#ifndef BTA_WO_ETH

#ifndef BTA_ETH_HELPER_H_INCLUDED
#define BTA_ETH_HELPER_H_INCLUDED

#include <bta.h>

typedef struct BTA_MemoryArea {
    void *p;
    uint32_t l;
} BTA_MemoryArea;

BTA_Status BTAinitMemoryArea(BTA_MemoryArea **memoryArea, int size);
BTA_Status BTAfreeMemoryArea(BTA_MemoryArea **memoryArea);
#endif

#endif