
#include "bta_jpg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


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



BTA_Status BTAjpgInit(BTA_WrapperInst *winst) {
#   ifndef BTA_WO_LIBJPEG
        if (!winst) {
            return BTA_StatusInvalidParameter;
        }
        winst->jpgInst = (BTA_JpgInst *)calloc(1, sizeof(BTA_JpgInst));
        BTA_JpgInst *inst = winst->jpgInst;
        if (!inst) {
            return BTA_StatusOutOfMemory;
        }
        inst->enabled = 1;
#   endif
    return BTA_StatusOk;
}


BTA_Status BTAjpgClose(BTA_WrapperInst *winst) {
#   ifndef BTA_WO_LIBJPEG
        if (!winst) {
            return BTA_StatusInvalidParameter;
        }
        BTA_JpgInst *inst = winst->jpgInst;
        if (!inst) {
            // not even opened
            return BTA_StatusOk;
        }
        free(inst);
#   endif
    return BTA_StatusOk;
}

BTA_Status BTAjpgEnable(BTA_WrapperInst *winst, uint8_t enable) {
#   ifndef BTA_WO_LIBJPEG
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_JpgInst *inst = winst->jpgInst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    inst->enabled = enable;
    return BTA_StatusOk;
#   endif
    return BTA_StatusNotSupported;
}

BTA_Status BTAjpgIsEnabled(BTA_WrapperInst *winst, uint8_t *enabled) {
#   ifndef BTA_WO_LIBJPEG
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_JpgInst *inst = winst->jpgInst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (enabled) {
        *enabled = inst->enabled;
    }
    return BTA_StatusOk;
#   endif
    return BTA_StatusNotSupported;
}


BTA_Status BTAdecodeJpgToRgb24(BTA_WrapperInst *winst, uint8_t *dataIn, uint32_t dataInLen, uint8_t *dataOut, uint32_t dataOutLen) {
#   ifndef BTA_WO_LIBJPEG
        int err;
        if (!dataIn || !dataOut) {
            return BTA_StatusInvalidParameter;
        }
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        cinfo.err->output_message = jpgOutputMessage;
        jpeg_create_decompress(&cinfo);
        jpeg_mem_src(&cinfo, dataIn, dataInLen);
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
        uint8_t *dataTemp = dataOut;
        while (cinfo.output_scanline < cinfo.output_height) {
            err = jpeg_read_scanlines(&cinfo, buffer, 1);
            if (!err) {
                return BTA_StatusRuntimeError;
            }
            memcpy(dataTemp, buffer[0], rowStride);
            dataTemp += rowStride;
        }
        err = jpeg_finish_decompress(&cinfo);
        if (!err) {
            return BTA_StatusRuntimeError;
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

