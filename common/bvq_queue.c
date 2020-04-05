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
} BVQ_QueueInst;



BTA_Status BVQinit(uint32_t queueLength, BTA_QueueMode queueMode, BVQ_QueueHandle *handle) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    if (queueLength == 0 || queueMode == BTA_QueueModeDoNotQueue) {
        *handle = 0;
        return BTA_StatusOk;
    }
    BVQ_QueueInst *inst = (BVQ_QueueInst *)malloc(sizeof(BVQ_QueueInst));
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    BTAinitSemaphore(&inst->semQueueCount, 0, 0);
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
    *handle = inst;
    return BTA_StatusOk;
}


BTA_Status BVQclose(BVQ_QueueHandle *handle, BTA_Status(*freeItem)(void **)) {
    int err;
    BVQ_QueueInst **inst = (BVQ_QueueInst **)handle;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (!*inst) {
        return BTA_StatusOk;
    }
    BTA_Status status = BVQclear(*inst, freeItem);
    if (status != BTA_StatusOk) {
        return status;
    }
    err = BTAcloseMutex((*inst)->queueMutex);
    if (err != 0) {
        return BTA_StatusRuntimeError;
    }
    free((*inst)->queue);
    free(*inst);
    *inst = 0;
    return BTA_StatusOk;
}


BTA_Status BVQclear(BVQ_QueueHandle handle, BTA_Status(*freeItem)(void **)) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTAlockMutex(inst->queueMutex);
    if (freeItem) {
        while (inst->queueCount) {
            int32_t index = inst->queuePos - inst->queueCount + 1;
            if (index < 0) {
                index += inst->queueLength;
            }
            (*freeItem)(&inst->queue[index]);
            BTAwaitSemaphore(inst->semQueueCount);
            inst->queueCount--;
        }
    }
    BTAunlockMutex(inst->queueMutex);
    return BTA_StatusOk;
}


uint32_t BVQgetLength(BVQ_QueueHandle handle) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
        return 0;
    }
    return inst->queueLength;
}


uint32_t BVQgetCount(BVQ_QueueHandle handle) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
        return 0;
    }
    return inst->queueCount;
}


BTA_Status BVQenqueue(BVQ_QueueHandle handle, void *item, BTA_Status(*freeItem)(void **)) {
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
            if (freeItem) {
                (*freeItem)(&inst->queue[inst->queuePos]);
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
            if (freeItem) {
                (*freeItem)(&item);
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


BTA_Status BVQpeek(BVQ_QueueHandle handle, void **item, uint32_t msecsTimeout) {
    // todo: use timed semaphore
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!item || !inst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t endTime = BTAgetTickCount() + msecsTimeout;
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
    } while (msecsTimeout == 0 || (uint32_t)BTAgetTickCount() <= endTime);
    *item = 0;
    return BTA_StatusTimeOut;
}


BTA_Status BVQdequeue(BVQ_QueueHandle handle, void **item, uint32_t msecsTimeout) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst) {
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
    if (item) {
        *item = inst->queue[index];
    }
    inst->queueCount--;
    BTAunlockMutex(inst->queueMutex);
    return BTA_StatusOk;
}


BTA_Status BVQgetList(BVQ_QueueHandle handle, void ***list, uint32_t *listLen) {
    BVQ_QueueInst *inst = (BVQ_QueueInst *)handle;
    if (!inst || !list || !listLen) {
        return BTA_StatusInvalidParameter;
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
