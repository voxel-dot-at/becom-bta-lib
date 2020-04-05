#pragma once

#include <bta_status.h>


BTA_Status BTAinitMutex(void **mutex);
void BTAlockMutex(void *mutex);
void BTAunlockMutex(void *mutex);
BTA_Status BTAcloseMutex(void *mutex);

/*  @post must be joined with BTAjoinThread         */
BTA_Status BTAcreateThread(void **tid, void *(*runFunction) (void *), void *arg, int priority);
BTA_Status BTAsetRealTimePriority(void *tid);
BTA_Status BTAjoinThread(void *tid);

BTA_Status BTAinitSemaphore(void **semaphore, int shared, int initValue);
//int BTAgetSemaphoreValue(void *semaphore);
void BTAwaitSemaphore(void *semaphore);
BTA_Status BTAwaitSemaphoreTimed(void *semaphore, int msecsTimeout);
void BTApostSemaphore(void *semaphore);
BTA_Status BTAcloseSemaphore(void *semaphore);