#include "bta_jpg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mth_math.h>
#include <assert.h>

#ifndef BTA_WO_LIBJPEG
#include <jpeglib.h>

static void jpgOutputMessage(j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    //my_error_ptr myerr = (my_error_ptr)cinfo->err;
    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    //(*cinfo->err->output_message) (cinfo);
    /* Return control to the setjmp point */
    //longjmp(myerr->setjmp_buffer, 1);
}
#endif


static BTA_Status BTAdecodeJpgToRgb24(uint8_t *dataIn, uint32_t dataInLen, uint8_t *dataOut, uint32_t *dataOutLen, BTA_DataFormat *dataFormat) {
#   ifndef BTA_WO_LIBJPEG
    int err;
    if (!dataIn || !dataOut) {
        return BTA_StatusInvalidParameter;
    }
    if (!dataInLen) {
        // Empty image, nothing to decode
        return BTA_StatusOk;
    }
    struct jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(struct jpeg_decompress_struct));
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    cinfo.err->output_message = jpgOutputMessage;
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char *)dataIn, (unsigned long)dataInLen);
    err = jpeg_read_header(&cinfo, 1);
    if (err != JPEG_HEADER_OK) {
        return BTA_StatusRuntimeError;
    }
    err = jpeg_start_decompress(&cinfo);
    if (!err) {
        return BTA_StatusRuntimeError;
    }
    int rowStride = cinfo.output_width * cinfo.output_components;
    if (!rowStride) {
        return BTA_StatusRuntimeError;
    }
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, rowStride, 1);
    if (!buffer) {
        return BTA_StatusRuntimeError;
    }
    int bytesCopied = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        err = jpeg_read_scanlines(&cinfo, buffer, 1);
        if (!err) {
            return BTA_StatusRuntimeError;
        }
        int bytesToCopy = MTHmin(rowStride, (int)(*dataOutLen - bytesCopied));
        memcpy(dataOut + bytesCopied, buffer[0], bytesToCopy);
        bytesCopied += bytesToCopy;
    }
    err = jpeg_finish_decompress(&cinfo);
    if (!err) {
        return BTA_StatusRuntimeError;
    }
    *dataOutLen = bytesCopied;
    if (cinfo.output_components == 3 && cinfo.out_color_space == JCS_RGB) {
        *dataFormat = BTA_DataFormatRgb24;
    }
    else {
        assert(0);
        *dataFormat = BTA_DataFormatUnknown;
    }
    jpeg_destroy_decompress(&cinfo);
    return BTA_StatusOk;
#   else
    //memset(dataOut, 0, dataOutLen);
    //memcpy(dataOut, dataIn, dataInLen);
    //return BTA_StatusOk;
    return BTA_StatusNotSupported;
#   endif
}


BTA_Status BTAjpegFrameToRgb24(BTA_Frame *frame) {
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
#   ifndef BTA_WO_LIBJPEG
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        BTA_Channel *channel = frame->channels[chInd];
        if (channel->dataFormat == BTA_DataFormatJpeg) {
            uint32_t dataOutLen = channel->xRes * channel->yRes * 3;
            uint8_t *dataOut = (uint8_t *)malloc(dataOutLen);
            if (!dataOut) {
                return BTA_StatusOutOfMemory;
            }
            BTA_DataFormat dataFormat;
            BTA_Status status = BTAdecodeJpgToRgb24((uint8_t *)channel->data, channel->dataLen, dataOut, &dataOutLen, &dataFormat);
            if (status != BTA_StatusOk) {
                free(dataOut);
                dataOut = 0;
                continue;
            }
            free(channel->data);
            channel->dataLen = dataOutLen;
            channel->data = dataOut;
            channel->dataFormat = dataFormat;
        }
    }
    return BTA_StatusOk;
#   else
    return BTA_StatusNotSupported;
#   endif
}

