#ifndef BTA_SERIALIZATION
#define BTA_SERIALIZATION

#include <bta.h>

BTA_Status BTAdeserializeFrameV1(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV2(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV3(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV4(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV5(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen);

#endif