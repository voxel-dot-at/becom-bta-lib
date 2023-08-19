#include "calcXYZ.h"
#include <bta_helper.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bta_oshelper.h>
//#include <direct.h>

static BTA_Status addLensVectors(BTA_CalcXYZInst *inst, BTA_LensVectors *calcXYZVectors);
static BTA_LensVectors *emptyLensVectors(BTA_CalcXYZInst *inst, uint16_t xRes, uint16_t yRes);
static BTA_LensVectors *getLenscalib(BTA_CalcXYZInst *inst, BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes);


BTA_Status BTAcalcXYZInit(BTA_CalcXYZInst **inst, BTA_InfoEventInst *infoEventInst) {
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    *inst = (BTA_CalcXYZInst *)calloc(1, sizeof(BTA_CalcXYZInst));
    if (!*inst) {
        return BTA_StatusOutOfMemory;
    }
    (*inst)->infoEventInst = infoEventInst;
    (*inst)->lensVectorsListLen = 0;
    (*inst)->lensVectorsList = 0;
    return BTA_StatusOk;
}


BTA_Status BTAcalcXYZClose(BTA_CalcXYZInst **inst) {
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (!*inst) {
        // not even opened
        return BTA_StatusOk;
    }
    for (int i = 0; i < (*inst)->lensVectorsListLen; i++) {
        BTAfreeLensVectors((*inst)->lensVectorsList[i]);
    }
    free((*inst)->lensVectorsList);
    (*inst)->lensVectorsList = 0;
    free(*inst);
    *inst = 0;
    return BTA_StatusOk;
}


BTA_Status BTAcalcXYZApply(BTA_CalcXYZInst *inst, BTA_WrapperInst *winst, BTA_Frame *frame, float offset) {
    if (!inst || !winst) {
        return BTA_StatusInvalidParameter;
    }
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    for (int chIn = 0; chIn < frame->channelsLen; chIn++) {
        BTA_Channel *channel = frame->channels[chIn];
        if (channel->id == BTA_ChannelIdDistance && channel->xRes > 0 && channel->yRes > 0) {
            BTA_LensVectors * lensVectors = getLenscalib(inst, winst, channel->xRes, channel->yRes);
            if (lensVectors->lensIndex != channel->lensIndex || lensVectors->xRes != channel->xRes || lensVectors->yRes != channel->yRes) {
                continue;
            }
            if (!lensVectors->vectorsX || !lensVectors->vectorsY || !lensVectors->vectorsZ) {
                return BTA_StatusInvalidParameter;
            }
            int pxCount = channel->xRes * channel->yRes;
            if (channel->dataFormat == BTA_DataFormatUInt16) {
                uint16_t *data = (uint16_t *)channel->data;
                int16_t *dataX = (int16_t *)malloc(pxCount * sizeof(int16_t));
                int16_t *dataY = (int16_t *)malloc(pxCount * sizeof(int16_t));
                int16_t *dataZ = (int16_t *)malloc(pxCount * sizeof(int16_t));
                if (!dataX || !dataY || !dataZ) {
                    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "BTAcalcXYZApply: out of memory");
                    continue;
                }
                int16_t *dataXp = dataX;
                int16_t *dataYp = dataY;
                int16_t *dataZp = dataZ;
                float *vectorsX = lensVectors->vectorsX;
                float *vectorsY = lensVectors->vectorsY;
                float *vectorsZ = lensVectors->vectorsZ;
                for (int xy = 0; xy < pxCount; xy++) {
                    *dataXp++ = (int16_t)((*data + offset) * *vectorsX++ + 0.5f);
                    *dataYp++ = (int16_t)((*data + offset) * *vectorsY++ + 0.5f);
                    if (*data < 10) {
                        *dataZp++ = INT16_MIN + *data;
                        vectorsZ++;
                    }
                    else {
                        *dataZp++ = (int16_t)((*data + offset) * *vectorsZ++ + 0.5f);
                    }
                    data++;
                }
                BTA_Status status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdX, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t *)dataX, pxCount * sizeof(int16_t),
                                                               0, 0, channel->lensIndex, channel->flags, channel->sequenceCounter, channel->gain);
                if (status != BTA_StatusOk) {
                    free(dataX);
                    dataX = 0;
                    BTAinfoEventHelper(inst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel X");
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdY, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t *)dataY, pxCount * sizeof(int16_t),
                                                    0, 0, channel->lensIndex, channel->flags, channel->sequenceCounter, channel->gain);
                if (status != BTA_StatusOk) {
                    free(dataY);
                    dataY = 0;
                    BTAinfoEventHelper(inst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel Y");
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdZ, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t *)dataZ, pxCount * sizeof(int16_t),
                                                    0, 0, channel->lensIndex, channel->flags, channel->sequenceCounter, channel->gain);
                if (status != BTA_StatusOk) {
                    free(dataZ);
                    dataZ = 0;
                    BTAinfoEventHelper(inst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel Z");
                }
                continue;
            }
            else {
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAcalcXYZApply: dataFormat %d not supported!", channel->dataFormat);
                return BTA_StatusNotSupported;
            }
        }
    }
    return BTA_StatusOk;
}


