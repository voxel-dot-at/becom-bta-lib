#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <bta_status.h>

#include "circular_buffer.h"


// The definition of our circular buffer structure is hidden from the user
struct circular_buf_t {
    void **buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t max; //of the buffer
    bool full;
};


// static functions

static void advance_pointer(cbuf_handle_t cbuf) {
    assert(cbuf);
    if (cbuf->full) {
        cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    }
    cbuf->head = (cbuf->head + 1) % cbuf->max;
    // We mark full because we will advance tail on the next time around
    cbuf->full = (cbuf->head == cbuf->tail);
}


static void retreat_pointer(cbuf_handle_t cbuf) {
    assert(cbuf);
    cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    cbuf->full = false;
}


// public functions


 BTA_Status circular_buf_init(uint32_t size, cbuf_handle_t *handle) {
     if (!size || !handle) {
         return BTA_StatusInvalidParameter;
     }
     circular_buf_t *inst = (circular_buf_t *)malloc(sizeof(circular_buf_t));
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
    inst->full = false;
    *handle = inst;
    return BTA_StatusOk;
}


 BTA_Status circular_buf_free(cbuf_handle_t cbuf, BTA_Status(*freeItem)(void **)) {
    if (!cbuf) {
        return BTA_StatusOk;
    }
    BTA_Status status = circular_buf_reset(cbuf, freeItem);
    if (status != BTA_StatusOk) {
        return status;
    }
    free(cbuf->buffer);
    cbuf->buffer = 0;
    free(cbuf);
    cbuf = 0;
    return BTA_StatusOk;
}


BTA_Status circular_buf_reset(cbuf_handle_t cbuf, BTA_Status(*freeItem)(void **)) {
    if (!cbuf) {
        return BTA_StatusInvalidParameter;
    }
    if (freeItem) {
        while (circular_buf_size(cbuf)) {
            void *item;
            BTA_Status status = circular_buf_get(cbuf, &item);
            if (status != BTA_StatusOk) {
                return status;
            }
            (*freeItem)(&item);
        }
    }
    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->full = false;
    return BTA_StatusOk;
}


uint32_t circular_buf_capacity(cbuf_handle_t cbuf) {
    if (!cbuf) return 0;
    return cbuf->max;
}


uint32_t circular_buf_size(cbuf_handle_t cbuf) {
    if (!cbuf) return 0;
    if (cbuf->full) return cbuf->max;
    if (cbuf->head >= cbuf->tail) return cbuf->head - cbuf->tail;
    else return cbuf->max + cbuf->head - cbuf->tail;
}


uint8_t circular_buf_empty(cbuf_handle_t cbuf) {
    if (!cbuf) return 1;
    return (!cbuf->full && (cbuf->head == cbuf->tail));
}


uint8_t circular_buf_full(cbuf_handle_t cbuf) {
    if (!cbuf) return 0;
    return cbuf->full;
}


BTA_Status circular_buf_put(cbuf_handle_t cbuf, void *data) {
    if (!cbuf) return BTA_StatusInvalidParameter;
    if (circular_buf_full(cbuf)) {
        return BTA_StatusOutOfMemory;
    }
    cbuf->buffer[cbuf->head] = data;
    advance_pointer(cbuf);
    return BTA_StatusOk;
}


BTA_Status circular_buf_get(cbuf_handle_t cbuf, void **data) {
    if (!cbuf) return BTA_StatusInvalidParameter;
    if (circular_buf_empty(cbuf)) {
        return BTA_StatusOutOfMemory;
    }
    *data = cbuf->buffer[cbuf->tail];
    retreat_pointer(cbuf);
    return BTA_StatusOk;
}