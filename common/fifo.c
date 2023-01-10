#include "fifo.h"
#include <stdint.h>
#include <stdio.h>


uint32_t getSize(fifo_t *inst) {
    return sizeof(fifo_t) + inst->fifo_length * sizeof(uint32_t);
}


int32_t fifo_push_buffer(fifo_t *p_inst, uint32_t offset) {
    uint32_t *p = (uint32_t *)((uint8_t *)p_inst + sizeof(fifo_t) + p_inst->head_index * sizeof(uint32_t));
    *p = offset;
    p_inst->head_index++;
    p_inst->head_index %= p_inst->fifo_length;
    return 0;
}


int32_t fifo_get_buffer(fifo_t *p_inst, uint32_t *p_offset) {
    uint32_t *p = (uint32_t *)((uint8_t *)p_inst + sizeof(fifo_t) + p_inst->tail_index * sizeof(uint32_t));
    *p_offset = *p;
    p_inst->tail_index++;
    p_inst->tail_index %= p_inst->fifo_length;
    return 0;
}
