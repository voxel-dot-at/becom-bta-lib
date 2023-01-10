#include "calcXYZ.h"
#include <bta_helper.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bta_oshelper.h>
//#include <direct.h>

static void freeCalcXYZVectors(BTA_CalcXYZVectors *calcXYZVectors);
static BTA_Status parseLenscalib(BTA_WrapperInst *winst, uint8_t *data, uint32_t dataLen, BTA_CalcXYZVectors **calcXYZVectors);
static BTA_Status addCalcXYZVectors(BTA_WrapperInst *winst, BTA_CalcXYZVectors *calcXYZVectors);
static BTA_CalcXYZVectors *emptyCalcXYZVectors(BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes);
static BTA_CalcXYZVectors *getLenscalib(BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes);


BTA_Status BTAcalcXYZInit(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    winst->calcXYZInst = (BTA_CalcXYZInst *)calloc(1, sizeof(BTA_CalcXYZInst));
    BTA_CalcXYZInst *inst = winst->calcXYZInst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    inst->enabled = 0;
    inst->vectorsListLen = 0;
    inst->vectorsList = 0;
    return BTA_StatusOk;
}


BTA_Status BTAcalcXYZClose(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_CalcXYZInst *inst = (BTA_CalcXYZInst *)winst->calcXYZInst;
    winst->calcXYZInst = 0;
    if (!inst) {
        // not even opened
        return BTA_StatusOk;
    }
    if (inst) {
        for (int i = 0; i < inst->vectorsListLen; i++) {
            freeCalcXYZVectors(inst->vectorsList[i]);
        }
        free(inst->vectorsList);
        free(inst);
    }
    return BTA_StatusOk;
}


BTA_Status BTAcalcXYZApply(BTA_WrapperInst *winst, BTA_Frame *frame) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_CalcXYZInst *inst = (BTA_CalcXYZInst *)winst->calcXYZInst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (!inst->enabled) {
        return BTA_StatusOk;
    }
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    for (int chIn = 0; chIn < frame->channelsLen; chIn++) {
        BTA_Channel *channel = frame->channels[chIn];
        if (channel->id == BTA_ChannelIdDistance && channel->xRes > 0 && channel->yRes > 0) {
            BTA_CalcXYZVectors *calcXYZVectors = getLenscalib(winst, channel->xRes, channel->yRes);
            if (!calcXYZVectors->vectorsX || !calcXYZVectors->vectorsY || !calcXYZVectors->vectorsZ || calcXYZVectors->xRes != channel->xRes || calcXYZVectors->yRes != channel->yRes) {
                continue;
            }
            int pxCount = channel->xRes * channel->yRes;
            if (channel->dataFormat == BTA_DataFormatUInt16) {
                uint16_t *data = (uint16_t *)channel->data;
                float offset = inst->offset;
                int16_t *dataX = (int16_t *)malloc(pxCount * sizeof(int16_t));
                int16_t *dataY = (int16_t *)malloc(pxCount * sizeof(int16_t));
                int16_t *dataZ = (int16_t *)malloc(pxCount * sizeof(int16_t));
                if (!dataX || !dataY || !dataZ) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "BTAcalcXYZApply: out of memory");
                    continue;
                }
                int16_t *dataXp = dataX;
                int16_t *dataYp = dataY;
                int16_t *dataZp = dataZ;
                float *vectorsX = calcXYZVectors->vectorsX;
                float *vectorsY = calcXYZVectors->vectorsY;
                float *vectorsZ = calcXYZVectors->vectorsZ;
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
                BTA_Status status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdX, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t *)dataX, pxCount * sizeof(int16_t));
                if (status != BTA_StatusOk) {
                    free(dataX);
                    dataX = 0;
                    BTAinfoEventHelper(winst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel X");
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdY, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t *)dataY, pxCount * sizeof(int16_t));
                if (status != BTA_StatusOk) {
                    free(dataY);
                    dataY = 0;
                    BTAinfoEventHelper(winst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel Y");
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdZ, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t *)dataZ, pxCount * sizeof(int16_t));
                if (status != BTA_StatusOk) {
                    free(dataZ);
                    dataZ = 0;
                    BTAinfoEventHelper(winst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel Z");
                }
                return BTA_StatusOk;
            }
            else {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAcalcXYZApply: dataFormat %d not supported!", channel->dataFormat);
                return BTA_StatusNotSupported;
            }

        }
    }
    return BTA_StatusOk;
}



