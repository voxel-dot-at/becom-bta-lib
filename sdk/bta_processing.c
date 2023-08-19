#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bta.h>
#include <bta_helper.h>
#include <mth_math.h>


BTA_Status BTA_CALLCONV BTAaverageFrames(BTA_Frame** frames, int framesLen, float minValidPixelPercentage, BTA_Frame** result) {
    if (!frames || framesLen < 1 || !result) {
        return BTA_StatusInvalidParameter;
    }
    uint8_t channelsLen = frames[0]->channelsLen;
    for (int frInd = 1; frInd < framesLen; frInd++) {
        if (channelsLen != frames[frInd]->channelsLen) {
            return BTA_StatusInvalidData;
        }
    }
    BTA_Channel** channels = (BTA_Channel**)malloc(framesLen * sizeof(BTA_Channel*));
    if (!channels) {
        return BTA_StatusOutOfMemory;
    }
    BTA_Status status = BTAcloneFrame(frames[0], result);
    if (status != BTA_StatusOk) {
        free(channels);
        return status;
    }
    BTA_Frame* resultTemp = *result;
    BTA_Channel* channelTemp;
    for (int chInd = 0; chInd < channelsLen; chInd++) {
        for (int frInd = 0; frInd < framesLen; frInd++) {
            channels[frInd] = frames[frInd]->channels[chInd];
        }
        status = BTAaverageChannels(channels, framesLen, minValidPixelPercentage, &channelTemp);
        if (status != BTA_StatusOk) {
            free(channels);
            BTAfreeFrame(result);
            return status;
        }
        // replace cloned resultTemp with averaged resultTemp
        BTAfreeChannel(&resultTemp->channels[chInd]);
        resultTemp->channels[chInd] = channelTemp;
    }
    free(channels);
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAaverageChannels(BTA_Channel **channels, int channelsLen, float minValidPixelPercentage, BTA_Channel **result) {
    if (!channels || channelsLen < 1 || !result || minValidPixelPercentage < 0 || minValidPixelPercentage > 100) {
        return BTA_StatusInvalidParameter;
    }
    BTA_ChannelId id = channels[0]->id;
    uint16_t xRes = channels[0]->xRes;
    uint16_t yRes = channels[0]->yRes;
    int res = (int)xRes * yRes;
    BTA_DataFormat dataFormat = channels[0]->dataFormat;
    BTA_Unit unit = channels[0]->unit;
    for (int chInd = 1; chInd < channelsLen; chInd++) {
        if (id != channels[chInd]->id || xRes != channels[chInd]->xRes || yRes != channels[chInd]->yRes || dataFormat != channels[chInd]->dataFormat || unit != channels[chInd]->unit) {
            return BTA_StatusInvalidData;
        }
    }
    // Allocate for sums
    double* sums = (double *)calloc(res, sizeof(double));
    if (!sums) {
        return BTA_StatusOutOfMemory;
    }
    // Allocate for result channel
    BTA_Status status = BTAcloneChannelEmpty(channels[0], result);
    if (status != BTA_StatusOk) {
        free(sums);
        return status;
    }
    // Allocate for invalid pixel statistics
    uint16_t **invalidsCounts = 0;
    if (id == BTA_ChannelIdDistance || id == BTA_ChannelIdZ || id == BTA_ChannelIdAmplitude || id == BTA_ChannelIdFlags) {
        invalidsCounts = (uint16_t **)malloc(res * sizeof(uint16_t *));
        if (!invalidsCounts) {
            free(sums);
            return BTA_StatusOutOfMemory;
        }
        for (int xy = 0; xy < res; xy++) {
            invalidsCounts[xy] = (uint16_t *)calloc(11, sizeof(uint16_t));
            if (!invalidsCounts[xy]) {
                free(sums);
                free(invalidsCounts);
                return BTA_StatusOutOfMemory;
            }
        }
    }
    BTA_Channel* resultTemp = *result;
    switch (id) {

    case BTA_ChannelIdDistance:
    case BTA_ChannelIdZ:
        switch (dataFormat) {

        case BTA_DataFormatUInt8:
            for (int chInd = 0; chInd < channelsLen; chInd++) {
                for (int xy = 0; xy < res; xy++) {
                    uint8_t v = ((uint8_t*)channels[chInd]->data)[xy];
                    if (v < 10) {
                        invalidsCounts[xy][v]++;
                        invalidsCounts[xy][10]++;
                    }
                    else {
                        sums[xy] += v;
                    }
                }
            }
            for (int xy = 0; xy < res; xy++) {
                if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                    uint16_t max = 0;
                    for (uint8_t i = 0; i < 10; i++) {
                        if (invalidsCounts[xy][i] > max) {
                            ((uint8_t*)resultTemp->data)[xy] = i;
                            max = invalidsCounts[xy][i];
                        }
                    }
                }
                else {
                    ((uint8_t*)resultTemp->data)[xy] = (uint8_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                }
            }
            break;
        case BTA_DataFormatUInt16:
        case BTA_DataFormatUInt16Mlx12U:
        case BTA_DataFormatUInt16Mlx1C11U:
            for (int chInd = 0; chInd < channelsLen; chInd++) {
                for (int xy = 0; xy < res; xy++) {
                    uint16_t v = ((uint16_t*)channels[chInd]->data)[xy];
                    if (v < 10) {
                        invalidsCounts[xy][v]++;
                        invalidsCounts[xy][10]++;
                    }
                    else {
                        sums[xy] += v;
                    }
                }
            }
            for (int xy = 0; xy < res; xy++) {
                if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                    uint16_t max = 0;
                    for (uint8_t i = 0; i < 10; i++) {
                        if (invalidsCounts[xy][i] > max) {
                            ((uint16_t*)resultTemp->data)[xy] = i;
                            max = invalidsCounts[xy][i];
                        }
                    }
                }
                else {
                    ((uint16_t*)resultTemp->data)[xy] = (uint16_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                }
            }
            break;
        case BTA_DataFormatSInt16:
        case BTA_DataFormatSInt16Mlx12S:
        case BTA_DataFormatSInt16Mlx1C11S:
            for (int chInd = 0; chInd < channelsLen; chInd++) {
                for (int xy = 0; xy < res; xy++) {
                    int16_t v = ((int16_t*)channels[chInd]->data)[xy];
                    if (v < INT16_MIN + 10) {
                        invalidsCounts[xy][v - INT16_MIN]++;
                        invalidsCounts[xy][10]++;
                    }
                    else {
                        sums[xy] += v;
                    }
                }
            }
            for (int xy = 0; xy < res; xy++) {
                if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                    uint16_t max = 0;
                    for (uint8_t i = 0; i < 10; i++) {
                        if (invalidsCounts[xy][i] > max) {
                            ((int16_t*)resultTemp->data)[xy] = i;
                            max = invalidsCounts[xy][i];
                        }
                    }
                }
                else {
                    ((int16_t*)resultTemp->data)[xy] = (int16_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                }
            }
            break;
        case BTA_DataFormatUInt32:
            for (int chInd = 0; chInd < channelsLen; chInd++) {
                for (int xy = 0; xy < res; xy++) {
                    uint32_t v = ((uint32_t*)channels[chInd]->data)[xy];
                    if (v < 10) {
                        invalidsCounts[xy][v]++;
                        invalidsCounts[xy][10]++;
                    }
                    else {
                        sums[xy] += v;
                    }
                }
            }
            for (int xy = 0; xy < res; xy++) {
                if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                    uint16_t max = 0;
                    for (uint8_t i = 0; i < 10; i++) {
                        if (invalidsCounts[xy][i] > max) {
                            ((uint32_t*)resultTemp->data)[xy] = i;
                            max = invalidsCounts[xy][i];
                        }
                    }
                }
                else {
                    ((uint32_t*)resultTemp->data)[xy] = (uint32_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                }
            }
            break;
        case BTA_DataFormatSInt32:
            for (int chInd = 0; chInd < channelsLen; chInd++) {
                for (int xy = 0; xy < res; xy++) {
                    int32_t v = ((int32_t*)channels[chInd]->data)[xy];
                    if (v < INT32_MIN + 10) {
                        invalidsCounts[xy][v - INT32_MIN]++;
                        invalidsCounts[xy][10]++;
                    }
                    else {
                        sums[xy] += v;
                    }
                }
            }
            for (int xy = 0; xy < res; xy++) {
                if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                    uint16_t max = 0;
                    for (uint8_t i = 0; i < 10; i++) {
                        if (invalidsCounts[xy][i] > max) {
                            ((int32_t*)resultTemp->data)[xy] = i;
                            max = invalidsCounts[xy][i];
                        }
                    }
                }
                else {
                    ((int32_t*)resultTemp->data)[xy] = (int32_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                }
            }
            break;
        default:
            BTAfreeChannel(result);
            for (int xy = 0; xy < res; xy++) free(invalidsCounts[xy]);
            free(invalidsCounts);
            free(sums);
            return BTA_StatusNotSupported;
        }
        break;

        case BTA_ChannelIdAmplitude:
        case BTA_ChannelIdFlags:
            switch (dataFormat) {

            case BTA_DataFormatUInt8:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        uint8_t v = ((uint8_t*)channels[chInd]->data)[xy];
                        if (v == UINT8_MAX) {
                            invalidsCounts[xy][10]++;
                        }
                        else {
                            sums[xy] += v;
                        }
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                        ((uint8_t*)resultTemp->data)[xy] = UINT8_MAX;
                    }
                    else {
                        ((uint8_t*)resultTemp->data)[xy] = (uint8_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                    }
                }
                break;
            case BTA_DataFormatUInt16:
            case BTA_DataFormatUInt16Mlx12U:
            case BTA_DataFormatUInt16Mlx1C11U:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        uint16_t v = ((uint16_t*)channels[chInd]->data)[xy];
                        if (v == UINT16_MAX) {
                            invalidsCounts[xy][10]++;
                        }
                        else {
                            sums[xy] += v;
                        }
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                        ((uint16_t*)resultTemp->data)[xy] = UINT16_MAX;
                    }
                    else {
                        ((uint16_t*)resultTemp->data)[xy] = (uint16_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                    }
                }
                break;
            case BTA_DataFormatSInt16:
            case BTA_DataFormatSInt16Mlx12S:
            case BTA_DataFormatSInt16Mlx1C11S:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        int16_t v = ((int16_t*)channels[chInd]->data)[xy];
                        if (v == INT16_MAX) {
                            invalidsCounts[xy][10]++;
                        }
                        else {
                            sums[xy] += v;
                        }
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                        ((int16_t*)resultTemp->data)[xy] = INT16_MAX;
                    }
                    else {
                        ((int16_t*)resultTemp->data)[xy] = (int16_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                    }
                }
                break;
            case BTA_DataFormatUInt32:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        uint32_t v = ((uint32_t*)channels[chInd]->data)[xy];
                        if (v == UINT32_MAX) {
                            invalidsCounts[xy][10]++;
                        }
                        else {
                            sums[xy] += v;
                        }
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                        ((uint32_t*)resultTemp->data)[xy] = UINT32_MAX;
                    }
                    else {
                        ((uint32_t*)resultTemp->data)[xy] = (uint32_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                    }
                }
                break;
            case BTA_DataFormatSInt32:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        int32_t v = ((int32_t*)channels[chInd]->data)[xy];
                        if (v == INT32_MAX) {
                            invalidsCounts[xy][10]++;
                        }
                        else {
                            sums[xy] += v;
                        }
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    if (invalidsCounts[xy][10] >= channelsLen * minValidPixelPercentage / 100) {
                        ((int32_t*)resultTemp->data)[xy] = INT32_MAX;
                    }
                    else {
                        ((int32_t*)resultTemp->data)[xy] = (int32_t)MTHround(sums[xy] / (channelsLen - invalidsCounts[xy][10]));
                    }
                }
                break;
            default:
                BTAfreeChannel(result);
                for (int xy = 0; xy < res; xy++) free(invalidsCounts[xy]);
                free(invalidsCounts);
                free(sums);
                return BTA_StatusNotSupported;
            }
            break;

        case BTA_ChannelIdColor:
            // No averaging, just clone the first channel (todo?)
            BTAfreeChannel(result);
            status = BTAcloneChannel(channels[0], result);
            if (status != BTA_StatusOk) {
                free(sums);
                return status;
            }
            break;

        default:
            switch (dataFormat) {

            case BTA_DataFormatUInt8:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        sums[xy] += ((uint8_t*)channels[chInd]->data)[xy];
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    ((uint8_t*)resultTemp->data)[xy] = (uint8_t)MTHround(sums[xy] / channelsLen);
                }
                break;
            case BTA_DataFormatUInt16:
            case BTA_DataFormatUInt16Mlx12U:
            case BTA_DataFormatUInt16Mlx1C11U:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        sums[xy] += ((uint16_t*)channels[chInd]->data)[xy];
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    ((uint16_t*)resultTemp->data)[xy] = (uint16_t)MTHround(sums[xy] / channelsLen);
                }
                break;
            case BTA_DataFormatSInt16:
            case BTA_DataFormatSInt16Mlx12S:
            case BTA_DataFormatSInt16Mlx1C11S:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        sums[xy] += ((int16_t*)channels[chInd]->data)[xy];
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    ((int16_t*)resultTemp->data)[xy] = (int16_t)MTHround(sums[xy] / channelsLen);
                }
                break;
            case BTA_DataFormatUInt32:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        sums[xy] += ((uint32_t*)channels[chInd]->data)[xy];
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    ((uint32_t*)resultTemp->data)[xy] = (uint32_t)MTHround(sums[xy] / channelsLen);
                }
                break;
            case BTA_DataFormatSInt32:
                for (int chInd = 0; chInd < channelsLen; chInd++) {
                    for (int xy = 0; xy < res; xy++) {
                        sums[xy] += ((int32_t*)channels[chInd]->data)[xy];
                    }
                }
                for (int xy = 0; xy < res; xy++) {
                    ((int32_t*)resultTemp->data)[xy] = (int32_t)MTHround(sums[xy] / channelsLen);
                }
                break;
            default:
                BTAfreeChannel(result);
                for (int xy = 0; xy < res; xy++) free(invalidsCounts[xy]);
                free(invalidsCounts);
                free(sums);
                return BTA_StatusNotSupported;
            }
    }
    if (id == BTA_ChannelIdDistance || id == BTA_ChannelIdZ || id == BTA_ChannelIdAmplitude || id == BTA_ChannelIdFlags) {
        for (int xy = 0; xy < res; xy++) free(invalidsCounts[xy]);
        free(invalidsCounts);
    }
    free(sums);
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAcalcXYZ(BTA_Frame *frame, BTA_LensVectors **lensVectorsList, uint16_t lensVectorsListLen, BTA_ExtrinsicData **extrinsicDataList, uint16_t extrinsicDataListLen) {
    if (!frame || !lensVectorsList) {
        return BTA_StatusInvalidParameter;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        BTA_Channel *channel = frame->channels[chInd];
        if (channel->id != BTA_ChannelIdDistance || channel->xRes == 0 || channel->yRes == 0) {
            continue;
        }
        for (int lvInd = 0; lvInd < lensVectorsListLen; lvInd++) {
            BTA_LensVectors* lensVectors = lensVectorsList[lvInd];
            if (lensVectors->lensIndex != channel->lensIndex || lensVectors->xRes != channel->xRes || lensVectors->yRes != channel->yRes) {
                continue;
            }
            if (!lensVectors->vectorsX || !lensVectors->vectorsY || !lensVectors->vectorsZ) {
                return BTA_StatusInvalidParameter;
            }
            float* rotTrlInv = 0;
            if (extrinsicDataList) {
                for (int edInd = 0; edInd < extrinsicDataListLen; edInd++) {
                    if (extrinsicDataList[edInd]->rotTrlInv && extrinsicDataList[edInd]->lensIndex == channel->lensIndex) {
                        rotTrlInv = extrinsicDataList[edInd]->rotTrlInv;
                        break;
                    }
                }
            }

            int pxCount = channel->xRes * channel->yRes;
            if (channel->dataFormat == BTA_DataFormatUInt16) {
                uint16_t* data = (uint16_t*)channel->data;
                int16_t* dataX = (int16_t*)malloc(pxCount * sizeof(int16_t));
                int16_t* dataY = (int16_t*)malloc(pxCount * sizeof(int16_t));
                int16_t* dataZ = (int16_t*)malloc(pxCount * sizeof(int16_t));
                if (!dataX || !dataY || !dataZ) {
                    //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "BTAcalcXYZApply: out of memory");
                    return BTA_StatusOutOfMemory;
                }
                int16_t* dataXp = dataX;
                int16_t* dataYp = dataY;
                int16_t* dataZp = dataZ;
                float* vectorsX = lensVectors->vectorsX;
                float* vectorsY = lensVectors->vectorsY;
                float* vectorsZ = lensVectors->vectorsZ;
                for (int xy = 0; xy < pxCount; xy++) {
                    if (*data < 10) {
                        // Invalid pixel map to respective range for invalidation values
                        *dataXp++ = (int16_t)(*data * *vectorsX++ + 0.5f);
                        *dataYp++ = (int16_t)(*data * *vectorsY++ + 0.5f);
                        *dataZp++ = INT16_MIN + *data;
                        vectorsZ++;
                    }
                    else {
                        if (rotTrlInv) {
                            float x = *data * *vectorsX++;
                            float y = *data * *vectorsY++;
                            float z = *data * *vectorsZ++;
                            *dataXp++ = (int16_t)(rotTrlInv[0] * x + rotTrlInv[1] * y + rotTrlInv[2] * z + rotTrlInv[3] + .5f);
                            *dataYp++ = (int16_t)(rotTrlInv[4] * x + rotTrlInv[5] * y + rotTrlInv[6] * z + rotTrlInv[7] + .5f);
                            *dataZp++ = (int16_t)(rotTrlInv[8] * x + rotTrlInv[9] * y + rotTrlInv[10] * z + rotTrlInv[11] + .5f);
                        }
                        else {
                            *dataXp++ = (int16_t)(*data * *vectorsX++ + 0.5f);
                            *dataYp++ = (int16_t)(*data * *vectorsY++ + 0.5f);
                            *dataZp++ = (int16_t)(*data * *vectorsZ++ + 0.5f);
                        }
                    }
                    data++;
                }
                BTA_Status status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdX, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t*)dataX, pxCount * sizeof(int16_t),
                                                               0, 0, channel->lensIndex, channel->flags, channel->sequenceCounter, channel->gain);
                if (status != BTA_StatusOk) {
                    free(dataX);
                    dataX = 0;
                    free(dataY);
                    dataY = 0;
                    free(dataZ);
                    dataZ = 0;
                    //BTAinfoEventHelper(winst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel X");
                    return status;
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdY, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t*)dataY, pxCount * sizeof(int16_t),
                                                    0, 0, channel->lensIndex, channel->flags, channel->sequenceCounter, channel->gain);
                if (status != BTA_StatusOk) {
                    free(dataY);
                    dataY = 0;
                    free(dataZ);
                    dataZ = 0;
                    //BTAinfoEventHelper(winst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel Y");
                    return status;
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdZ, channel->xRes, channel->yRes, BTA_DataFormatSInt16, BTA_UnitMillimeter, channel->integrationTime, channel->modulationFrequency, (uint8_t*)dataZ, pxCount * sizeof(int16_t),
                                                    0, 0, channel->lensIndex, channel->flags, channel->sequenceCounter, channel->gain);
                if (status != BTA_StatusOk) {
                    free(dataZ);
                    dataZ = 0;
                    //BTAinfoEventHelper(winst->infoEventInst, 5, status, "BTAcalcXYZApply: Error adding channel Z");
                    return status;
                }
                continue;
            }
            else {
                return BTA_StatusNotSupported;
            }
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAcalcMonochromeFromAmplitude(BTA_Frame *frame) {
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        BTA_Channel *channel = frame->channels[chInd];
        if (channel->id != BTA_ChannelIdAmplitude || channel->xRes == 0 || channel->yRes == 0) {
            continue;
        }
        if (channel->dataFormat == BTA_DataFormatUInt16) {
            int pxCount = channel->xRes * channel->yRes;
            uint16_t *dataAmp = (uint16_t *)channel->data;
            uint8_t *dataMono = (uint8_t *)malloc(pxCount * sizeof(uint8_t));
            if (!dataMono) {
                return BTA_StatusOutOfMemory;
            }
            uint16_t ampMin = UINT16_MAX;
            uint16_t ampMax = 0;
            for (int xy = 0; xy < pxCount; xy++) {
                ampMin = *dataAmp < ampMin ? *dataAmp : ampMin;
                ampMax = *dataAmp > ampMax ? *dataAmp : ampMax;
                dataAmp++;
            }
            dataAmp = (uint16_t *)channel->data;
            uint8_t *data = dataMono;
            for (int xy = 0; xy < pxCount; xy++) {
                *data++ = (uint8_t)((*dataAmp++ - ampMin) * 255 / (ampMax - ampMin));
            }
            BTA_Status status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdColor, channel->xRes, channel->yRes, BTA_DataFormatUInt8, BTA_UnitUnitLess, 0, 0, dataMono, pxCount * sizeof(uint8_t),
                                                           0, 0, channel->lensIndex, channel->flags, channel->sequenceCounter, channel->gain);
            if (status != BTA_StatusOk) {
                free(dataMono);
                return status;
            }
            continue;
        }
        else {
            return BTA_StatusNotSupported;
        }
    }
    return BTA_StatusOk;
}


