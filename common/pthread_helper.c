#include "pthread_helper.h"
#include "timing_helper.h"
//#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#ifdef PLAT_WINDOWS
#   include <semaphore.h>
#else
#   include <semaphore.h>
#   include <errno.h>
#endif
#if defined PLAT_APPLE
#   include <dispatch/dispatch.h>
#endif


#define MARK_USED(x) ((void)(x))


BTA_Status BTAinitMutex(void **mutex) {
    pthread_mutex_t *mutexPosix = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
    if (!mutexPosix) {
        *mutex = 0;
        return BTA_StatusOutOfMemory;
    }
    int result = pthread_mutex_init(mutexPosix, NULL);
    if (result) {
        *mutex = 0;
        return BTA_StatusRuntimeError;
    }
    *mutex = mutexPosix;
    return BTA_StatusOk;
}


void BTAlockMutex(void *mutex) {
    assert(mutex);
    int result = pthread_mutex_lock((pthread_mutex_t *)mutex);
    assert(!result);
    MARK_USED(result);
    //if (!result) {
    //}
}


void BTAunlockMutex(void *mutex) {
    assert(mutex);
    int result = pthread_mutex_unlock((pthread_mutex_t *)mutex);
    assert(!result);
    MARK_USED(result);
    //if (!result) {
    //}
}


BTA_Status BTAcloseMutex(void *mutex) {
    if (!mutex) {
        // no mutex to close
        return BTA_StatusOk;
    }
    int result = pthread_mutex_destroy((pthread_mutex_t *)mutex);
    if (result) {
        //printf("pthread_mutex_destroy: %d", result);
        return BTA_StatusRuntimeError;
    }
    free(mutex);
    mutex = 0;
    return BTA_StatusOk;
}


BTA_Status BTAcreateThread(void **tid, void *(*runFunction) (void *), void *arg) {
    if (!tid) {
        return BTA_StatusRuntimeError;
    }

    *tid = 0;
    int result;
    pthread_attr_t *tattr = (pthread_attr_t *)0;
    pthread_t *tidPosix = (pthread_t *)malloc(sizeof(pthread_t));
    if (!tidPosix) {
        free(tattr);
        tattr = 0;
        return BTA_StatusOutOfMemory;
    }
    result = pthread_create(tidPosix, tattr, runFunction, arg);
    if (result) {
        free(tattr);
        tattr = 0;
        free(tidPosix);
        tidPosix = 0;
        return BTA_StatusRuntimeError;
    }
    *tid = tidPosix;
    return BTA_StatusOk;
}


BTA_Status BTAjoinThread(void *tid) {
    if (!tid) {
        // no thread to join
        return BTA_StatusOk;
    }
    int result = pthread_join(*((pthread_t *)tid), 0);
    if (result) {
        return BTA_StatusRuntimeError;
    }
    free(tid);
    tid = 0;
    return BTA_StatusOk;
}


BTA_Status BTAinitSemaphore(void **semaphore, int shared, int initValue) {
#   ifdef PLAT_APPLE
        *semaphore = dispatch_semaphore_create(initValue);
#   else
        sem_t *semaphorePosix = (sem_t *)calloc(1, sizeof(sem_t));
        if (!semaphorePosix) {
            *semaphore = 0;
            return BTA_StatusOutOfMemory;
        }
        int result = sem_init(semaphorePosix, shared, initValue);
        if (result) {
            *semaphore = 0;
            return BTA_StatusRuntimeError;
        }
        *semaphore = semaphorePosix;
#   endif
    return BTA_StatusOk;
}


void BTAwaitSemaphore(void *semaphore) {
    assert(semaphore);
#   ifdef PLAT_APPLE
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
#   else
        if (!semaphore) {
        }
        int result = sem_wait((sem_t *)semaphore);
        assert(!result);
        if (result) {
    }
#   endif
}


BTA_Status BTAwaitSemaphoreTimed(void *semaphore, int msecsTimeout) {
    assert(semaphore);
#   ifdef PLAT_APPLE
        long result = dispatch_semaphore_wait(semaphore, msecsTimeout);
        if (result) {
            return BTA_StatusTimeOut;
        }
#   else
        int result;
        if (!msecsTimeout) {
            result = sem_wait((sem_t *)semaphore);
        }
        else {
            struct timespec timeout = { 0 };
            BTAgetTimeSpec(&timeout);
            timeout.tv_sec += msecsTimeout / 1000;
            timeout.tv_nsec += (msecsTimeout % 1000) * 1000000;
            timeout.tv_sec += timeout.tv_nsec / 1000000000;
            timeout.tv_nsec %= 1000000000;
            result = sem_timedwait((sem_t *)semaphore, &timeout);
        }
        if (result != 0) {
#           ifdef PLAT_WINDOWS
            return BTA_StatusTimeOut;
#           elif defined PLAT_LINUX
            int err = errno;
            if (err == ETIMEDOUT) {
                return BTA_StatusTimeOut;
            }
            return BTA_StatusRuntimeError;
#           endif
        }
#   endif
    return BTA_StatusOk;
}


void BTApostSemaphore(void *semaphore) {
#   ifdef PLAT_APPLE
        dispatch_semaphore_signal(semaphore);
#   else
        assert(semaphore);
        int result = sem_post((sem_t *)semaphore);
        assert(!result);
        if (result) { // avoids compiler warning
        }
#   endif
}


BTA_Status BTAcloseSemaphore(void *semaphore) {
#   ifdef PLAT_APPLE
        dispatch_release(semaphore);
#   else
        if (!semaphore) {
            // no semReadPool to close
            return BTA_StatusOk;
        }
        int result = sem_destroy((sem_t *)semaphore);
        if (result) {
            return BTA_StatusRuntimeError;
        }
        free(semaphore);
#   endif
    semaphore = 0;
    return BTA_StatusOk;
}