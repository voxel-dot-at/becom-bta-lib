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

struct BTA_InfoEventInst;

typedef struct BTA_GrabInst {
    uint8_t grabbingEnabled;
    uint8_t *grabbingFilename;
    void *grabbingThread;
    uint8_t *grabbingQueue;
    int32_t grabbingQueuePos;      ///< index of first free byte (0 >= grabbingQueuePos < grabbingQueueLength)
    int32_t grabbingQueueCount;    ///< count of valid bytes in queue (from index grabbingQueuePos - 1 backwards) (0 >= grabbingQueueCount <= grabbingQueueLength)
    void *grabbingQueueMutex;
    uint32_t totalFrameCount;
    struct BTA_InfoEventInst *infoEventInst;
} BTA_GrabInst;


BTA_Status BGRBinit(BTA_GrabbingConfig *config, uint8_t *libNameVer, BTA_DeviceInfo *deviceInfo, BTA_GrabInst **inst, struct BTA_InfoEventInst *infoEventInst);
BTA_Status BGRBgrab(BTA_GrabInst *inst, BTA_Frame *frame);
BTA_Status BGRBclose(BTA_GrabInst **inst);


#endif
