#ifndef TIMING_HELPER_H_INCLUDED
#define TIMING_HELPER_H_INCLUDED

#include <stdint.h>
#include <bta_status.h>
#include <time.h>

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif

void BTAmsleep(uint32_t milliseconds);
//void BTAusleep(int32_t microseconds);

uint64_t BTAgetTickCount64(void);
uint64_t BTAgetTickCountNano(void);
BTA_Status BTAgetTime(uint64_t *seconds, uint32_t *nanoseconds);
struct timespec; // why I don't know...
BTA_Status BTAgetTimeSpec(struct timespec *ts);

#endif