///**  @file bta_byte_queue.h
//*
//*    @brief Support for queueing of bytes
//*
//*    BLT_DISCLAIMER
//*
//*    @author Alex Falkensteiner
//*
//*    @cond svn
//*
//*    Information of last commit
//*    $Rev::               $:  Revision of last commit
//*    $Author::            $:  Author of last commit
//*    $Date::              $:  Date of last commit
//*
//*    @endcond
//*/
//
//#include <bta.h>
//
//#ifndef BTA_BYTE_QUEUE_H_INCLUDED
//#define BTA_BYTE_QUEUE_H_INCLUDED
//
//
//typedef struct BTA_ByteQueueInst {
//    uint32_t queueLength;      ///< length of queue
//    uint8_t *queue;
//    uint32_t queuePos;         ///< index of newest valid frame (0 >= frameQueuePos < frameQueueLength)
//    uint32_t queueCount;       ///< count of valid frames in  queue (from index frameQueuePos backwards) (0 >= framwQueueCount <= frameQueueLength)
//    void *queueMutex;
//} BTA_ByteQueueInst;
//
//BTA_Status BBQinit(BTA_ByteQueueInst **inst, uint32_t queueLength);
//BTA_Status BBQclose(BTA_ByteQueueInst **inst);
//BTA_Status BBQgetCount(BTA_ByteQueueInst *inst, uint32_t *count);
//BTA_Status BBQenqueue(BTA_ByteQueueInst *inst, uint8_t *buffer, uint32_t bufferLen);
//BTA_Status BBQdequeue(BTA_ByteQueueInst *inst, uint8_t *buffer, uint32_t bufferLen, uint32_t timeout);
//
//#endif