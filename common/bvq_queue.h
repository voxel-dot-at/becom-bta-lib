/**  @file bvq_queue.h
*
*    @brief Support for queueing of void*
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

// TODO: remove dependency!!!
#include <bta.h>

#ifndef BVQ_QUEUE_H_INCLUDED
#define BVQ_QUEUE_H_INCLUDED

typedef void* BVQ_QueueHandle;

typedef BTA_Status(BTA_CALLCONV *FN_FreeItem)(void** item);

BTA_Status BTA_CALLCONV BVQinit(uint32_t queueLength, BTA_QueueMode queueMode, FN_FreeItem freeItem, BVQ_QueueHandle *handle);
BTA_Status BTA_CALLCONV BVQclose(BVQ_QueueHandle *handle);
BTA_Status BTA_CALLCONV BVQclear(BVQ_QueueHandle handle);
uint32_t BTA_CALLCONV BVQgetCount(BVQ_QueueHandle handle);
uint32_t BTA_CALLCONV BVQgetLength(BVQ_QueueHandle handle);
//BTA_Status BTA_CALLCONV BVQgetItem(BVQ_QueueHandle handle, uint32_t index, void **item);
BTA_Status BTA_CALLCONV BVQenqueue(BVQ_QueueHandle handle, void *item);
BTA_Status BTA_CALLCONV BVQpeek(BVQ_QueueHandle handle, void **item, uint32_t timeout);
BTA_Status BTA_CALLCONV BVQdequeue(BVQ_QueueHandle handle, void **item, uint32_t timeout);
BTA_Status BTA_CALLCONV BVQgetList(BVQ_QueueHandle handle, void ***list, uint32_t *listLen);

#endif