static void freeCalcXYZVectors(BTA_CalcXYZVectors *calcXYZVectors) {
    free(calcXYZVectors->vectorsX);
    free(calcXYZVectors->vectorsY);
    free(calcXYZVectors->vectorsZ);
    free(calcXYZVectors);
}


static BTA_Status parseLenscalib(BTA_WrapperInst *winst, uint8_t *data, uint32_t dataLen, BTA_CalcXYZVectors **calcXYZVectors) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (dataLen < 50) {
        return BTA_StatusInvalidParameter;
    }
    BTA_LenscalibHeader *lenscalibHeader = (BTA_LenscalibHeader *)data;
    if (lenscalibHeader->preamble0 != 0xf011 || lenscalibHeader->preamble1 != 0xb1ad) {
        return BTA_StatusInvalidParameter;
    }

    BTA_CalcXYZVectors *v;
    switch (lenscalibHeader->version) {
    case 1:
    case 2:
    {
        uint16_t xRes = lenscalibHeader->xRes;
        uint16_t yRes = lenscalibHeader->yRes;
        uint16_t bytesPerPixel = lenscalibHeader->bytesPerPixel;
        float expansionfactor = (float)lenscalibHeader->expasionFactor;
        v = (BTA_CalcXYZVectors *)calloc(1, sizeof(BTA_CalcXYZVectors));
        if (!v) {
            return BTA_StatusOutOfMemory;
        }
        v->xRes = xRes;
        v->yRes = yRes;
        v->vectorsX = (float *)calloc(sizeof(float), yRes * xRes);
        v->vectorsY = (float *)calloc(sizeof(float), yRes * xRes);
        v->vectorsZ = (float *)calloc(sizeof(float), yRes * xRes);
        uint16_t coordSysId = lenscalibHeader->coordSysId;
        if (!v->vectorsX || !v->vectorsY || !v->vectorsZ) {
            free(v->vectorsX);
            free(v->vectorsY);
            free(v->vectorsZ);
            free(v);
            return BTA_StatusOutOfMemory;
        }
        if (bytesPerPixel == 2) {
            int16_t *dataXYZ = (int16_t *)(data + sizeof(BTA_LenscalibHeader));
            float *dataX = v->vectorsX;
            float *dataY = v->vectorsY;
            float *dataZ = v->vectorsZ;
            if (coordSysId) {
                for (int i = 0; i < xRes * yRes; i++) {
                    *dataX++ = *dataXYZ++ / expansionfactor;
                    *dataY++ = *dataXYZ++ / expansionfactor;
                    *dataZ++ = *dataXYZ++ / expansionfactor;
                }
            }
            else {
                for (int i = 0; i < xRes * yRes; i++) {
                    *dataZ++ = *dataXYZ++ / expansionfactor;
                    *dataX++ = -*dataXYZ++ / expansionfactor;
                    *dataY++ = -*dataXYZ++ / expansionfactor;
                }
            }
            *calcXYZVectors = v;
            return BTA_StatusOk;
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "addLenscalib: bytesPerPixel %d not supported!", bytesPerPixel);
            return BTA_StatusNotSupported;
        }
    }

    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "addLenscalib: version %d not supported!", lenscalibHeader->version);
        return BTA_StatusNotSupported;
    }
}


static BTA_Status addCalcXYZVectors(BTA_WrapperInst *winst, BTA_CalcXYZVectors *calcXYZVectors) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_CalcXYZInst *inst = (BTA_CalcXYZInst *)winst->calcXYZInst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (!inst->vectorsList) {
        inst->vectorsList = (BTA_CalcXYZVectors **)malloc(sizeof(BTA_CalcXYZVectors *));
        if (!inst->vectorsList) {
            freeCalcXYZVectors(calcXYZVectors);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "addCalcXYZVectors: out of memory 1");
            return BTA_StatusOutOfMemory;
        }
        inst->vectorsListLen = 1;
    }
    else {
        inst->vectorsListLen++;
        BTA_CalcXYZVectors **temp = inst->vectorsList;
        inst->vectorsList = (BTA_CalcXYZVectors **)realloc(inst->vectorsList, inst->vectorsListLen * sizeof(BTA_CalcXYZVectors *));
        if (!inst->vectorsList) {
            inst->vectorsListLen--;
            inst->vectorsList = temp;
            freeCalcXYZVectors(calcXYZVectors);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "addCalcXYZVectors: out of memory 2");
            return BTA_StatusOutOfMemory;
        }
    }
    inst->vectorsList[inst->vectorsListLen - 1] = calcXYZVectors;
    return BTA_StatusOk;
}


