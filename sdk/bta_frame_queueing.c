/**  @file bta_frame_queueing.c
*  
*    @brief Support for queueing of frames
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

#include "bta_frame_queueing.h"
#include <bta_oshelper.h>
#include <bvq_queue.h>
#include <stdlib.h>
#include <assert.h>



BTA_Status BTA_CALLCONV BFQinit(uint32_t frameQueueLength, BTA_QueueMode frameQueueMode, BFQ_FrameQueueHandle *handle) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    BVQ_QueueHandle handleTemp;
    BTA_Status status = BVQinit(frameQueueLength, frameQueueMode, &handleTemp);
    if (status == BTA_StatusOk) {
        *handle = handleTemp;
    }
    return status;
}


BTA_Status BTA_CALLCONV BFQclose(BFQ_FrameQueueHandle *handle) {
    return BVQclose(handle, (BTA_Status(*)(void **))&BTAfreeFrame);
}

BTA_Status BTA_CALLCONV BFQgetCount(BFQ_FrameQueueHandle handle, uint32_t *count) {
    *count = BVQgetCount(handle);
    return BTA_StatusOk;
}

BTA_Status BTA_CALLCONV BFQenqueue(BFQ_FrameQueueHandle handle, BTA_Frame *frame) {
    return BVQenqueue(handle, frame, (BTA_Status(*)(void **))&BTAfreeFrame);
}

BTA_Status BTA_CALLCONV BFQdequeue(BFQ_FrameQueueHandle handle, BTA_Frame **frame, uint32_t msecsTimeout) {
    return BVQdequeue(handle, (void **)frame, msecsTimeout);
}

BTA_Status BTA_CALLCONV BFQclear(BFQ_FrameQueueHandle handle) {
    return BVQclear(handle, (BTA_Status(*)(void **))&BTAfreeFrame);
}