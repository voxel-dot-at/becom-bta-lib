
/**  @file bta_jpg.h
*
*    @brief Header file for bta_jpg.c
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



#ifndef BTA_JPG_H_INCLUDED
#define BTA_JPG_H_INCLUDED

#include <stdint.h>
// TODO: remove dependency!!!
#include <bta_status.h>
#include <bta_helper.h>


//#define MAXHUFFSIZE 512
//
//
//
//typedef struct tagAppArgs {					/*Input arguments*/
//    char* szInputFile;
//    char* szOutputFile;
//    int scalefactor;
//} AppArgs;
//
//
//typedef struct tagDCHuffman {
//    uint8_t BITS[2][16];					/*Number of Codes of Length I[1-16]*/
//    uint8_t VALUES[2][16];					/*Code Values*/
//    int CODE[2][16];						/*Huffman Code of the Values*/
//    uint8_t SIZE[2][16];					/*Size Table*/
//    int MAXCODE[2][16];						/*Maximum code of Length I[1-16]*/
//    int MINCODE[2][16];						/*Minimum code of Length I[1-16]*/
//    uint32_t VALPTR[2][16];						/*Index of Maximum code of Length I[1-16] */
//} DCHuffman;
//
//
//typedef struct tagACHuffman {
//    uint8_t BITS[2][16];					/*Number of Codes of Length I[1-16]*/
//    uint8_t VALUES[2][MAXHUFFSIZE];			/*Code Values*/
//    int CODE[2][MAXHUFFSIZE];				/*Huffman Code of the Values*/
//    uint8_t SIZE[2][MAXHUFFSIZE];			/*Size Table*/
//    int MAXCODE[2][16];						/*Maximum code of Length I[1-16]*/
//    int MINCODE[2][16];						/*Minimum code of Length I[1-16]*/
//    uint32_t VALPTR[2][16];						/*Index of Maximum code of Length I[1-16] */
//} ACHuffman;
//
//
//typedef enum tagYUVFormat {					/*Internal Color Format*/
//    YUV400 = 0,
//    YUV420 = 1,
//    YUV422 = 2,
//    YUV444 = 3
//} YUVFormat;
//
//
//typedef struct tagOutputStream {				/*Output Buffer*/
//    uint8_t *streamY, *streamU, *streamV;
//    uint32_t indexY[2], indexU[2], indexV[2];
//} OutputStream;
//
//
//typedef struct tagInputStructure {
//    uint8_t* rawData;						/*Input Buffer*/
//    uint8_t soi, eoi;						/* Start of Image, End of Image */
//    uint32_t xRes;
//    uint32_t yRes;
//    uint32_t bitDepth;
//    uint8_t nf, ns;							/*Number of Frame Components & Scan Components*/
//    uint8_t* componentAttributes;			/*Component Attributes*/
//    uint8_t quantTables[4][64];				/*Quant Tables*/
//    uint32_t restartInterval;               /*Restart Interval*/
//    DCHuffman dcCode;						/*Huffman Codes*/
//    ACHuffman acCode;
//    YUVFormat inputFormat;					/*Encoded Format*/
//    uint32_t expectedMCUCount;				/*Expected MCU Count*/
//    uint8_t multiScan;						/*Scan Component Index[7bits]|multiScan[1bit]*/
//    OutputStream out;						/*Output Buffer*/
//    uint32_t extwidth, extheight;			/*extended width, height*/
//    uint8_t DNL;							/*This register is set if DNL marker is present*/
//    int scalefactor;						/*down scale the image by 'K' along width & height [K = 1|2|4|8]*/
//} InputStructure;
//
//
//
typedef struct BTA_JpgInst {
    uint8_t enabled;

//    InputStructure *input;
//    int buffer_1_402[256];
//    int buffer_0_34414[256];
//    int buffer_0_71414[256];
//    int buffer_1_772[256];
} BTA_JpgInst;


BTA_Status BTAjpgInit(BTA_WrapperInst *winst);
BTA_Status BTAjpgClose(BTA_WrapperInst *winst);
BTA_Status BTAjpgEnable(BTA_WrapperInst *winst, uint8_t enable);
BTA_Status BTAjpgIsEnabled(BTA_WrapperInst *winst, uint8_t *enabled);
BTA_Status BTAdecodeJpgToRgb24(BTA_WrapperInst *winst, uint8_t *dataIn, uint32_t dataInLen, uint8_t *dataOut, uint32_t dataOutLen);

#endif

