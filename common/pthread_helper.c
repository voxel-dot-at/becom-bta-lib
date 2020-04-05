#include "pthread_helper.h"
#include "timing_helper.h"
//#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#ifdef PLAT_WINDOWS
#   include <semaphore.h>
#elif defined PLAT_LINUX
#   include <semaphore.h>
#   include <errno.h>
#elif defined PLAT_APPLE
#   include <dispatch/dispatch.h>
#endif




BTA_Status BTAinitMutex(void **mutex) {
    pthread_mutex_t *mutexPosix = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
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
    if (!mutex) {
    }
    int result = pthread_mutex_lock((pthread_mutex_t *)mutex);
    assert(!result);
    if (!result) {
    }
}


void BTAunlockMutex(void *mutex) {
    assert(mutex);
    if (!mutex) {
    }
    int result = pthread_mutex_unlock((pthread_mutex_t *)mutex);
    assert(!result);
    if (!result) {
    }
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

#include <stdio.h>
BTA_Status BTAcreateThread(void **tid, void *(*runFunction) (void *), void *arg, int priority) {
    if (!tid) {
        return BTA_StatusRuntimeError;
    }

    *tid = 0;
    //int min2 = sched_get_priority_min(policy);
    //printf("priomin %d\n", min2);
    //int max2 = sched_get_priority_max(policy);
    //printf("priomax %d\n", max2);
    //if (priority > 0) {
    //    if (priority > 100) priority = 100;
    //    int max = sched_get_priority_max(policy);
    //    prio = priority * max / 100;
    //    printf("priomax %d, set %d\n", max, prio);
    //}
    //if (priority < 0) {
    //    if (priority < -100) priority = -100;
    //    int min = sched_get_priority_min(policy);
    //    prio = -priority * min / 100;
    //    printf("priomin %d, set %d\n", min, prio);
    //}

    int result;
    pthread_attr_t *tattr = (pthread_attr_t *)0;
    //if (priority) {
    //    int policy = SCHED_RR;
    //    int prio = 99;
    //    tattr = (pthread_attr_t *)malloc(sizeof(pthread_attr_t));
    //    result = pthread_attr_init(tattr);
    //    if (result) {
    //        printf("pthread_attr_init\n");
    //        free(tattr);
    //        tattr = 0;
    //        return BTA_StatusRuntimeError;
    //    }

    //    result = pthread_attr_getschedparam(tattr, &param);
    //    if (result) {
    //        printf("pthread_attr_getschedparam\n");
    //        free(tattr);
    //        tattr = 0;
    //        return BTA_StatusRuntimeError;
    //    }

    //    result = pthread_attr_setinheritsched(tattr, PTHREAD_EXPLICIT_SCHED);
    //    if (result) {
    //        printf("pthread_attr_setinheritsched\n");
    //        free(tattr);
    //        tattr = 0;
    //        return BTA_StatusRuntimeError;
    //    }

    //    result = pthread_attr_setschedpolicy(tattr, policy);
    //    if (result) {
    //        printf("pthread_attr_setschedpolicy\n");
    //        free(tattr);
    //        tattr = 0;
    //        return BTA_StatusRuntimeError;
    //    }

    //    param.sched_priority = prio;
    //    result = pthread_attr_setschedparam(tattr, &param);
    //    if (result) {
    //        printf("pthread_attr_setschedparam\n");
    //        free(tattr);
    //        tattr = 0;
    //        return BTA_StatusRuntimeError;
    //    }
    //}

    pthread_t *tidPosix = (pthread_t *)malloc(sizeof(pthread_t));
    if (!tidPosix) {
        free(tattr);
        tattr = 0;
        return BTA_StatusOutOfMemory;
    }
    result = pthread_create(tidPosix, tattr, runFunction, arg);
    if (result) {
        //printf("pthread_create %d\n", result);
        free(tattr);
        tattr = 0;
        free(tidPosix);
        tidPosix = 0;
        return BTA_StatusRuntimeError;
    }

    //int policyTest;
    //struct sched_param paramTest;
    //pthread_getschedparam(*tidPosix, &policyTest, &paramTest);
    //printf("policy:: %d pri :: %d\n", policyTest, paramTest.sched_priority);





    ////// initialized with default attributes
    ////result = pthread_attr_init(&tattr);
    ////// safe to get existing scheduling param
    ////sched_param param;
    ////result = pthread_attr_getschedparam(&tattr, &param);
    ////// set the priority; others are unchanged
    ////printf("\nPRIORITY  %d", param.sched_priority);
    ////param.sched_priority = 20;
    ////// setting the new scheduling param
    ////result = pthread_attr_setschedparam(&tattr, &param);

    //pthread_t *tidPosix = (pthread_t *)malloc(sizeof(pthread_t));
    //if (!tidPosix) {
    //    *tid = 0;
    //    return BTA_StatusOutOfMemory;
    //}
    //result = pthread_create(tidPosix, tattr, runFunction, arg);
    //if (result) {
    //    *tid = 0;
    //    free(tidPosix);
    //    tidPosix = 0;
    //    return BTA_StatusRuntimeError;
    //}

    /////*if (priority > 0 && priority <= 100) */{
    ////    int policy;
    ////    struct sched_param param;
    ////    result = pthread_getschedparam(*tidPosix, &policy, &param);
    ////    printf("\npthread_getschedparam %d   policy %d   prio %d\n", result, policy, param.sched_priority);
    ////    if (!result) {
    ////        int max = sched_get_priority_max(policy);
    ////        int min = sched_get_priority_min(policy);
    ////        printf("\nmin %d, max %d\n", min, max);
    ////    }
    ////}

    *tid = tidPosix;
    return BTA_StatusOk;
}


BTA_Status BTAsetRealTimePriority(void *tid) {
    pthread_t this_thread = *((pthread_t *)tid);
    // struct sched_param is used to store the scheduling priority
    struct sched_param params;
    // We'll set the priority to the maximum.
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    // Attempt to set thread real-time priority to the SCHED_FIFO policy
    int result = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
    if (result) {
        printf("error in pthread_setschedparam %d\n", result);
        return BTA_StatusRuntimeError;
    }
    // Now verify the change in thread priority
    int policy = 0;
    result = pthread_getschedparam(this_thread, &policy, &params);
    if (result != 0) {
        printf("error in pthread_getschedparam %d\n", result);
        return BTA_StatusRuntimeError;
    }
    // Check the correct policy was applied
    if (policy != SCHED_FIFO) {
        printf("Scheduling is NOT SCHED_FIFO!\n");
    }
    else {
        printf("SCHED_FIFO OK\n");
    }
    // Print thread scheduling priority
    printf("policy:: %d pri :: %d\n", policy, params.sched_priority);
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
        sem_t *semaphorePosix = (sem_t *)malloc(sizeof(sem_t));
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


//int BTAgetSemaphoreValue(void *semaphore) {
//    assert(semaphore);
//    if (!semaphore) {
//    }
//    int value = 0xffffffff;
//    int counter = 0;
//    while (value == 0xffffffff) {
//        counter++;
//        int result = sem_getvalue((sem_t *)semaphore, &value);
//        assert(!result);
//        if (value == 0xffffffff) {
//            //printf("debug\n");
//            BTAmsleep(1);
//        }
//        if (result) {
//        }
//    }
//    if (counter > 1) {
//        printf("BTAgetSemaphoreValue %d times\n", counter);
//    }
//    return value;
//}


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
        if(result != 0)
        {
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