/**  @file bvq_queue.c
*
*    @brief Implementation of bvq_queue.h
*
*    BLT_DISCLAIMER
*
*    @author Alex Falkensteiner
*
*    @cond svn
*
*    Information of last commit
*    $Rev::               $:  Revision of last commit
*    $Author::            $:  Author of last commit
*    $Date::              $:  Date of last commit
*
*    @endcond
*/

#include "bvq_queue.h"
#include "bta_oshelper.h"
#include "pthread_helper.h"
#include "timing_helper.h"
#include <stdlib.h>
#include <assert.h>



typedef struct BVQ_QueueInst {
    uint32_t queueLength;      ///< length of queue
    BTA_QueueMode queueMode;
    void **queue;
    uint32_t queuePos;         ///< index of newest valid item (0 >= queuePos < queueLength)
    uint32_t queueCount;       ///< count of valid items in  queue (from index queuePos backwards) (0 >= queueCount <= queueLength)
    void *semQueueCount;
    void *queueMutex;
    FN_FreeItem freeItem;
} BVQ_QueueInst;



BTA_Status BTA_CALLCONV BVQinit(uint32_t queueLength, BTA_QueueMode queueMode, FN_FreeItem freeItem, BVQ_QueueHandle *handle) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    if (queueLength == 0 || queueMode == BTA_QueueModeDoNotQueue) {
        *handle = 0;
        return BTA_StatusOk;
    }
    BVQ_QueueInst *inst = (BVQ_QueueInst *)calloc(1, sizeof(BVQ_QueueInst));
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    BTA_Status status = BTAinitSemaphore(&inst->semQueueCount, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    inst->queuePos = 0;
    inst->queueCount = 0;
    inst->queueLength = queueLength;
    inst->queueMode = queueMode;
    inst->queue = (void **)malloc(inst->queueLength * sizeof(void *));
    if (!inst->queue) {
        free(inst);
        return BTA_StatusOutOfMemory;
    }
    int result = BTAinitMutex(&inst->queueMutex);
    if (result < 0 || !inst->queueMutex) {
        free(inst->queue);
        inst->queue = 0;
        free(inst);
        inst = 0;
        return BTA_StatusRuntimeError;
    }
    inst->freeItem = freeItem;
    *handle = inst;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BVQclose(BVQ_QueueHandle *handle) {
    int err;
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    BVQ_QueueInst *inst = (BVQ_QueueInst *)*handle;
    if (!inst) {
        return BTA_StatusOk;
    }
    BTA_Status status = BVQclear(inst);
    if (status != BTA_StatusOk) {
        return status;
    }
    err = BTAcloseMutex(inst->queueMutex);
    if (err != 0) {
        return BTA_StatusRuntimeError;
    }
    err = BTAcloseSemaphore(inst->semQueueCount);
    if (err != 0) {
        return BTA_StatusRuntimeError;
    }
    free(inst->queue);
    free(inst);
    *handle = 0;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BVQclear(BVQ_QueueHandle handle) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTAlockMutex(inst->queueMutex);
    if (inst->freeItem) {
        while (inst->queueCount) {
            int32_t index = inst->queuePos - inst->queueCount + 1;
            if (index < 0) {
                index += inst->queueLength;
            }
            (*inst->freeItem)(&inst->queue[index]);
            BTAwaitSemaphore(inst->semQueueCount);
            inst->queueCount--;
        }
    }
    BTAunlockMutex(inst->queueMutex);
    return BTA_StatusOk;
}


uint32_t BTA_CALLCONV BVQgetLength(BVQ_QueueHandle handle) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
        return 0;
    }
    return inst->queueLength;
}


uint32_t BTA_CALLCONV BVQgetCount(BVQ_QueueHandle handle) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
        return 0;
    }
    return inst->queueCount;
}


