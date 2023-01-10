#include "calc_bilateral.h"
#include <bta_helper.h>
#include <stdlib.h>
#include <string.h>
#include <fastBF/shiftableBF.h>


BTA_Status BTAcalcBilateralApply(BTA_WrapperInst *winst, BTA_Frame *frame, uint8_t windowSize) {
    if (!winst || !frame || windowSize < 3 || (windowSize % 2) == 0) {
        return BTA_StatusInvalidParameter;
    }
    for (int chIn = 0; chIn < frame->channelsLen; chIn++) {
        BTA_Channel *channel = frame->channels[chIn];
        if (channel->id == BTA_ChannelIdDistance && channel->xRes > 0 && channel->yRes > 0) {
            int pxCount = channel->xRes * channel->yRes;
            if (channel->dataFormat == BTA_DataFormatUInt16) {
                float *dataCpy = (float *)malloc(pxCount * sizeof(float));
                if (!dataCpy) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "BTAcalcBilateralApply: out of memory");
                    continue;
                }

                uint16_t *src = (uint16_t *)channel->data;
                float *dst = dataCpy;
                for (int xy = 0; xy < pxCount; xy++) {
                    *dst++ = (float)*src++ / 1000.0f;
                }

                shiftableBFU16(dataCpy, (uint16_t *)channel->data, channel->yRes, channel->xRes, BILAT_SIGMA_S, BILAT_SIGMA_R, windowSize, (float)BILAT_TOL, 1000.0f);
                free(dataCpy);
            }
            else {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAcalcXYZApply: dataFormat %d not supported!", channel->dataFormat);
                return BTA_StatusNotSupported;
            }

        }
    }
    return BTA_StatusOk;
}