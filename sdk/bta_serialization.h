#ifndef BTA_SERIALIZATION
#define BTA_SERIALIZATION

#include <bta.h>

#define BTA_FRAME_SERIALIZED_PREAMBLE           0xb105
#define BTA_FRAME_SERIALIZED_VERSION            5
#define BTA_FRAME_SERIALIZED_PREAMBLE_LZMAV22   0x5cbc

BTA_Status BTAcompressSerializedFrameLzmaV22(uint8_t *frameSerialized, uint32_t frameSerializedLen, uint8_t *frameSerializedCompressed, uint32_t *frameSerializedCompressedLen);
BTA_Status BTAdeserializeFrameV1(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV2(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV3(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV4(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV5(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);

#endif