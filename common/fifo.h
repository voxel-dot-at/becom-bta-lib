#ifndef FIFO_H
#define FIFO_H

#include <pthread.h>
#include <stdint.h>

typedef struct
{
    pthread_mutexattr_t attr;
    pthread_mutex_t     mutex;
    uint32_t            head_index;
    uint32_t            tail_index;
    uint32_t            fifo_length;
    uint32_t            max_data_lenght;
} fifo_t;

uint32_t getSize(fifo_t *inst);
int32_t fifo_push_buffer(fifo_t *p_inst, uint32_t offset);
int32_t fifo_get_buffer(fifo_t *p_inst, uint32_t *p_offset);

#endif