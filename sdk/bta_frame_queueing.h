/**  @file bta_frame_queueing.h
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

#include <bta.h>

#ifndef BTA_FRAME_QUEUEING_H_INCLUDED
#define BTA_FRAME_QUEUEING_H_INCLUDED


//BTA_Status BFQinit(uint32_t frameQueueLength, BTA_QueueMode frameQueueMode, BTA_FrameQueueInst **frameQueue);
//BTA_Status BFQclose(BTA_FrameQueueInst **inst);
//BTA_Status BFQgetCount(BTA_FrameQueueInst *inst, uint32_t *count);
////BTA_Status BFQgetFrame(BTA_FrameQueueInst *inst, uint32_t index, BTA_Frame **frame);
//
///*  @brief Enqueues an item
//*   @param inst     The instance of the queue
//*   @param frame    The item to enqueue
//*   @return BTA_StatusOk on success
//*           BTA_StatusOutOfMemory if mode set to BTA_QueueModeAvoidDrop and queue is full       */
//BTA_Status BFQenqueue(BTA_FrameQueueInst *inst, BTA_Frame *frame);
//BTA_Status BFQdequeue(BTA_FrameQueueInst *inst, BTA_Frame **frame, uint32_t timeout);
//BTA_Status BFQclear(BTA_FrameQueueInst *inst);

#endif