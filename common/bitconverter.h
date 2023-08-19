#ifndef BITCONVERTER_H_INCLUDED
#define BITCONVERTER_H_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

void BTAbitConverterFromUInt08(uint8_t v, uint8_t *stream, uint32_t *offset);
void BTAbitConverterFromUInt16(uint16_t v, uint8_t *stream, uint32_t *offset);
void BTAbitConverterFromUInt32(uint32_t v, uint8_t *stream, uint32_t *offset);
void BTAbitConverterFromFloat4(float v, uint8_t *stream, uint32_t *offset);
void BTAbitConverterFromStream(uint8_t *v, uint32_t vLen, uint8_t *stream, uint32_t *offset);
uint8_t BTAbitConverterToUInt08(uint8_t *stream, uint32_t *offset);
uint16_t BTAbitConverterToUInt16(uint8_t *stream, uint32_t *offset);
uint32_t BTAbitConverterToUInt32(uint8_t *stream, uint32_t *offset);
float BTAbitConverterToFloat4(uint8_t *stream, uint32_t *offset);
void BTAbitConverterToStream(uint8_t *stream, uint32_t *offset, uint8_t *v, uint32_t vLen);

#endif