BTA_Status BTA_CALLCONV BVQenqueue(BVQ_QueueHandle handle, void *item) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst || !item) {
        return BTA_StatusInvalidParameter;
    }
    if (inst->queueMode == BTA_QueueModeDropOldest) {
        BTAlockMutex(inst->queueMutex);
        inst->queuePos++;
        if (inst->queuePos == inst->queueLength) {
            inst->queuePos = 0;
        }
        if (inst->queueCount == inst->queueLength) {
            if (inst->freeItem) {
                (*inst->freeItem)(&inst->queue[inst->queuePos]);
            }
            inst->queue[inst->queuePos] = item;
        }
        else {
            inst->queue[inst->queuePos] = item;
            BTApostSemaphore(inst->semQueueCount);
            inst->queueCount++;
        }
        BTAunlockMutex(inst->queueMutex);
        return BTA_StatusOk;
    }

    if (inst->queueMode == BTA_QueueModeDropCurrent) {
        BTAlockMutex(inst->queueMutex);
        if (inst->queueCount < inst->queueLength) {
            // there is enough space
            inst->queuePos++;
            if (inst->queuePos == inst->queueLength) {
                inst->queuePos = 0;
            }
            inst->queue[inst->queuePos] = item;
            BTApostSemaphore(inst->semQueueCount);
            inst->queueCount++;
        }
        else {
            if (inst->freeItem) {
                (*inst->freeItem)(&item);
            }
        }
        BTAunlockMutex(inst->queueMutex);
        return BTA_StatusOk;
    }

    if (inst->queueMode == BTA_QueueModeAvoidDrop) {
        BTAlockMutex(inst->queueMutex);
        if (inst->queueCount < inst->queueLength) {
            // there is enough space
            inst->queuePos++;
            if (inst->queuePos == inst->queueLength) {
                inst->queuePos = 0;
            }
            inst->queue[inst->queuePos] = item;
            BTApostSemaphore(inst->semQueueCount);
            inst->queueCount++;
            BTAunlockMutex(inst->queueMutex);
            return BTA_StatusOk;
        }
        BTAunlockMutex(inst->queueMutex);
        return BTA_StatusOutOfMemory;
    }

    // unreachable
    return BTA_StatusNotSupported;
}


BTA_Status BTA_CALLCONV BVQpeek(BVQ_QueueHandle handle, void **item, uint32_t msecsTimeout) {
    // todo: use timed semaphore
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!item || !inst) {
        return BTA_StatusInvalidParameter;
    }
    if (inst->queueMode == BTA_QueueModeDropOldest && inst->freeItem) {
        // Peek is not allowed if the void pointer could be dropped at any time
        return BTA_StatusIllegalOperation;
    }
    uint64_t endTime = BTAgetTickCount64() + msecsTimeout;
    do {
        BTAlockMutex(inst->queueMutex);
        if (inst->queueCount) {
            int32_t index = inst->queuePos - inst->queueCount + 1;
            if (index < 0) {
                index += inst->queueLength;
            }
            *item = inst->queue[index];
            BTAunlockMutex(inst->queueMutex);
            return BTA_StatusOk;
        }
        BTAunlockMutex(inst->queueMutex);
        BTAmsleep(10);
    } while (msecsTimeout == 0 || (uint64_t)BTAgetTickCount64() <= endTime);
    *item = 0;
    return BTA_StatusTimeOut;
}


BTA_Status BTA_CALLCONV BVQdequeue(BVQ_QueueHandle handle, void **item, uint32_t msecsTimeout) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
        if (item) *item = 0;
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status = BTAwaitSemaphoreTimed(inst->semQueueCount, msecsTimeout);
    if (status != BTA_StatusOk) {
        if (item) *item = 0;
        return status;
    }
    BTAlockMutex(inst->queueMutex);
    int32_t index = inst->queuePos - inst->queueCount + 1;
    if (index < 0) {
        index += inst->queueLength;
    }
    if (item) *item = inst->queue[index];
    inst->queueCount--;
    BTAunlockMutex(inst->queueMutex);
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BVQgetList(BVQ_QueueHandle handle, void ***list, uint32_t *listLen) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst || !list || !listLen) {
        return BTA_StatusInvalidParameter;
    }
    if (inst->queueMode == BTA_QueueModeDropOldest && inst->freeItem) {
        // getList is not allowed if the void pointer could be dropped at any time
        return BTA_StatusIllegalOperation;
    }
    BTAlockMutex(inst->queueMutex);
    *listLen = inst->queueCount;
    *list = (void **)malloc(*listLen * sizeof(void *));
    uint32_t index = inst->queuePos - inst->queueCount + 1 + inst->queueLength;
    if (index >= inst->queueLength) {
        index -= inst->queueLength;
    }
    for (uint32_t i = 0; i < inst->queueCount; i++) {
        (*list)[i] = inst->queue[index++];
        if (index == inst->queueLength) {
            index = 0;
        }
    }
    BTAunlockMutex(inst->queueMutex);
    return BTA_StatusOk;
}
