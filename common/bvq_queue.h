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


BTA_Status BVQinit(uint32_t queueLength, BTA_QueueMode queueMode, BVQ_QueueHandle *handle);
BTA_Status BVQclose(BVQ_QueueHandle *handle, BTA_Status(*freeItem)(void **));
BTA_Status BVQclear(BVQ_QueueHandle handle, BTA_Status(*freeItem)(void **));
uint32_t BVQgetCount(BVQ_QueueHandle handle);
uint32_t BVQgetLength(BVQ_QueueHandle handle);
//BTA_Status BVQgetItem(BVQ_QueueHandle handle, uint32_t index, void **item);
BTA_Status BVQenqueue(BVQ_QueueHandle handle, void *item, BTA_Status(*freeItem)(void **));
BTA_Status BVQpeek(BVQ_QueueHandle handle, void **item, uint32_t timeout);
BTA_Status BVQdequeue(BVQ_QueueHandle handle, void **item, uint32_t timeout);
BTA_Status BVQgetList(BVQ_QueueHandle handle, void ***list, uint32_t *listLen);

#endif