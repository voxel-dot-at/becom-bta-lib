#include "timing_helper.h"
//#include <stdio.h>
#include <time.h>
#include <stdint.h>

#ifdef PLAT_WINDOWS
#   define PTW32_TIMESPEC_TO_FILETIME_OFFSET (((LONGLONG)27111902 << 32) + (LONGLONG)3577643008)
#   include <Windows.h>
#   include <chrono>
#   include <thread>
//#include <io.h>
//#include <fcntl.h>
//#include <share.h>
//#include <sys\stat.h>
#elif defined PLAT_LINUX
//#   include <sys/ioctl.h>
#include <unistd.h>
#endif


void BTAmsleep(uint32_t milliseconds) {
    if (milliseconds == 0) {
        return;
    }
#   ifdef PLAT_WINDOWS
    Sleep(milliseconds);
#   elif defined PLAT_LINUX
    while (milliseconds > 100000) {
        sleep(1);
        milliseconds -= 1000;
    }
    usleep(1000 * milliseconds);
#   endif
}


//void BTAusleep(int32_t microseconds) {
//    if (microseconds == 0) {
//        return;
//    }
//#   ifdef PLAT_WINDOWS
//    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
//    //nanosleep();
//    //HANDLE timer;
//    //LARGE_INTEGER ft;
//    //ft.QuadPart = -(10 * microseconds); // Convert to 100 nanosecond interval, negative value indicates relative time
//    //timer = CreateWaitableTimer(NULL, TRUE, NULL);
//    //SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
//    //WaitForSingleObject(timer, INFINITE);
//    //CloseHandle(timer);
//#   elif defined PLAT_LINUX
//    usleep(microseconds);
//#   endif
//}



///     @brief  Retrieves the number of milliseconds that have elapsed since the system was started, up to 49.7 days. (Windows)
///             Clock that cannot be set and represents monotonic time since some unspecified starting point. (Linux)
uint32_t BTAgetTickCount() {
#   ifdef PLAT_WINDOWS
    return GetTickCount();
#   elif defined PLAT_LINUX
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // what can I do?
        return 0;
    }
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#   endif
}



///     @brief  Retrieves the number of milliseconds that have elapsed since the system was started, up to 49.7 days. (Windows)
///             Clock that cannot be set and represents monotonic time since some unspecified starting point. (Linux)
uint64_t BTAgetTickCount64() {
#   ifdef PLAT_WINDOWS
    return GetTickCount64();
#   elif defined PLAT_LINUX
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // what can I do?
        return 0;
    }
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#   endif
}



///     @brief  Retrieves the number of nanoeconds that have elapsed since the system was started, up to 49.7 days. (Windows)
///             Clock that cannot be set and represents monotonic time since some unspecified starting point. (Linux)
uint64_t BTAgetTickCountNano() {
#   ifdef PLAT_WINDOWS
    LARGE_INTEGER frequency, start;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    return (uint64_t)((double)(start.QuadPart) / frequency.QuadPart * 1000000000);
#   elif defined PLAT_LINUX
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // what can I do?
        return 0;
    }
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
#   endif
}


BTA_Status BTAgetTime(uint64_t *seconds, uint32_t *nanoseconds) {
    struct timespec ts = { 0 };
    BTA_Status status = BTAgetTimeSpec(&ts);
    if (status != BTA_StatusOk) {
        return status;
    }
    if (seconds) {
        *seconds = ts.tv_sec;
    }
    if (nanoseconds) {
        *nanoseconds = (uint32_t)ts.tv_nsec;
    }
    return BTA_StatusOk;
}


BTA_Status BTAgetTimeSpec(struct timespec *ts) {
#   ifdef PLAT_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ts->tv_sec = (*(LONGLONG *)(&ft) - PTW32_TIMESPEC_TO_FILETIME_OFFSET) / 10000000;
    ts->tv_nsec = (int)((*(LONGLONG *)(&ft) - PTW32_TIMESPEC_TO_FILETIME_OFFSET - ((LONGLONG)ts->tv_sec * (LONGLONG)10000000)) * 100);
    return BTA_StatusOk;
#   elif defined PLAT_LINUX
    if (clock_gettime(CLOCK_REALTIME, ts) != 0) {
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
#   endif
}