#ifndef BTA_WO_ETH

#include "bta_helper.h"
#include <bta_oshelper.h>
#include "bta_eth_helper.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

#ifdef PLAT_WINDOWS
#elif defined(PLAT_APPLE)
#   include <stdbool.h>
#endif



BTA_Status BTAinitMemoryArea(BTA_MemoryArea **memoryArea, int size) {
    BTA_MemoryArea *memoryAreaTemp = (BTA_MemoryArea *)malloc(sizeof(BTA_MemoryArea));
    if (!memoryAreaTemp) {
        return BTA_StatusOutOfMemory;
    }
    memoryAreaTemp->l = size;
    memoryAreaTemp->p = malloc(size);
    if (!memoryAreaTemp->p) {
        free(memoryAreaTemp);
        memoryAreaTemp = 0;
        return BTA_StatusOutOfMemory;
    }
    *memoryArea = memoryAreaTemp;
    return BTA_StatusOk;
}


BTA_Status BTAfreeMemoryArea(BTA_MemoryArea **memoryArea) {
    if (!memoryArea) {
        return BTA_StatusIllegalOperation;
    }
    if (!(*memoryArea)) {
        return BTA_StatusIllegalOperation;
    }
    free((*memoryArea)->p);
    (*memoryArea)->p = 0;
    free(*memoryArea);
    *memoryArea = 0;
    return BTA_StatusOk;
}

#endif