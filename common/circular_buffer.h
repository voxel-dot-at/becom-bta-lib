#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <stdbool.h>

/// Opaque circular buffer structure
typedef struct circular_buf_t circular_buf_t;

/// Handle type, the way users interact with the API
typedef circular_buf_t* cbuf_handle_t;

/// Pass in a storage buffer and size, returns a circular buffer handle
/// Requires: buffer is not NULL, size > 0
/// Ensures: cbuf has been created and is returned in an empty state
BTA_Status circular_buf_init(uint32_t size, cbuf_handle_t *handle);

/// Free a circular buffer structure
/// Requires: cbuf is valid and created by circular_buf_init
/// Does not free data buffer; owner is responsible for that
BTA_Status circular_buf_free(cbuf_handle_t cbuf, BTA_Status(*freeItem)(void **));

/// Reset the circular buffer to empty, head == tail. Data not cleared
/// Requires: cbuf is valid and created by circular_buf_init
BTA_Status circular_buf_reset(cbuf_handle_t cbuf, BTA_Status(*freeItem)(void **));

/// Check the capacity of the buffer
/// Requires: cbuf is valid and created by circular_buf_init
/// Returns the maximum capacity of the buffer
uint32_t circular_buf_capacity(cbuf_handle_t cbuf);

/// Check the number of elements stored in the buffer
/// Requires: cbuf is valid and created by circular_buf_init
/// Returns the current number of elements in the buffer
uint32_t circular_buf_size(cbuf_handle_t cbuf);

/// CHecks if the buffer is empty
/// Requires: cbuf is valid and created by circular_buf_init
/// Returns true if the buffer is empty
uint8_t circular_buf_empty(cbuf_handle_t cbuf);

/// Checks if the buffer is full
/// Requires: cbuf is valid and created by circular_buf_init
/// Returns true if the buffer is full
uint8_t circular_buf_full(cbuf_handle_t cbuf);

/// Put Version 2 rejects new data if the buffer is full
/// Requires: cbuf is valid and created by circular_buf_init
/// Returns 0 on success, -1 if buffer is full
BTA_Status circular_buf_put(cbuf_handle_t cbuf, void *data);

/// Retrieve a value from the buffer
/// Requires: cbuf is valid and created by circular_buf_init
/// Returns 0 on success, -1 if the buffer is empty
BTA_Status circular_buf_get(cbuf_handle_t cbuf, void **data);

#endif //CIRCULAR_BUFFER_H_
