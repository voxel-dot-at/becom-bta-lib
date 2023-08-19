#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <bta_status.h>
#include <pthread_helper.h>
#include "bcb_circular_buffer.h"


struct BCB_CircularBuffer {
    void **buffer;
    uint32_t volatile head;
    uint32_t volatile tail;
    uint32_t max;
    uint8_t volatile full;
    void *mutex;
};


static BTA_Status reset(BCB_Handle handle, BTA_Status(*freeItem)(void **));
static uint32_t getSize(BCB_Handle handle);
static BTA_Status get(BCB_Handle handle, void **data);


BTA_Status BCBinit(uint32_t size, BCB_Handle *handle) {
     if (!size || !handle) {
         return BTA_StatusInvalidParameter;
     }
     BCB_CircularBuffer *inst = (BCB_CircularBuffer *)calloc(1, sizeof(BCB_CircularBuffer));
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    inst->buffer = (void **)malloc(size * sizeof(void *));
    if (!inst->buffer) {
        free(inst);
        return BTA_StatusOutOfMemory;
    }
    inst->max = size;
    inst->head = 0;
    inst->tail = 0;
    inst->full = 0;
    BTA_Status status = BTAinitMutex(&inst->mutex);
    if (status != BTA_StatusOk) {
        free(inst->buffer);
        free(inst);
        return status;
    }
    *handle = inst;
    return BTA_StatusOk;
}


BTA_Status BCBfree(BCB_Handle handle, BTA_Status(*freeItem)(void **)) {
    if (!handle) {
        return BTA_StatusOk;
    }
    void *mutex = handle->mutex;
    BTAlockMutex(mutex);
    BTA_Status status = reset(handle, freeItem);
    if (status != BTA_StatusOk) {
        BTAunlockMutex(mutex);
        return status;
    }
    free(handle->buffer);
    handle->buffer = 0;
    free(handle);
    handle = 0;
    BTAunlockMutex(mutex);
    status = BTAcloseMutex(mutex);
    if (status != BTA_StatusOk) {
        return status;
    }
    return BTA_StatusOk;
}


static BTA_Status reset(BCB_Handle handle, BTA_Status(*freeItem)(void **)) {
     if (!handle) {
         return BTA_StatusInvalidParameter;
     }
     if (freeItem) {
         while (getSize(handle)) {
             void *item;
             BTA_Status status = get(handle, &item);
             if (status != BTA_StatusOk) {
                 return status;
             }
             (*freeItem)(&item);
         }
     }
     handle->head = 0;
     handle->tail = 0;
     handle->full = 0;
     return BTA_StatusOk;
}


BTA_Status BCBreset(BCB_Handle handle, BTA_Status(*freeItem)(void **)) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    BTAlockMutex(handle->mutex);
    if (freeItem) {
        while (getSize(handle)) {
            void *item;
            BTA_Status status = get(handle, &item);
            if (status != BTA_StatusOk) {
                BTAunlockMutex(handle->mutex);
                return status;
            }
            (*freeItem)(&item);
        }
    }
    handle->head = 0;
    handle->tail = 0;
    handle->full = 0;
    BTAunlockMutex(handle->mutex);
    return BTA_StatusOk;
}


uint32_t BCBgetCapacity(BCB_Handle handle) {
    if (!handle) return 0;
    return handle->max;
}


static uint32_t getSize(BCB_Handle handle) {
    if (!handle) return 0;
    if (handle->full) return handle->max;
    if (handle->head >= handle->tail) return handle->head - handle->tail;
    else return handle->max + handle->head - handle->tail;
}


uint32_t BCBgetSize(BCB_Handle handle) {
    if (!handle) return 0;
    BTAlockMutex(handle->mutex);
    if (handle->full) {
        BTAunlockMutex(handle->mutex);
        return handle->max;
    }
    uint32_t size = handle->head >= handle->tail ? handle->head - handle->tail : handle->max + handle->head - handle->tail;
    BTAunlockMutex(handle->mutex);
    return size;
}


uint8_t BCBisEmpty(BCB_Handle handle) {
    if (!handle) return 1;
    BTAlockMutex(handle->mutex);
    uint8_t result = (!handle->full && (handle->head == handle->tail));
    BTAunlockMutex(handle->mutex);
    return result;
}


uint8_t BCBisFull(BCB_Handle handle) {
    if (!handle) return 0;
    BTAlockMutex(handle->mutex);
    uint8_t result = handle->full;
    BTAunlockMutex(handle->mutex);
    return result;
}


BTA_Status BCBput(BCB_Handle handle, void *data) {
    if (!handle) return BTA_StatusInvalidParameter;
    BTAlockMutex(handle->mutex);
    if (handle->full) {
        BTAunlockMutex(handle->mutex);
        return BTA_StatusOutOfMemory;
    }
    handle->buffer[handle->head] = data;
    // advance pointer
    if (handle->full) {
        handle->tail = (handle->tail + 1) % handle->max;
    }
    handle->head = (handle->head + 1) % handle->max;
    // We mark full because we will advance tail on the next time
    handle->full = (handle->head == handle->tail);
    BTAunlockMutex(handle->mutex);
    return BTA_StatusOk;
}


static BTA_Status get(BCB_Handle handle, void **data) {
    if (!handle) return BTA_StatusInvalidParameter;
    if (!handle->full && (handle->head == handle->tail)) {
        return BTA_StatusOutOfMemory;
    }
    *data = handle->buffer[handle->tail];
    // retreat pointer
    handle->tail = (handle->tail + 1) % handle->max;
    handle->full = 0;
    return BTA_StatusOk;
}


BTA_Status BCBget(BCB_Handle handle, void **data) {
    if (!handle) return BTA_StatusInvalidParameter;
    BTAlockMutex(handle->mutex);
    if (!handle->full && (handle->head == handle->tail)) {
        BTAunlockMutex(handle->mutex);
        return BTA_StatusOutOfMemory;
    }
    *data = handle->buffer[handle->tail];
    // retreat pointer
    handle->tail = (handle->tail + 1) % handle->max;
    handle->full = 0;
    BTAunlockMutex(handle->mutex);
    return BTA_StatusOk;
}