static BTA_CalcXYZVectors *emptyCalcXYZVectors(BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes) {
    if (!winst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusOutOfMemory, "emptyCalcXYZVectors: madness");
        return 0;
    }
    while (1) {
        BTA_CalcXYZVectors *calcXYZVectors = (BTA_CalcXYZVectors *)calloc(1, sizeof(BTA_CalcXYZVectors));
        if (!calcXYZVectors) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusOutOfMemory, "emptyCalcXYZVectors: out of memory 2");
            BTAsleep(200);
            continue;
        }
        calcXYZVectors->xRes = xRes;
        calcXYZVectors->yRes = yRes;
        return calcXYZVectors;
    }
}


static BTA_CalcXYZVectors *getLenscalib(BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes) {
    // go through list and find
    for (int i = 0; i < winst->calcXYZInst->vectorsListLen; i++) {
        BTA_CalcXYZVectors *calcXYZVectors = winst->calcXYZInst->vectorsList[i];
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
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "getLenscalib from file: fopen did not find %s in %s", filename, cwd);
            break;
        }
        uint32_t filesize;
        status = BTAfseek(file, 0, BTA_SeekOriginEnd, &filesize);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "getLenscalib from file: fseek failed: %s", BTAstatusToString2(status));
            BTAfclose(file);
            break;
        }
        status = BTAfseek(file, 0, BTA_SeekOriginBeginning, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "getLenscalib from file: fseek failed: %s", BTAstatusToString2(status));
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
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "getLenscalib from file: Error reading firmware file: %s. bytes read: %d", BTAstatusToString2(status), fileSizeRead);
            break;
        }
        BTAfclose(file);

        BTA_CalcXYZVectors *calcXYZVectors2 = 0;
        status = parseLenscalib(winst, cdata, filesize, &calcXYZVectors2);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "getLenscalib from file: Could not parse lenscalib data for %dx%d", xRes, yRes);
            break;
        }
        free(cdata);
        addCalcXYZVectors(winst, calcXYZVectors2);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "getLenscalib from file: Loaded lenscalib data for %dx%d", xRes, yRes);
        return calcXYZVectors2;
    }


    // No lenscalib found for this resolution -> try to read from device
    BTA_FlashUpdateConfig flashUpdateConfig;
    BTAinitFlashUpdateConfig(&flashUpdateConfig);
    flashUpdateConfig.target = BTA_FlashTargetLensCalibration;
    BTA_CalcXYZVectors *calcXYZVectors = 0;
    BTA_Status status = BTAflashRead(winst, &flashUpdateConfig, 0);
    if (status != BTA_StatusOk) {
        // the device does not deliver lenscalib. remember by adding an empty calcXYZVectors to list
        calcXYZVectors = emptyCalcXYZVectors(winst, xRes, yRes);
        addCalcXYZVectors(winst, calcXYZVectors);  // if adding fails, don't bother, will try again with next frame
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Could not load lenscalib data for %dx%d", xRes, yRes);
        return calcXYZVectors;
    }
    status = parseLenscalib(winst, flashUpdateConfig.data, flashUpdateConfig.dataLen, &calcXYZVectors);
    if (status != BTA_StatusOk) {
        // the lenscalib is not parseable. remember by adding an empty calcXYZVectors to list
        calcXYZVectors = emptyCalcXYZVectors(winst, xRes, yRes);
        addCalcXYZVectors(winst, calcXYZVectors);  // if adding fails, don't bother, will try again with next frame
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Could not parse lenscalib data for %dx%d", xRes, yRes);
        return calcXYZVectors;
    }
    free(flashUpdateConfig.data);
    addCalcXYZVectors(winst, calcXYZVectors);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Loaded lenscalib data for %dx%d", xRes, yRes);
    return calcXYZVectors;
}