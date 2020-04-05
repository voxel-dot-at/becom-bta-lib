#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


#define BTAmax(a,b)  (((a) > (b)) ? (a) : (b))
#define BTAmin(a,b)  (((a) < (b)) ? (a) : (b))
#define BTAround(a)  ((int)(a + 0.5))
#define BTAabs(a)  (((a) > 0) ? (a) : (-(a)))

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

uint8_t BTAstartsWith(const char *pre, const char *str);
uint8_t BTAendsWith(const char *phrase, const char *str);



int readStringFromConsole(char *inputStr, int inputStrLen, char *defaultValue);
int readIntFromConsole(void);
char *createStrReplace(const char *str, const char *orig, const char *rep);

#endif