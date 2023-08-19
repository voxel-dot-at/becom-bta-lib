#pragma once

#include <bta_status.h>

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif


BTA_Status BTAinitMutex(void **mutex);
void BTAlockMutex(void *mutex);
void BTAunlockMutex(void *mutex);
BTA_Status BTAcloseMutex(void *mutex);

/*  @post must be joined with BTAjoinThread         */
BTA_Status BTAcreateThread(void **tid, void *(*runFunction) (void *), void *arg);
BTA_Status BTAjoinThread(void *tid);

BTA_Status BTAinitSemaphore(void **semaphore, int shared, int initValue);
void BTAwaitSemaphore(void *semaphore);
BTA_Status BTAwaitSemaphoreTimed(void *semaphore, int msecsTimeout);
void BTApostSemaphore(void *semaphore);
BTA_Status BTAcloseSemaphore(void *semaphore);