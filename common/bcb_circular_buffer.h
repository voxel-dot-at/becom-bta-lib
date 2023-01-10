#ifndef BCB_CIRCULAR_BUFFER_H
#define BCB_CIRCULAR_BUFFER_H


/// Opaque circular buffer structure
typedef struct BCB_CircularBuffer BCB_CircularBuffer;

/// Handle type, the way users interact with the API
typedef BCB_CircularBuffer* BCB_Handle;

/// Pass in a storage buffer and size, returns a circular buffer handle
/// Requires: buffer is not NULL, size > 0
/// Ensures: handle has been created and is returned in an empty state
BTA_Status BCBinit(uint32_t size, BCB_Handle *handle);

/// Free a circular buffer structure
/// Requires: handle is valid and created by circular_buf_init
/// Does not free data buffer; owner is responsible for that
BTA_Status BCBfree(BCB_Handle handle, BTA_Status(*freeItem)(void **));

/// Reset the circular buffer to empty, head == tail. Data not cleared
/// Requires: handle is valid and created by circular_buf_init
BTA_Status BCBreset(BCB_Handle handle, BTA_Status(*freeItem)(void **));

/// Check the capacity of the buffer
/// Requires: handle is valid and created by circular_buf_init
/// Returns the maximum capacity of the buffer
uint32_t BCBgetCapacity(BCB_Handle handle);

/// Check the number of elements stored in the buffer
/// Requires: handle is valid and created by circular_buf_init
/// Returns the current number of elements in the buffer
uint32_t BCBgetSize(BCB_Handle handle);

/// CHecks if the buffer is empty
/// Requires: handle is valid and created by circular_buf_init
/// Returns true if the buffer is empty
uint8_t BCBisEmpty(BCB_Handle handle);

/// Checks if the buffer is full
/// Requires: handle is valid and created by circular_buf_init
/// Returns true if the buffer is full
uint8_t BCBisFull(BCB_Handle handle);

/// Put Version 2 rejects new data if the buffer is full
/// Requires: handle is valid and created by circular_buf_init
/// Returns 0 on success, -1 if buffer is full
BTA_Status BCBput(BCB_Handle handle, void *data);

/// Retrieve a value from the buffer
/// Requires: handle is valid and created by circular_buf_init
/// Returns 0 on success, -1 if the buffer is empty
BTA_Status BCBget(BCB_Handle handle, void **data);

#endif