static BTA_Status addLensVectors(BTA_CalcXYZInst *inst, BTA_LensVectors *lensVectors) {
    if (!inst || !lensVectors) {
        return BTA_StatusInvalidParameter;
    }
    if (!inst->lensVectorsList) {
        inst->lensVectorsList = (BTA_LensVectors **)malloc(sizeof(BTA_LensVectors *));
        if (!inst->lensVectorsList) {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "addCalcXYZVectors: out of memory 1");
            return BTA_StatusOutOfMemory;
        }
        inst->lensVectorsListLen = 1;
    }
    else {
        inst->lensVectorsListLen++;
        BTA_LensVectors **temp = inst->lensVectorsList;
        inst->lensVectorsList = (BTA_LensVectors **)realloc(inst->lensVectorsList, inst->lensVectorsListLen * sizeof(BTA_LensVectors *));
        if (!inst->lensVectorsList) {
            inst->lensVectorsListLen--;
            inst->lensVectorsList = temp;
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "addCalcXYZVectors: out of memory 2");
            return BTA_StatusOutOfMemory;
        }
    }
    inst->lensVectorsList[inst->lensVectorsListLen - 1] = lensVectors;
    return BTA_StatusOk;
}


static BTA_LensVectors *emptyLensVectors(BTA_CalcXYZInst *inst, uint16_t xRes, uint16_t yRes) {
    BTA_LensVectors *calcXYZVectors = (BTA_LensVectors *)calloc(1, sizeof(BTA_LensVectors));
    if (!calcXYZVectors) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "emptyCalcXYZVectors: out of memory 2");
        return 0;
    }
    calcXYZVectors->lensIndex = 0;
    calcXYZVectors->lensId = 0;
    calcXYZVectors->xRes = xRes;
    calcXYZVectors->yRes = yRes;
    return calcXYZVectors;
}


// TODO: use getLensVectors instead
static BTA_LensVectors *getLenscalib(BTA_CalcXYZInst *inst, BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes) {
    // go through list and find
    for (int i = 0; i < inst->lensVectorsListLen; i++) {
        BTA_LensVectors *calcXYZVectors = inst->lensVectorsList[i];
        if (xRes == calcXYZVectors->xRes && yRes == calcXYZVectors->yRes) {
            return calcXYZVectors;
        }
    }


    // HACK
    //read from file and see if valid match
    while (1) {
        void *file;
        char filename[100];
        //char *pwd = _getcwd(0, 0);
        sprintf(filename, "lenscalib_%dx%d.bin", xRes, yRes);
        BTA_Status status = BTAfopen(filename, "rb", &file);
        if (status != BTA_StatusOk) {
            uint8_t cwd[1024];
            BTAgetCwd(cwd, 1024);
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, status, "getLenscalib from file: fopen did not find %s in %s", filename, cwd);
            break;
        }
        uint32_t filesize;
        status = BTAfseek(file, 0, BTA_SeekOriginEnd, &filesize);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "getLenscalib from file: fseek failed: %s", BTAstatusToString2(status));
            BTAfclose(file);
            break;
        }
        status = BTAfseek(file, 0, BTA_SeekOriginBeginning, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "getLenscalib from file: fseek failed: %s", BTAstatusToString2(status));
            BTAfclose(file);
            break;
        }

        uint8_t *cdata = (unsigned char *)malloc(filesize);
        if (!cdata) {
            BTAfclose(file);
            break;
        }

        uint32_t fileSizeRead;
        status = BTAfread(file, cdata, filesize, &fileSizeRead);
        if (status != BTA_StatusOk || fileSizeRead != filesize) {
            BTAfclose(file);
            free(cdata);
            cdata = 0;
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "getLenscalib from file: Error reading firmware file: %s. bytes read: %d", BTAstatusToString2(status), fileSizeRead);
            break;
        }
        BTAfclose(file);

        BTA_LensVectors *lensVectors2 = 0;
        status = BTAparseLenscalib(cdata, filesize, &lensVectors2, winst->infoEventInst);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "getLenscalib from file: Could not parse lenscalib data for %dx%d", xRes, yRes);
            break;
        }
        free(cdata);
        addLensVectors(inst, lensVectors2);
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "getLenscalib from file: Loaded lenscalib data for %dx%d", xRes, yRes);
        return lensVectors2;
    }


    // No lenscalib found for this resolution -> try to read from device
    BTA_FlashUpdateConfig flashUpdateConfig;
    BTAinitFlashUpdateConfig(&flashUpdateConfig);
    flashUpdateConfig.target = BTA_FlashTargetLensCalibration;
    BTA_LensVectors *lensVectors = 0;
    BTA_Status status = BTAflashRead(winst, &flashUpdateConfig, 0);
    if (status != BTA_StatusOk) {
        // the device does not deliver lenscalib. remember by adding an empty calcXYZVectors to list
        lensVectors = emptyLensVectors(inst, xRes, yRes);
        addLensVectors(inst, lensVectors);  // if adding fails, don't bother, will try again with next frame
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Could not load lenscalib data for %dx%d", xRes, yRes);
        return lensVectors;
    }
    status = BTAparseLenscalib(flashUpdateConfig.data, flashUpdateConfig.dataLen, &lensVectors, winst->infoEventInst);
    if (status != BTA_StatusOk) {
        // the lenscalib is not parseable. remember by adding an empty calcXYZVectors to list
        lensVectors = emptyLensVectors(inst, xRes, yRes);
        addLensVectors(inst, lensVectors);  // if adding fails, don't bother, will try again with next frame
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Could not parse lenscalib data for %dx%d", xRes, yRes);
        return lensVectors;
    }
    free(flashUpdateConfig.data);
    addLensVectors(inst, lensVectors);
    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Loaded lenscalib data for %dx%d", xRes, yRes);
    return lensVectors;
}