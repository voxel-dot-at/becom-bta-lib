#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bta.h>
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
        case BTA_ChannelIdGrayScale:
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