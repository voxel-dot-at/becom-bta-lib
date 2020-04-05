#ifndef TIMING_HELPER_H_INCLUDED
#define TIMING_HELPER_H_INCLUDED

#include <stdint.h>
#include <bta_status.h>

//#ifdef PLAT_WINDOWS
//#   include <time.h>
//#elif defined PLAT_LINUX
//#   include <pthread.h>
//#elif defined PLAT_APPLE
//#   include <time.h>
//#endif
#include <time.h>

void BTAmsleep(uint32_t milliseconds);
//void BTAusleep(int32_t microseconds);

uint32_t BTAgetTickCount(void);
uint64_t BTAgetTickCount64(void);
uint64_t BTAgetTickCountNano(void);
BTA_Status BTAgetTime(uint64_t *seconds, uint32_t *nanoseconds);
struct timespec; // why I don't know...
BTA_Status BTAgetTimeSpec(struct timespec *ts);

#endif