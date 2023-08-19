#include "bitconverter.h"




void BTAbitConverterFromUInt08(uint8_t v, uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    stream[off] = v & 0xff;
    if (offset) {
        *offset = *offset + 1;
    }
}


void BTAbitConverterFromUInt16(uint16_t v, uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    memcpy(&stream[off], &v, 2);
    if (offset) {
        *offset = *offset + 2;
    }
}


void BTAbitConverterFromUInt32(uint32_t v, uint8_t *stream, uint32_t * offset) {
    uint32_t off = offset ? *offset : 0;
    memcpy(&stream[off], &v, 4);
    if (offset) {
        *offset = *offset + 4;
    }
}


void BTAbitConverterFromFloat4(float v, uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    memcpy(&stream[off], &v, 4);
    if (offset) {
        *offset = *offset + 4;
    }
}


void BTAbitConverterFromStream(uint8_t *v, uint32_t vLen, uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    memcpy(&stream[off], v, vLen);
    if (offset) {
        *offset = *offset + vLen;
    }
}


uint8_t BTAbitConverterToUInt08(uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    uint8_t v = stream[off];
    if (offset) {
        *offset = *offset + 1;
    }
    return v;
}


uint16_t BTAbitConverterToUInt16(uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    uint16_t v;
    memcpy(&v, &stream[off], 2);
    if (offset) {
        *offset = *offset + 2;
    }
    return v;
}


uint32_t BTAbitConverterToUInt32(uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    uint32_t v;
    memcpy(&v, &stream[off], 4);
    if (offset) {
        *offset = *offset + 4;
    }
    return v;
}


float BTAbitConverterToFloat4(uint8_t *stream, uint32_t *offset) {
    uint32_t off = offset ? *offset : 0;
    float v;
    memcpy(&v, &stream[off], 4);
    if (offset) {
        *offset = *offset + 4;
    }
    return v;
}


void BTAbitConverterToStream(uint8_t *stream, uint32_t *offset, uint8_t *v, uint32_t vLen) {
    uint32_t off = offset ? *offset : 0;
    memcpy(v, &stream[off], vLen);
    if (offset) {
        *offset = *offset + vLen;
    }
}
