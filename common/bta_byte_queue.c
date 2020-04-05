///**  @file bta_frame_queueing.c
//*
//*    @brief Support for queueing of frames
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
//#include <bta_byte_queue.h>
//#include <bta_oshelper.h>
//#include <stdlib.h>
//#include <assert.h>
//#include <string.h>
//
//
//BTA_Status BBQinit(BTA_ByteQueueInst **inst, uint32_t queueLength) {
//    int result;
//    BTA_ByteQueueInst *queueInst;
//    if (!inst) {
//        return BTA_StatusInvalidParameter;
//    }
//    if (!queueLength) {
//        *inst = 0;
//        return BTA_StatusInvalidParameter;
//    }
//    queueInst = (BTA_ByteQueueInst *)malloc(sizeof(BTA_ByteQueueInst));
//    if (!queueInst) {
//        return BTA_StatusOutOfMemory;
//    }
//    queueInst->queueCount = 0;
//    queueInst->queuePos = 0;
//    queueInst->queueLength = queueLength;
//    queueInst->queue = (uint8_t *)malloc(queueInst->queueLength);
//    if (!queueInst->queue) {
//        free(queueInst);
//        return BTA_StatusOutOfMemory;
//    }
//    result = BTAinitMutex(&queueInst->queueMutex);
//    if (result < 0 || !queueInst->queueMutex) {
//        free(queueInst->queue);
//        return BTA_StatusRuntimeError;
//    }
//    *inst = queueInst;
//    return BTA_StatusOk;
//}
//
//
//BTA_Status BBQclose(BTA_ByteQueueInst **inst) {
//    int err;
//    if (!inst) {
//        return BTA_StatusInvalidParameter;
//    }
//    if (!*inst) {
//        return BTA_StatusOk;
//    }
//    free((*inst)->queue);
//    err = BTAcloseMutex((*inst)->queueMutex);
//    if (err != 0) {
//        return BTA_StatusRuntimeError;
//    }
//    free(*inst);
//    *inst = 0;
//    return BTA_StatusOk;
//}
//
//
//BTA_Status BBQgetCount(BTA_ByteQueueInst *inst, uint32_t *count) {
//    if (!count || !inst) {
//        return BTA_StatusInvalidParameter;
//    }
//    *count = inst->queueCount;
//    return BTA_StatusOk;
//}
//
//
//BTA_Status BBQenqueue(BTA_ByteQueueInst *inst, uint8_t *buffer, uint32_t bufferLen) {
//    if (!inst || !buffer) {
//        return BTA_StatusInvalidParameter;
//    }
//
//    BTA_Status status;
//    BTAlockMutex(inst->queueMutex);
//    if (inst->queueCount + bufferLen <= inst->queueLength) {
//        // there is enough space
//        int32_t portion1 = min((inst->queueLength - inst->queuePos), bufferLen);
//        int32_t portion2 = bufferLen - portion1;
//        memcpy(inst->queue + inst->queuePos, buffer, portion1);
//        inst->queuePos += portion1;
//        inst->queueCount += portion1;
//        if (inst->queuePos == inst->queueLength) {
//            inst->queuePos = 0;
//        }
//        if (portion2) {
//            memcpy(inst->queue + inst->queuePos, buffer + portion1, portion2);
//            inst->queuePos += portion2;
//            inst->queueCount += portion2;
//        }
//        status = BTA_StatusOk;
//    }
//    else {
//        status = BTA_StatusOutOfMemory;
//    }
//    BTAunlockMutex(inst->queueMutex);
//    return status;
//}
//
//
//BTA_Status BBQdequeue(BTA_ByteQueueInst *inst, uint8_t *buffer, uint32_t bufferLen, uint32_t timeout) {
//    uint32_t endTime;
//    if (!buffer || !inst) {
//        return BTA_StatusInvalidParameter;
//    }
//    endTime = BTAgetTickCount() + timeout;
//    do {
//        BTAlockMutex(inst->queueMutex);
//        if (inst->queueCount >= bufferLen) {
//            // Data to write to disk is available
//            int32_t index = inst->queuePos - inst->queueCount;
//            if (index < 0) {
//                index += inst->queueLength;
//            }
//            int32_t portion1 = min(inst->queueCount, inst->queueLength - index);
//            int32_t portion2 = bufferLen - portion1;
//            memcpy(buffer, inst->queue + index, portion1);
//            inst->queueCount -= portion1;
//            if (portion2) {
//                memcpy(buffer + portion1, inst->queue, portion2);
//                inst->queueCount -= portion2;
//            }
//            BTAunlockMutex(inst->queueMutex);
//            return BTA_StatusOk;
//        }
//        else {
//            // Not enough in queue
//            BTAunlockMutex(inst->queueMutex);
//            return BTA_StatusOutOfMemory;
//        }
//        BTAmsleep(2);   // A lower value will not improve performance
//    } while (timeout == 0 || (uint32_t)BTAgetTickCount() <= endTime);
//    return BTA_StatusTimeOut;
//}
