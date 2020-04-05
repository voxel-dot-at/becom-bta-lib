
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
        winst->jpgInst = (BTA_JpgInst *)malloc(sizeof(BTA_JpgInst));
        BTA_JpgInst *inst = winst->jpgInst;
        if (!inst) {
            return BTA_StatusOutOfMemory;
        }
        inst->enabled = 1;

        //for (int i = 0; i < 256; i++) {
        //    inst->buffer_1_402[i] = (i - 128)*INT_1_4020;
        //    inst->buffer_0_34414[i] = (i - 128)*INT_0_34414;
        //    inst->buffer_0_71414[i] = (i - 128)*INT_0_71414;
        //    inst->buffer_1_772[i] = (i - 128)*INT_1_7720;
        //}
        //inst->input = (InputStructure *)malloc(sizeof(InputStructure));
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
            return BTA_StatusOk;
        }
        //free(inst->input);
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























//
//#include <bta.h>
//#include <bta_oshelper.h>
////#include <assert.h>
//#include "bta_jpg.h"
//
//#include <stdlib.h>
//
//typedef struct tagDecodeStatusREGS {
//    char count;
//    uint8_t value;
//    int Predict[3];
//} DecodeStatusREGS;
//
//
//#define SLLCODE1(a) (a<<1)
//#define PRODUCT(a,b) (a*b)
//#define CLIP_UINT8(v) ((v) < 0 ? 0 : ((v) > 255 ? 255 : (v)))
//#define CLIP_SINT8(v) ((v) < -128 ? -128 : ((v) > 127 ? 127 : (v)))
//#define rShift 8
///* Notation :: ck = cos(k*pi/16) and sk = sin (k*pi/16)
//Used in the function iDct1x8() */
//#define INT_1_4142135624	362		//sqrt(2)
//#define INT_1_8477590650	473		//sqrt(2)*[c6+s6]
//#define INT_0_5411961001	139		//sqrt(2)*[c6]
//#define INT_1_3065629649	334		//sqrt(2)*[s6]
//#define INT_0_8314696123	213		//c3
//#define INT_0_5555702330	142		//s3
//#define INT_0_1950903220	50		//s1
//#define INT_0_9807852804	251		//c1
//#define INT_1_1758756024	301		//c1+s1
//#define INT_1_3870398453	355		//c3+s3
///* Used in YUVtoRGB */
//#define INT_1_4020			359		// 1.4020
//#define INT_0_34414			 88		// 0.34414
//#define INT_0_71414			183		// 0.71414
//#define INT_1_7720			454		// 1.772
//
//
//
//static BTA_Status YUVtoRGB(BTA_JpgInst *inst, uint8_t *dataOut, uint32_t dataOutLen) {
//    // This function receives yuv data and tranforms them in to rgb data.
//    //	Transformation equations used are
//    //	R = Y*1.0 + (Cr - 128)*1.4020
//    //	G = Y*1.0 - 0.3441*(Cb - 128)-(Cr - 128)*0.7141
//    //	B = Y*1.0 + (Cb - 128)*1.7720
//    //if (!inst) {
//    //    return BTA_StatusInvalidParameter;
//    //}
//    // assert(inst);
//    InputStructure *input = inst->input;
//    // assert(input);
//    uint32_t xRes = input->xRes;
//    uint32_t yRes = input->yRes;
//    if (dataOutLen != 3 * xRes * yRes) {
//        return BTA_StatusInvalidData;
//    }
//
//    for (uint32_t y = 0; y < yRes; y++) {
//        for (uint32_t x = 0; x < xRes; x++) {
//            *dataOut++ = CLIP_UINT8(input->out.streamY[y*input->extwidth + x] + ((inst->buffer_1_402[input->out.streamV[y*input->extwidth + x]] + (1 << (rShift - 1))) >> rShift));
//            *dataOut++ = CLIP_UINT8(input->out.streamY[y*input->extwidth + x] - ((inst->buffer_0_34414[input->out.streamU[y*input->extwidth + x]] + inst->buffer_0_71414[input->out.streamV[y*input->extwidth + x]] + (1 << (rShift - 1))) >> rShift));
//            *dataOut++ = CLIP_UINT8(input->out.streamY[y*input->extwidth + x] + ((inst->buffer_1_772[input->out.streamU[y*input->extwidth + x]] + (1 << (rShift - 1))) >> rShift));
//        }
//    }
//    return BTA_StatusOk;
//}
//
//
//static BTA_Status YUVtoMono(InputStructure *input, uint8_t *dataOut, uint32_t dataOutLen) {
//    //if (dataOutLen != input->xRes * input->yRes) {
//    //    return BTA_StatusInvalidData;
//    //}
//    // assert(dataOutLen == input->xRes * input->yRes);
//
//    uint8_t *streamYTemp = input->out.streamY;
//    for (uint32_t y = 0; y < input->yRes; y++) {
//        for (uint32_t x = 0; x < input->xRes; x++) {
//            *dataOut++ = *streamYTemp++;
//        }
//    }
//    return BTA_StatusOk;
//}
//
//
//static void RESET(DecodeStatusREGS *statusRegister) {
//    // Resets the status of decoder. It is invoked when engine receives a restart marker
//    statusRegister->count = 0;
//    statusRegister->Predict[0] = statusRegister->Predict[1] = statusRegister->Predict[2] = 0;
//    statusRegister->value = 0;
//}
//
//
//static void PushtoStream(InputStructure *input, uint8_t cIdentifier, int *arr) {
//    // The decoded data units are sent to output buffer.
//    int i;
//    int row, column;
//    if (cIdentifier == 0) {		// Y Stream
//        row = input->out.indexY[1];
//        column = input->out.indexY[0];
//        i = 0;
//        while (i < 64) {
//            input->out.streamY[row*input->extwidth + column + (i % 8)] = arr[i];
//            i += input->scalefactor;
//            if (i % 8 == 0) {
//                row += input->scalefactor;
//                i += ((input->scalefactor - 1) * 8);
//            }
//        }
//        input->out.indexY[0] += 8;
//        if (input->ns == 1) {		// Grayscale, Multi Scan Images with single component
//            if (input->out.indexY[0] == 8 * ((input->xRes + 7) / 8)) {
//                input->out.indexY[0] = 0;
//                input->out.indexY[1] += 8;
//            }
//        }
//        else {
//            if (input->inputFormat == YUV420) {
//                if (input->out.indexY[0]) {
//                    if (((input->out.indexY[0] / 8) % 2 == 0)) {
//                        if (((input->out.indexY[1] / 8) % 2 == 0)) {
//                            input->out.indexY[0] -= 16;
//                            input->out.indexY[1] += 8;
//                        }
//                        else if (((input->out.indexY[1] / 8) % 2 != 0)) {
//                            input->out.indexY[1] -= 8;
//                        }
//                    }
//                }
//            }
//        }
//        if (input->out.indexY[0] == input->extwidth) {
//            input->out.indexY[0] = 0;
//            input->out.indexY[1] += (8 * ((input->inputFormat == YUV420 && input->ns != 1) ? 2 : 1));
//        }
//    }
//    else {
//        uint8_t *streamptr;
//        uint32_t *indexptr, j;
//        if (cIdentifier == 6) {	// U Stream
//            streamptr = input->out.streamU;
//            indexptr = input->out.indexU;
//        }
//        else {	// V Stream
//            streamptr = input->out.streamV;
//            indexptr = input->out.indexV;
//        }
//        i = j = 0;
//        row = indexptr[1];
//        column = indexptr[0];
//        while (i < 64) {
//            streamptr[row*input->extwidth + column + j] = arr[i];
//            if (input->inputFormat != YUV444) {
//                streamptr[row*input->extwidth + column + j + input->scalefactor] = arr[i];
//                if (input->inputFormat == YUV420) {
//                    streamptr[(row + input->scalefactor)*input->extwidth + column + j] = arr[i];
//                    streamptr[(row + input->scalefactor)*input->extwidth + column + j + input->scalefactor] = arr[i];
//                }
//                j += input->scalefactor;
//            }
//            j += input->scalefactor;
//            i += input->scalefactor;
//            if (i % 8 == 0) {
//                j = 0;
//                i += ((input->scalefactor - 1) * 8);
//                row += (input->scalefactor*((input->inputFormat == YUV420) ? 2 : 1));
//            }
//        }
//        indexptr[0] += (8 * ((input->inputFormat == YUV444) ? 1 : 2));
//        if (indexptr[0] == input->extwidth) {
//            indexptr[0] = 0;
//            indexptr[1] += (8 * ((input->inputFormat == YUV420) ? 2 : 1));
//        }
//    }
//}
//
//
//static void ImageDownScaling(InputStructure *input, int *arr) {
//    // Scale the image Down by averaging
//    int(*arr_)[8], i, j, sum;
//    uint8_t loop1, loop2;
//    arr_ = (int(*)[8])arr;
//    if (input->scalefactor == 1);
//    else if (input->scalefactor == 8) {
//        *arr = (*arr + 4) >> 3;
//        *arr = CLIP_SINT8(*arr);
//        *arr += 128;
//    }
//    else {
//        for (i = 0; i < 8; i += input->scalefactor) {
//            for (j = 0; j < 8; j += input->scalefactor) {
//                sum = 0;
//                for (loop1 = 0; loop1 < input->scalefactor; loop1++)
//                    for (loop2 = 0; loop2 < input->scalefactor; loop2++)
//                        sum = sum + arr_[i + loop1][j + loop2];
//                sum = (sum + ((input->scalefactor*input->scalefactor) >> 1)) / (input->scalefactor*input->scalefactor);
//                arr_[i][j] = sum;
//            }
//        }
//    }
//}
//
//
//static void LevelShift(int *arr) {
//    // performs Level shift
//    int i = 0;
//    while (i < 64) {
//        arr[i] = arr[i] + 128;	// Level Shift
//        arr[i] = CLIP_UINT8(arr[i]);
//        i++;
//    }
//}
//
//
//static void IDct1x8(int *mcuRorC) {
//    // This function performs the 1-D 8 point Inverse DCT.
//    // The following implementation is loosely based on the paper "A Low Complexity Implementation of a Fast BinDCT", by S. Timakul, S. Chuntree and S. Choomchuay.
//    // All the floating point calculations are substituted with integer operations at a precision of 8 bits.
//
//    int stageVal[10];
//
//    // Stage 1 [Even Calcs]
//    stageVal[1] = (mcuRorC[1] + mcuRorC[7]) << rShift;
//    stageVal[7] = (mcuRorC[1] - mcuRorC[7]) << rShift;
//    stageVal[3] = PRODUCT(INT_1_4142135624, mcuRorC[3]);
//    stageVal[5] = PRODUCT(INT_1_4142135624, mcuRorC[5]);
//    // Stage 1 [Odd Calcs]
//
//    stageVal[0] = (mcuRorC[0] + mcuRorC[4]) << rShift;
//    stageVal[4] = (mcuRorC[0] - mcuRorC[4]) << rShift;
//    stageVal[8] = PRODUCT(mcuRorC[2], INT_1_8477590650);
//    stageVal[2] = PRODUCT(-(mcuRorC[2] + mcuRorC[6]), INT_1_3065629649) + stageVal[8];
//    stageVal[6] = PRODUCT((-mcuRorC[2] + mcuRorC[6]), INT_0_5411961001) + stageVal[8];
//    // Stage 2 [Even Calcs]
//    stageVal[8] = stageVal[7];
//    stageVal[7] = stageVal[8] + stageVal[5];
//    stageVal[5] = stageVal[8] - stageVal[5];
//    stageVal[8] = stageVal[1];
//    stageVal[1] = stageVal[8] + stageVal[3];
//    stageVal[3] = stageVal[8] - stageVal[3];
//    // Stage 2 [Odd Calcs]
//
//    stageVal[8] = stageVal[0];
//    stageVal[0] = stageVal[8] + stageVal[6];
//    stageVal[6] = stageVal[8] - stageVal[6];
//    stageVal[8] = stageVal[4];
//    stageVal[4] = stageVal[8] + stageVal[2];
//    stageVal[2] = stageVal[8] - stageVal[2];
//    // Stage 3 [Even Calcs]
//    stageVal[8] = stageVal[7];
//    stageVal[9] = PRODUCT(stageVal[8], INT_1_3870398453);
//    stageVal[7] = (PRODUCT(-(stageVal[8] + stageVal[1]), INT_0_5555702330) + stageVal[9] + (1 << (rShift - 1))) >> rShift;
//    stageVal[1] = (PRODUCT((-stageVal[8] + stageVal[1]), INT_0_8314696123) + stageVal[9] + (1 << (rShift - 1))) >> rShift;
//    stageVal[8] = stageVal[3];
//    stageVal[9] = PRODUCT(stageVal[8], INT_1_1758756024);
//    stageVal[3] = (PRODUCT(-(stageVal[8] + stageVal[5]), INT_0_1950903220) + stageVal[9] + (1 << (rShift - 1))) >> rShift;
//    stageVal[5] = (PRODUCT((-stageVal[8] + stageVal[5]), INT_0_9807852804) + stageVal[9] + (1 << (rShift - 1))) >> rShift;
//    // Stage 3 [Odd Calcs]
//
//    stageVal[8] = stageVal[0];
//    mcuRorC[0] = (stageVal[8] + stageVal[1] + (1 << (rShift - 1))) >> rShift;
//    mcuRorC[7] = (stageVal[8] - stageVal[1] + (1 << (rShift - 1))) >> rShift;
//    stageVal[8] = stageVal[2];
//    mcuRorC[2] = (stageVal[8] + stageVal[3] + (1 << (rShift - 1))) >> rShift;
//    mcuRorC[5] = (stageVal[8] - stageVal[3] + (1 << (rShift - 1))) >> rShift;
//    stageVal[8] = stageVal[4];
//    mcuRorC[1] = (stageVal[8] + stageVal[5] + (1 << (rShift - 1))) >> rShift;
//    mcuRorC[6] = (stageVal[8] - stageVal[5] + (1 << (rShift - 1))) >> rShift;
//    stageVal[8] = stageVal[6];
//    mcuRorC[3] = (stageVal[8] + stageVal[7] + (1 << (rShift - 1))) >> rShift;
//    mcuRorC[4] = (stageVal[8] - stageVal[7] + (1 << (rShift - 1))) >> rShift;
//}
//
//
//static void InverseDCT(int *arr) {
//    // performs 2-D IDCT for 8x8 block
//    int i, j;
//    int _arr[8];
//    for (i = 0; i < 8; i++)		// row dct
//        IDct1x8(arr + 8 * i);
//    for (i = 0; i < 8; i++) {		// column dct
//        for (j = 0; j < 8; j++)
//            _arr[j] = arr[i + j * 8];
//        IDct1x8(_arr);
//        for (j = 0; j < 8; j++)
//            arr[i + j * 8] = _arr[j];
//    }
//    for (i = 0; i < 64; i++) {
//        arr[i] += 4;
//        arr[i] >>= 3;
//    }
//}
//
//
//static void DeQuant(InputStructure *input, uint8_t tIdentifier, int *arr) {
//    // Performs Inverse Quantiztion and removes the zig zig scan order
//    int i, reOrder[64];
//    uint8_t ZigZag[64] = {
//        0,  1,  5,  6, 14, 15, 27, 28,
//        2,  4,  7, 13, 16, 26, 29, 42,
//        3,  8, 12, 17, 25, 30, 41, 43,
//        9, 11, 18, 24, 31, 40, 44, 53,
//        10, 19, 23, 32, 39, 45, 52, 54,
//        20, 22, 33, 38, 46, 51, 55, 60,
//        21, 34, 37, 47, 50, 56, 59, 61,
//        35, 36, 48, 49, 57, 58, 62, 63
//    };
//    for (i = 0; i < 64; i++)
//        arr[i] = arr[i] * input->quantTables[tIdentifier][i];
//    for (i = 0; i < 64; i++)
//        reOrder[i] = arr[ZigZag[i]];
//    for (i = 0; i < 64; i++)
//        arr[i] = reOrder[i];
//}
//
//
//static int Extend(int CODE, int VALUE) {
//    //Returns Value for the Code
//    if (VALUE == 0) {
//    }
//    else if (CODE < (1 << (VALUE - 1))) {
//        CODE = CODE + ((-1) << VALUE) + 1;
//    }
//    return CODE;
//}
//
//
//inline static int NextBit(uint8_t **ipPtr, InputStructure *input, DecodeStatusREGS *statusRegister) {
//    // Returns the next bit in the byte stream
//    int CODE = 0;
//    if (statusRegister->count < 0) {
//        // We might have hit some marker. So stop reading the stream and exit scan decode
//    }
//    else {
//        if (statusRegister->count == 0) {
//            statusRegister->value = **ipPtr;
//            (*ipPtr) += 1;
//            if (statusRegister->value == 0xFF) {
//                if (**ipPtr == 0x00)	/*Stuff Byte*/
//                    (*ipPtr) += 1;
//                else {					/*Some Marker*/
//                    (*ipPtr) -= 1;
//                    statusRegister->count = -1;
//                    return 0;
//                }
//            }
//            statusRegister->count = 8;
//        }
//        CODE = statusRegister->value >> 7;
//        statusRegister->value = SLLCODE1(statusRegister->value);
//        statusRegister->count--;
//    }
//    return CODE;
//}
//
//
//static void DecodeAC(uint8_t **ipPtr, InputStructure *input, DecodeStatusREGS *statusRegister, uint8_t tIdentifier, int *arr) {
//    // Decodes AC value(s)
//    uint8_t index = 0, VALUE, RRRR, K = 1;
//    int CODE, AC;
//    uint32_t J;
//
//    CODE = NextBit(ipPtr, input, statusRegister);
//    if (statusRegister->count < 0)
//        return;
//    while (1) {
//        if (CODE > input->acCode.MAXCODE[tIdentifier][index]) {
//            index++;
//            CODE = SLLCODE1(CODE) + NextBit(ipPtr, input, statusRegister);
//            if (statusRegister->count < 0)
//                return;
//        }
//        else {
//            J = input->acCode.VALPTR[tIdentifier][index];
//            J = J - input->acCode.MINCODE[tIdentifier][index] + CODE;
//            VALUE = input->acCode.VALUES[tIdentifier][J];
//            RRRR = VALUE >> 4;		//Runs of Zeros
//            VALUE = VALUE % 16;		//SSSS
//            if (RRRR == 0) {
//                if (VALUE == 0)		// EOB Block
//                    break;
//                else {
//                    AC = 0;
//                    for (J = 0; J < VALUE; J++) {
//                        AC = SLLCODE1(AC) + NextBit(ipPtr, input, statusRegister);
//                        if (statusRegister->count < 0)
//                            return;
//                    }
//                    AC = Extend(AC, VALUE);
//                    arr[K] = AC;
//                    K += 1;
//                }
//
//            }
//            else {
//                K += RRRR;
//                AC = 0;
//                for (J = 0; J < VALUE; J++) {
//                    AC = SLLCODE1(AC) + NextBit(ipPtr, input, statusRegister);
//                    if (statusRegister->count < 0)
//                        return;
//                }
//                AC = Extend(AC, VALUE);
//                arr[K] = AC;
//                K += 1;
//            }
//            if (K == 64)
//                break;
//            CODE = NextBit(ipPtr, input, statusRegister);
//            if (statusRegister->count < 0)
//                return;
//            index = 0;
//        }
//    }
//}
//
//
//static void DecodeDC(uint8_t **ipPtr, InputStructure *input, DecodeStatusREGS *statusRegister, uint8_t tIdentifier, int *arr) {
//    // Decodes DC value
//    int CODE, DC;
//    uint8_t index = 0, VALUE;
//    uint32_t J;
//
//    CODE = NextBit(ipPtr, input, statusRegister);
//    if (statusRegister->count < 0)
//        return;
//    while (1) {
//        if (CODE > input->dcCode.MAXCODE[tIdentifier][index]) {
//            index++;
//            CODE = SLLCODE1(CODE) + NextBit(ipPtr, input, statusRegister);
//            if (statusRegister->count < 0)
//                return;
//        }
//        else {
//            J = input->dcCode.VALPTR[tIdentifier][index];
//            J = J - input->dcCode.MINCODE[tIdentifier][index] + CODE;
//            VALUE = input->dcCode.VALUES[tIdentifier][J];
//            DC = 0;
//            for (J = 0; J < VALUE; J++) {
//                DC = SLLCODE1(DC) + NextBit(ipPtr, input, statusRegister);
//                if (statusRegister->count < 0)
//                    return;
//            }
//            arr[0] += Extend(DC, VALUE);
//            break;
//        }
//    }
//}
//
//
//inline static void DecodeDataUnit(uint8_t **ipPtr, InputStructure *input, DecodeStatusREGS *statusRegister, uint8_t cIdentifier) {
//    // Decode data unit
//    int arr[64] = { 0 };
//
//    DecodeDC(ipPtr, input, statusRegister, input->componentAttributes[cIdentifier + 4], arr);	// Decode DC
//    statusRegister->Predict[cIdentifier / 6] += arr[0];	// Inverse Prediction
//    arr[0] = statusRegister->Predict[cIdentifier / 6];
//    DecodeAC(ipPtr, input, statusRegister, input->componentAttributes[cIdentifier + 5], arr);	// Decode AC
//    if (statusRegister->count >= 0) {
//        DeQuant(input, input->componentAttributes[cIdentifier + 3], arr);	// DeQuant
//        if (input->scalefactor != 8) {
//            InverseDCT(arr);	// InverseDCT
//            LevelShift(arr);	// UnBias
//        }
//        ImageDownScaling(input, arr);
//        PushtoStream(input, cIdentifier, arr);	// Write to Output Buffer
//    }
//}
//
//
//static void DecodeMCUs(uint8_t **ipPtr, InputStructure *input, DecodeStatusREGS *statusRegister) {
//    // Decode all the Data Units in an MCU
//    if (input->multiScan) {
//        if (input->ns == 1) {
//            DecodeDataUnit(ipPtr, input, statusRegister, (input->multiScan) >> 1);	// Y|U|V
//        }
//        if (input->ns == 2) {
//            if ((input->multiScan >> 1) == 0) {
//                DecodeDataUnit(ipPtr, input, statusRegister, 0);	// Y0
//                if (input->inputFormat == YUV422 || input->inputFormat == YUV420) {
//                    DecodeDataUnit(ipPtr, input, statusRegister, 0);	// Y1
//                    if (input->inputFormat == YUV420) {
//                        DecodeDataUnit(ipPtr, input, statusRegister, 0);	// Y2
//                        DecodeDataUnit(ipPtr, input, statusRegister, 0);	// Y3
//                    }
//                }
//                DecodeDataUnit(ipPtr, input, statusRegister, 6);	// U0
//            }
//            if ((input->multiScan >> 1) == 6) {
//                DecodeDataUnit(ipPtr, input, statusRegister, 6);	// U0
//                DecodeDataUnit(ipPtr, input, statusRegister, 12);	// V0
//            }
//        }
//    }
//    else {
//        DecodeDataUnit(ipPtr, input, statusRegister, 0);		// Y0
//        if (input->inputFormat != YUV400) {
//            if (input->inputFormat == YUV422 || input->inputFormat == YUV420) {
//                DecodeDataUnit(ipPtr, input, statusRegister, 0);	// Y1
//                if (input->inputFormat == YUV420) {
//                    DecodeDataUnit(ipPtr, input, statusRegister, 0);	// Y2
//                    DecodeDataUnit(ipPtr, input, statusRegister, 0);	// Y3
//                }
//            }
//            DecodeDataUnit(ipPtr, input, statusRegister, 6);	// U0
//            DecodeDataUnit(ipPtr, input, statusRegister, 12);	// V0
//        }
//    }
//}
//
//
//static void ExpectedMCUCount(InputStructure *input) {
//    // This function returns the number of MCU's expected in the current scan.
//    int count = 0;
//    YUVFormat format = YUV444;
//
//    if (input->multiScan) {
//        if (input->ns == 1) {
//            if ((input->multiScan >> 1) > 0)
//                format = input->inputFormat;
//            else
//                format = YUV444;
//        }
//        else if (input->ns == 2)
//            format = input->inputFormat;
//    }
//    else {
//        format = input->inputFormat;
//    }
//    if (format == YUV400 || format == YUV444)
//        count = ((input->xRes + 7) / 8)*((input->yRes + 7) / 8);
//    else if (format == YUV422)
//        count = ((input->xRes + 15) / 16)*((input->yRes + 7) / 8);
//    else if (format == YUV420)
//        count = ((input->xRes + 15) / 16)*((input->yRes + 15) / 16);
//
//    input->expectedMCUCount = count;
//}
//
//
//static void DecodeScan(uint8_t **ipPtr, InputStructure *input) {
//    // The objective of this function is to decode the current scan data. This is accomplished by decoding all the MCUs in all restart intervals.
//    uint32_t MCUCount = 0, restartIntervalCount, noofMCUstoDecode;
//    DecodeStatusREGS statusRegister = { 0, 0, { 0, 0, 0 } };
//
//    ExpectedMCUCount(input);
//
//    // Compute the number of restart Intervals
//    if (input->restartInterval)
//        restartIntervalCount = (input->expectedMCUCount + input->restartInterval - 1) / input->restartInterval;
//    else
//        restartIntervalCount = 1;
//
//    // The outer loop extends until all the restart markers in the scan are completed
//    while (restartIntervalCount > 0) {
//
//        // If restart marker is enabled, the number of MCUs in the current restart interval is length of the restart interval. This might not valid for the final restart marker.
//        noofMCUstoDecode = ((input->restartInterval == 0) ? input->expectedMCUCount : input->restartInterval);
//        if (restartIntervalCount == 1)
//            noofMCUstoDecode = input->expectedMCUCount - MCUCount;
//
//        // The inner loop extends until all the MCUs in the restart interval are completed
//        while (noofMCUstoDecode > 0) {
//            DecodeMCUs(ipPtr, input, &statusRegister);
//            if (statusRegister.count >= 0) {
//                MCUCount++;
//                noofMCUstoDecode--;
//            }
//            else {	// Some marker must have been encountered, but reached the end of scan.
//                // assert(**ipPtr == 0xff);
//                // This assertion validates that we have decoded all the MCUs for the scan. Hence we forcefully end the scan decode by pushing noofMCUstoDecode to zero.
//                // assert(input->expectedMCUCount == MCUCount);
//                noofMCUstoDecode = 0;
//            }
//        }
//        restartIntervalCount--;
//        if (restartIntervalCount) {		// Restart Marker, so Reset the status registers
//            // assert(**ipPtr == 0xff);
//            // assert((*(*ipPtr + 1)) >= 0xd0 && (*(*ipPtr + 1)) <= 0xd7);
//            (*ipPtr) += 2;
//            RESET(&statusRegister);
//        }
//    }
//}
//
//
//static void SOS(uint8_t **ipPtr, InputStructure *input) {
//    // Start of scan marker.
//    // Post this marker, the actual huffman stream is written. Hence upon decoding this marker, we invoke other functions which decode the scan data and write the output into a buffer
//    int i, j;
//    // int Length;
//
//    // Length = (**ipPtr * 256)+(*((*ipPtr)+1));
//    (*ipPtr) += 2;
//
//    input->ns = **ipPtr;				// Number of Scan Components
//    (*ipPtr) += 1;
//
//    if (input->ns != input->nf)			// Multi-scan Image
//        input->multiScan = 1;
//
//    for (i = 0; i < input->ns; i++, (*ipPtr) += 2) {
//        j = 0;
//        while (1) {
//            if (input->componentAttributes[j + 0] == **ipPtr) {
//                if (input->multiScan)
//                    if (i == 0)
//                        input->multiScan |= (j << 1);		/*Scan Component Index[7bits]|multiScan[1bit]*/
//                input->componentAttributes[j + 4] = *((*ipPtr) + 1) >> 4;	// Td
//                input->componentAttributes[j + 5] = *((*ipPtr) + 1) & 0x0F;	// Ta
//                break;
//            }
//            else
//                j += 6;
//        }
//    }
//    // assert(**ipPtr == 0);	// Ss
//    (*ipPtr) += 1;
//    // assert(**ipPtr == 63);	// Se
//    (*ipPtr) += 1;
//    // assert(**ipPtr == 0);	// Ah|Al
//    (*ipPtr) += 1;
//
//    // Decode huffman stream
//    DecodeScan(ipPtr, input);
//}
//
//
//inline static void DRI(uint8_t **ipPtr, InputStructure *input) {
//    // Restart Interval
//    // assert((**ipPtr * 256) + (*((*ipPtr) + 1)) == 4);
//    (*ipPtr) += 2;
//    input->restartInterval = (**ipPtr * 256) + (*((*ipPtr) + 1));
//    (*ipPtr) += 2;
//}
//
//
//static void GenerateDecodeTables(InputStructure *input, uint8_t tc, uint8_t th) {
//    // Organize Huffman Codes to make the decoding process easier
//    int i, index = 0;
//    if (tc == 0) {
//        for (i = 0; i < 16; i++) {
//            if (input->dcCode.BITS[th][i] == 0) {
//                input->dcCode.MAXCODE[th][i] = -1;
//            }
//            else {
//                input->dcCode.MINCODE[th][i] = input->dcCode.CODE[th][index];
//                input->dcCode.VALPTR[th][i] = index;
//                index = index + input->dcCode.BITS[th][i] - 1;
//                input->dcCode.MAXCODE[th][i] = input->dcCode.CODE[th][index];
//                index += 1;
//            }
//        }
//    }		//DC Table
//    else {
//        for (i = 0; i < 16; i++) {
//            if (input->acCode.BITS[th][i] == 0) {
//                input->acCode.MAXCODE[th][i] = -1;
//            }
//            else {
//                input->acCode.MINCODE[th][i] = input->acCode.CODE[th][index];
//                input->acCode.VALPTR[th][i] = index;
//                index = index + input->acCode.BITS[th][i] - 1;
//                input->acCode.MAXCODE[th][i] = input->acCode.CODE[th][index];
//                index += 1;
//            }
//        }
//    }		//AC Table
//}
//
//
//static void GenerateCodeTable(InputStructure *input, uint8_t tc, uint8_t th) {
//    // Huffman Codes Generation
//    int index = 0, code = 0, length = 1;
//    if (tc == 0) {
//        while (1) {
//            if (length == input->dcCode.SIZE[th][index]) {
//                input->dcCode.CODE[th][index] = code;
//                code += 1;
//                index += 1;
//            }
//            else {
//                if (input->dcCode.SIZE[th][index] == 0)
//                    break;
//                code = SLLCODE1(code);
//                length += 1;
//            }
//        }
//    }		//DC Table
//    else {
//        while (1) {
//            if (length == input->acCode.SIZE[th][index]) {
//                input->acCode.CODE[th][index] = code;
//                code += 1;
//                index += 1;
//            }
//            else {
//                if (input->acCode.SIZE[th][index] == 0)
//                    break;
//                code = SLLCODE1(code);
//                length += 1;
//            }
//        }
//    }		//AC Table
//}
//
//
//static void GenerateSizeTable(InputStructure *input, uint8_t tc, uint8_t th) {
//    int i, count, index;
//    if (tc == 0) {
//        for (i = 0, index = 0; i < 16; i++) {
//            count = input->dcCode.BITS[th][i];
//            while (count > 0) {
//                input->dcCode.SIZE[th][index] = i + 1;
//                index++;
//                count--;
//            }
//        }
//    }		//DC Table
//    else {
//        for (i = 0, index = 0; i < 16; i++) {
//            count = input->acCode.BITS[th][i];
//            while (count > 0) {
//                input->acCode.SIZE[th][index] = i + 1;
//                index++;
//                count--;
//            }
//        }
//    }		//AC Table
//}
//
//
//static void GenerateHuffmanTables(InputStructure *input, uint8_t tc, uint8_t th) {
//    //Generates necessary tables for Huffman Decoding
//    GenerateSizeTable(input, tc, th);
//    GenerateCodeTable(input, tc, th);
//    GenerateDecodeTables(input, tc, th);
//}
//
//
//static void DHT(uint8_t **ipPtr, InputStructure *input) {
//    // Huffman tables marker
//    uint32_t Length = (**ipPtr * 256) + (*((*ipPtr) + 1));
//    uint8_t tc, th;
//    (*ipPtr) += 2;
//    Length -= 2;
//
//    while (Length > 0) {
//        tc = **ipPtr >> 4;		// Table class
//        th = **ipPtr & 0x0F;	// Table destination identifier
//        (*ipPtr) += 1;
//        Length -= 1;
//        if (tc == 0) {		//DC Table
//            int i, m;
//            for (i = 0, m = 0; i < 16; i++, (*ipPtr)++) {
//                input->dcCode.BITS[th][i] = **ipPtr;	// Number of Huff Codes of Length i
//                m += (**ipPtr);
//            }
//            Length -= 16;
//            for (i = 0; i < m; i++, (*ipPtr)++) {
//                input->dcCode.VALUES[th][i] = **ipPtr;	// HUFFVAL
//            }
//            Length -= m;
//        }
//        else {				// AC Table
//            int i, m;
//            for (i = 0, m = 0; i < 16; i++, (*ipPtr)++) {
//                input->acCode.BITS[th][i] = **ipPtr;	// Number of Huff Codes of Length i
//                m += (**ipPtr);
//            }
//            Length -= 16;
//            for (i = 0; i < m; i++, (*ipPtr)++) {
//                input->acCode.VALUES[th][i] = **ipPtr;	// HUFFVAL
//            }
//            Length -= m;
//        }
//        // Upon extracting the tables from the marker generate huffman codes and re-order them such that decoding gets easier
//        GenerateHuffmanTables(input, tc, th);
//    }
//}
//
//
//static void DQT(uint8_t **ipPtr, InputStructure *input) {
//    // Quantization table marker
//    int NoofTables;
//    int Length = (**ipPtr * 256) + (*((*ipPtr) + 1));
//
//    Length -= 2;
//    NoofTables = Length / 65;
//    (*ipPtr) += 2;
//    while (NoofTables > 0) {
//        int index, i;
//        // assert((**ipPtr >> 4) == 0);	// Quantization table element precision
//        index = **(ipPtr) & 0x0F;		// Quantization table destination identifier
//        (*ipPtr) += 1;
//        for (i = 0; i < 64; i++, (*ipPtr) += 1)
//            input->quantTables[index][i] = **ipPtr;	// Quant Values
//        NoofTables--;
//    }
//}
//
//
//static BTA_Status SOF0(uint8_t **ipPtr, InputStructure *input) {
//    // Start of Frame Marker (baseline DCT)
//    // Ideally jpeg baseline DCT Profile does not impose any restriction on the number of components present in a jpeg image, the current codec imposes restrictions. This software supports decoding images with only 1 component (Gray scale) or 3 Components (YUV). Component count other than these (like CMYK, ...) are not supported. Also this software can only decode images encoded with internal color formats YUV400, YUV422, YUV420 and YUV444.
//    int i;
//    // int Length;
//
//    // Length = (**ipPtr * 256)+(*((*ipPtr)+1));
//    (*ipPtr) += 2;
//
//    // assert(**ipPtr == 8);						/*Precision*/
//
//    input->bitDepth = **ipPtr;		/*bitDepth*/
//    (*ipPtr) += 1;
//
//    if (!input->DNL) {
//        /* If DNL Marker is set by this time height information must also be read */
//        input->yRes = (**ipPtr * 256) + (*((*ipPtr) + 1));	/*Height*/
//    }
//    (*ipPtr) += 2;
//
//    input->xRes = (**ipPtr * 256) + (*((*ipPtr) + 1));	/*Width*/
//    (*ipPtr) += 2;
//
//    // assert(**ipPtr == 3 || **ipPtr == 1);
//    input->nf = **ipPtr;						/*Number of Components*/
//    (*ipPtr) += 1;
//
//    input->componentAttributes = (uint8_t*)calloc(input->nf * 6, sizeof(uint8_t));
//    if (!input->componentAttributes) {
//        return BTA_StatusOutOfMemory;
//        //printf("stderr :: Unable to allocate memory for component attributes ");
//        //CleanUp(input);
//        //exit(1);
//    }
//    else {
//        int index = 0;
//        for (i = 0; i < input->nf; i++, (*ipPtr) += 3, index += 6) {
//            input->componentAttributes[index + 0] = (**ipPtr);		//Ci
//            input->componentAttributes[index + 1] = (*((*ipPtr) + 1)) >> 4;	// Hi
//            input->componentAttributes[index + 2] = (*((*ipPtr) + 1)) & 0x0F;	// Vi
//            input->componentAttributes[index + 3] = (*((*ipPtr) + 2));		// Tqi
//        }
//        // The image variables extwidth and extheight represents the number of columns and rows encoded in the given jpeg image. [ Width and Height post padding]
//        if (input->nf == 1) {
//            input->inputFormat = YUV400;
//            input->extheight = 8 * ((input->yRes + 7) / 8);
//            input->extwidth = 8 * ((input->xRes + 7) / 8);
//        }
//        else {
//            if (input->componentAttributes[7] == 1 && input->componentAttributes[8] == 1 && input->componentAttributes[13] == 1 && input->componentAttributes[14] == 1) {
//                if (input->componentAttributes[1] == 2 && input->componentAttributes[2] == 2) {
//                    input->inputFormat = YUV420;
//                    input->extheight = 16 * ((input->yRes + 15) / 16);
//                    input->extwidth = 16 * ((input->xRes + 15) / 16);
//                }
//                else if (input->componentAttributes[1] == 2 && input->componentAttributes[2] == 1) {
//                    input->inputFormat = YUV422;
//                    input->extheight = 8 * ((input->yRes + 7) / 8);
//                    input->extwidth = 16 * ((input->xRes + 15) / 16);
//                }
//                else if (input->componentAttributes[1] == 1 && input->componentAttributes[2] == 1) {
//                    input->inputFormat = YUV444;
//                    input->extheight = 8 * ((input->yRes + 7) / 8);
//                    input->extwidth = 8 * ((input->xRes + 7) / 8);
//                }
//                else {
//                    return BTA_StatusInvalidData;
//                    //printf("The current Codec supports decoding images encoded with internal color formats :: YUV 400, YUV 422, YUV 444, YUV 420 ");
//                    //CleanUp(input);
//                    //exit(1);
//                }
//            }
//            else {
//                return BTA_StatusInvalidData;
//                //printf("The current Codec supports decoding images encoded with internal color formats :: YUV 400, YUV 422, YUV 444, YUV 420 ");
//                //CleanUp(input);
//                //exit(1);
//            }
//        }
//        // Allocate memory for the output stream
//        input->out.streamY = (uint8_t*)malloc(sizeof(char)*input->extheight*input->extwidth);
//        if (!input->out.streamY) {
//            return BTA_StatusOutOfMemory;
//            //printf("stderr :: Unable to allocate memory for Output");
//            //CleanUp(input);
//            //exit(1);
//        }
//        if (input->nf == 3) {
//            input->out.streamU = (uint8_t*)malloc(sizeof(char)*input->extheight*input->extwidth);
//            input->out.streamV = (uint8_t*)malloc(sizeof(char)*input->extheight*input->extwidth);
//            if ((!input->out.streamU) || (!input->out.streamV)) {
//                return BTA_StatusOutOfMemory;
//                //printf("stderr :: Unable to allocate memory for Output ");
//                //CleanUp(input);
//                //exit(1);
//            }
//        }
//    }
//    return BTA_StatusOk;
//}
//
//
//static void COMM(uint8_t **ipPtr) {
//    // Comment marker
//    // Does nothing. Just skips ahead
//    int Length = (**ipPtr * 256) + (*((*ipPtr) + 1));
//    (*ipPtr) += Length;
//}
//
//
//static void APPn(uint8_t **ipPtr) {
//    // The file format known as "JPEG Interchange Format" (JIF) is specified in Annex B of the standard (ITU T.81). However, this "pure" file format is rarely used, primarily because of the difficulty of programming encoders and decoders that fully implement all aspects of the standard and because of certain shortcomings of the standard like Color space definition, Component sub-sampling registration, Pixel aspect ratio definition.
//    // Several additional standards have evolved to address these issues. The first of these, released in 1992, was JPEG File Interchange Format (or JFIF), followed in recent years by Exchangeable image file format (Exif) and ICC color profiles. Both of these formats use the actual JIF byte layout, consisting of different markers, but in addition employ one of the JIF standard's extension points, namely the application markers: JFIF use APP0, while Exif use APP1.
//    // The Current codec does not support decoding any of these APP Segments. It ignores the content inside this marker and skips ahead.
//    int Length = (**ipPtr * 256) + (*((*ipPtr) + 1));
//    (*ipPtr) += Length;
//}
//
//
//inline static BTA_Status AnalyzeMarker(uint8_t** ipPtr, InputStructure *input) {
//    // This module detects and parses the marker(s)
//    // If the marker is SOS, subsequently it invokes other routines to decode the scan data
//    BTA_Status status = BTA_StatusOk;
//
//    if (**ipPtr == 0xD8) {			/*Start of Image*/
//        (*ipPtr)++;
//        input->soi = 1;
//        input->restartInterval = 0;
//    }
//    else if (**ipPtr == 0xFF) {		/*Stuff Byte*/
//                                    /*do Nothing*/
//    }
//    else if (**ipPtr == 0xD9) {		/*End of Image*/
//        // assert(input->soi == 1);
//        input->eoi = 1;
//    }
//    else if (**ipPtr >= 0xE0 && **ipPtr <= 0xEF) {
//        // assert(input->soi == 1);	/*APP Marker*/
//        (*ipPtr)++;
//        APPn(ipPtr);
//    }
//    else if (**ipPtr == 0xFE) {		/*Comment*/
//        // assert(input->soi == 1);
//        (*ipPtr)++;
//        COMM(ipPtr);
//    }
//    else if (**ipPtr == 0xC0) {		/*Baseline, Start of Frame*/
//        // assert(input->soi == 1);
//        (*ipPtr)++;
//        status = SOF0(ipPtr, input);
//    }
//    else if (**ipPtr == 0xDC) {		/*DNL*/
//        // assert(input->soi == 1);
//        (*ipPtr)++;
//        // Encountered a marker, Do nothing. Just Skip ahead.
//        (*ipPtr) += (256 * (**ipPtr) + *(*ipPtr + 1));
//        //markerSKIP(ipPtr);
//    }
//    else if (**ipPtr == 0xDB) {		/*Quant Tables*/
//        // assert(input->soi == 1);
//        (*ipPtr)++;
//        DQT(ipPtr, input);
//    }
//    else if (**ipPtr == 0xC4) {		/*Huffman Tables*/
//        // assert(input->soi == 1);
//        (*ipPtr)++;
//        DHT(ipPtr, input);
//    }
//    else if (**ipPtr == 0xDD) {		/*DRI*/
//        // assert(input->soi == 1);
//        (*ipPtr)++;
//        DRI(ipPtr, input);
//    }
//    else if (**ipPtr == 0xDA) {		/*SOS*/
//        // assert(input->soi == 1);
//        (*ipPtr)++;
//        SOS(ipPtr, input);
//    }
//    else if (**ipPtr >= 0xD0 && **ipPtr <= 0xD7) {
//        // assert(input->soi == 1);	/*RST Marker*/
//        (*ipPtr)++;
//        status = BTA_StatusInvalidData;
//        //printf("stderr :: encountered RST marker, but it should not have been invoked from here. ");
//    }
//    else if (**ipPtr == 0x00) {
//        // assert(input->soi == 1);	/*Stuff Byte*/
//        (*ipPtr)++;
//        status = BTA_StatusInvalidData;
//        //printf("stderr :: encountered Stuff byte 0x00, but it should not have been here. ");
//    }
//    else if (**ipPtr >= 0xC1 && **ipPtr <= 0xCF) {
//        // assert(input->soi == 1);	/*UnSupported Markers*/
//        status = BTA_StatusNotSupported;
//        //printf("stderr :: %x,", **ipPtr);
//    }
//    else if (**ipPtr >= 0x01 && **ipPtr <= 0xBF) {
//        // assert(input->soi == 1);	/*Reserved/UnSupported Markers*/
//        status = BTA_StatusNotSupported;
//        //printf("stderr :: %x,", **ipPtr);
//    }
//    else if (**ipPtr >= 0xF0 && **ipPtr <= 0xFD) {
//        // assert(input->soi == 1);	/*UnSupported Markers*/
//        status = BTA_StatusNotSupported;
//        //printf("stderr :: %x,", **ipPtr);
//    }
//    else if (**ipPtr == 0xDE || **ipPtr == 0xDF) {
//        // assert(input->soi == 1);	/*UnSupported Markers*/
//        status = BTA_StatusNotSupported;
//        //printf("stderr :: %x,", **ipPtr);
//    }
//
//    return status;
//}
//
//
//static void DNL(uint8_t **ipPtr, InputStructure *input) {
//    // DNL Marker constitutes the height of the encoded image.
//    // assert((**ipPtr * 256) + (*((*ipPtr) + 1)) == 4);
//    *ipPtr += 2;
//    input->yRes = (**ipPtr * 256) + (*((*ipPtr) + 1));	// Height
//    *ipPtr += 2;
//}
//
//
//static void InitInput(InputStructure *input) {
//    // Reset the input structure for smooth execution
//    uint8_t *ipPtr = input->rawData;
//    int count = 0, i = 0, noofScans = 0;
//
//    input->eoi = 0;
//    input->soi = 0;
//    input->componentAttributes = 0;
//    input->multiScan = 0;
//    input->DNL = 0;
//    input->scalefactor = 1;
//
//    /* Huffman DC, AC BITS and VAL Reset */
//    for (count = 0; count < 2; count++) {
//        for (i = 0; i < 16; i++) {
//            input->dcCode.BITS[count][i] = 0;
//            input->dcCode.VALUES[count][i] = 0;
//            input->acCode.BITS[count][i] = 0;
//        }
//    }
//    for (count = 0; count < 2; count++) {
//        for (i = 0; i < MAXHUFFSIZE; i++) {
//            input->acCode.VALUES[count][i] = 0;
//        }
//    }
//
//    /*Output Stream Pointer Initialization*/
//    input->out.indexY[0] = input->out.indexY[1] = 0;
//    input->out.indexU[0] = input->out.indexU[1] = 0;
//    input->out.indexV[0] = input->out.indexV[1] = 0;
//    input->out.streamY = input->out.streamU = input->out.streamV = 0;
//
//    /*DNL Marker Support*/
//    /*The height of an image is declared in the marker SOI. But it so happens sometimes in multi-scan images,
//    the height is declared after the occurence of first scan under the marker DNL. Hence the correct height information is
//    not known until the first scan is completely decoded. To avoid this wait we look in to the entire image ahead for DNL and extract the actual height of the image.*/
//    while (noofScans < 2) {
//        if (*ipPtr == 0xff) {
//            // Stand Alone Marker(s)
//            if ((*(ipPtr + 1) == 0x00) || ((*(ipPtr + 1) >= 0xd0) && (*(ipPtr + 1) <= 0xd8)))
//                ipPtr += 2;
//            // Stuff Byte
//            else if (*(ipPtr + 1) == 0xff)
//                ipPtr += 1;
//            // EOI Marker
//            else if (*(ipPtr + 1) == 0xd9)
//                noofScans = 2;
//            else {
//                // DNL Marker
//                if (*(ipPtr + 1) == 0xdc) {
//                    ipPtr += 2;
//                    DNL(&ipPtr, input);
//                    input->DNL = 1;
//                    noofScans = 2;
//                }
//                else {
//                    // Start of Scan Marker
//                    if (*(ipPtr + 1) == 0xda)
//                        noofScans++;
//                    ipPtr += 2;
//                    // Encountered a marker, Do nothing. Just Skip ahead.
//                    ipPtr += (256 * (*ipPtr) + *(ipPtr + 1));
//                    //markerSKIP(&ipPtr);
//                }
//            }
//        }
//        else {
//            ipPtr++;
//        }
//    }
//}
//
//
//
//BTA_Status BTAdecodeJpgToRgb24(BTA_WrapperInst *winst, uint8_t *dataIn, uint32_t dataInLen, uint8_t *dataOut, uint32_t dataOutLen) {
//    if (!winst || !dataIn || !dataOut) {
//        return BTA_StatusInvalidParameter;
//    }
//    BTA_JpgInst *inst = (BTA_JpgInst *)winst->jpgInst;
//    if (!winst->jpgInst) {
//        return BTA_StatusInvalidParameter;
//    }
//
//    if (!inst->enabled) {
//        return BTA_StatusRuntimeError;
//    }
//
//    inst->input->rawData = dataIn;
//    InitInput(inst->input);
//
//
//    // Every jpeg image shall begin with SOI marker. This is usually followed by several other markers which contain the information necessary for decoding (like Quantization tables, Huffman tables used by image components while encoding, Restart interval, Internal color format, Width, Height and so on). After these markers, the image contains the actual huffman stream and finally the EOI marker.
//    // The current module first parses all the markers present in the image and then invokes the function "DecodeScan" which decodes the huffman stream and stores the result in the output buffer. This process is repeated until the EOI marker is reached.
//    BTA_Status status;
//    uint8_t *ipPtr = inst->input->rawData;
//
//    while (!inst->input->eoi) {
//        // Analyze the input image bitstream until end of image.
//        if (*ipPtr == 0xFF) {
//            ipPtr++;
//            status = AnalyzeMarker(&ipPtr, inst->input);
//            if (status != BTA_StatusOk) {
//                return status;
//            }
//        }
//        else {
//            ipPtr++;
//            //return BTA_StatusInvalidData;
//            //printf("stderr :: expected some marker. Every marker has to begin with 0xff but received byte is %x", *ipPtr);
//            //CleanUp(input);
//            //exit(1);
//        }
//    }
//    // Perform Color Space Conversion [if necessary]
//    if (inst->input->nf == 3) {
//        status = YUVtoRGB(inst, dataOut, dataOutLen);
//        free(inst->input->componentAttributes);
//        free(inst->input->out.streamY);
//        free(inst->input->out.streamU);
//        free(inst->input->out.streamV);
//        return status;
//    }
//    else if (inst->input->nf == 1) {
//        status = YUVtoMono(inst->input, dataOut, dataOutLen);
//        free(inst->input->componentAttributes);
//        free(inst->input->out.streamY);
//        return status;
//    }
//    else {
//        return BTA_StatusNotSupported;
//    }
//}