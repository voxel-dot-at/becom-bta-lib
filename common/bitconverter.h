#ifndef BITCONVERTER_H_INCLUDED
#define BITCONVERTER_H_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

void BTAbitConverterFromUInt08(uint8_t *stream, uint32_t *offset, uint8_t v);
void BTAbitConverterFromUInt16(uint8_t *stream, uint32_t *offset, uint16_t v);
void BTAbitConverterFromUInt32(uint8_t *stream, uint32_t *offset, uint32_t v);
void BTAbitConverterFromFloat4(uint8_t *stream, uint32_t *offset, float v);
void BTAbitConverterFromStream(uint8_t *stream, uint32_t *offset, uint8_t *v, uint32_t vLen);
void BTAbitConverterToUInt08(uint8_t *stream, uint32_t *offset, uint8_t *v);
void BTAbitConverterToUInt16(uint8_t *stream, uint32_t *offset, uint16_t *v);
void BTAbitConverterToUInt32(uint8_t *stream, uint32_t *offset, uint32_t *v);
void BTAbitConverterToFloat4(uint8_t *stream, uint32_t *offset, float *v);
void BTAbitConverterToStream(uint8_t *stream, uint32_t *offset, uint8_t *v, uint32_t vLen);

#endif