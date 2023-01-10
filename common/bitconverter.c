#include "bitconverter.h"




void BTAbitConverterFromUInt08(uint8_t *stream, uint32_t *offset, uint8_t v) {
    stream[*offset] = v & 0xff;
    *offset = *offset + 1;
}


void BTAbitConverterFromUInt16(uint8_t *stream, uint32_t *offset, uint16_t v) {
    memcpy(&stream[*offset], &v, 2);
    *offset = *offset + 2;
}


void BTAbitConverterFromUInt32(uint8_t *stream, uint32_t *offset, uint32_t v) {
    memcpy(&stream[*offset], &v, 4);
    *offset = *offset + 4;
}


void BTAbitConverterFromFloat4(uint8_t *stream, uint32_t *offset, float v) {
    memcpy(&stream[*offset], &v, 4);
    *offset = *offset + 4;
}


void BTAbitConverterFromStream(uint8_t *stream, uint32_t *offset, uint8_t *v, uint32_t vLen) {
    memcpy(&stream[*offset], v, vLen);
    *offset = *offset + vLen;
}


void BTAbitConverterToUInt08(uint8_t *stream, uint32_t *offset, uint8_t *v) {
    *v = stream[*offset];
    *offset = *offset + 1;
}


void BTAbitConverterToUInt16(uint8_t *stream, uint32_t *offset, uint16_t *v) {
    memcpy(v, &stream[*offset], 2);
    *offset = *offset + 2;
}


void BTAbitConverterToUInt32(uint8_t *stream, uint32_t *offset, uint32_t *v) {
    memcpy(v, &stream[*offset], 4);
    *offset = *offset + 4;
}


void BTAbitConverterToFloat4(uint8_t *stream, uint32_t *offset, float *v) {
    memcpy(v, &stream[*offset], 4);
    *offset = *offset + 4;
}


void BTAbitConverterToStream(uint8_t *stream, uint32_t *offset, uint8_t *v, uint32_t vLen) {
    memcpy(v, &stream[*offset], vLen);
    *offset = *offset + vLen;
}
