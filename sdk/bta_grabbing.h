/**  @file bta_grabbing.h
*
*    @brief Support for grabbing
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

#ifndef BTA_GRABBING_H_INCLUDED
#define BTA_GRABBING_H_INCLUDED

#include <bvq_queue.h>

struct BTA_InfoEventInst;

typedef struct BTA_GrabInst {
    uint8_t grabbingEnabled;
    uint8_t *grabbingFilename;
    uint8_t libNameVer[70];
    BTA_CompressionMode lpBltstreamCompressionMode;
    void *grabbingThread;
    BFQ_FrameQueueHandle grabbingQueue;
    //void *grabbingQueueMutex;
    uint32_t totalFrameCount;
    struct BTA_InfoEventInst *infoEventInst;
} BTA_GrabInst;


BTA_Status BGRBinit(BTA_GrabInst **inst, struct BTA_InfoEventInst *infoEventInst);
BTA_Status BGRBstart(BTA_GrabInst *inst, BTA_GrabbingConfig *config, BTA_DeviceInfo *deviceInfo);
BTA_Status BGRBgrab(BTA_GrabInst *inst, BTA_Frame *frame);
BTA_Status BGRBstop(BTA_GrabInst *inst);
BTA_Status BGRBclose(BTA_GrabInst **inst);


#endif
