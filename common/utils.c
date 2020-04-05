#include "utils.h"




uint8_t BTAstartsWith(const char *phrase, const char *str) {
    if (!phrase || !str) return 0;
    size_t lenpre = strlen(phrase);
    size_t lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(phrase, str, lenpre) == 0;
}


uint8_t BTAendsWith(const char *phrase, const char *str) {
    if (!phrase || !str) return 0;
    size_t lenphr = strlen(phrase);
    size_t lenstr = strlen(str);
    return lenphr > lenstr ? 0 : strncmp(str + lenstr - lenphr, phrase, lenphr) == 0;
}


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




int readStringFromConsole(char *inputStr, int inputStrLen, char *defaultValue) {
    fflush(stdin);
    char *result = fgets(inputStr, inputStrLen, stdin);
    if (!result || (strlen(inputStr) > 0 && (inputStr[0] == '\n' || inputStr[0] == '\r'))) {
        strcpy(inputStr, defaultValue);
        return 0;
    }
    while (strlen(inputStr) > 0 && (inputStr[strlen(inputStr) - 1] == '\n' || inputStr[strlen(inputStr) - 1] == '\r' || inputStr[strlen(inputStr) - 1] == ' ')) {
        inputStr[strlen(inputStr) - 1] = 0;
    }
    while (strlen(inputStr) > 0 && inputStr[0] == ' ') {
        int l = (int)strlen(inputStr);
        for (int i = 1; i < l - 1; i++) {
            inputStr[i - 1] = inputStr[i];
        }
    }
    return 1;
}


int readIntFromConsole() {
    char inputStr[50];
    readStringFromConsole(inputStr, 50, (char *)"0");
    return atoi(inputStr);
}


char *createStrReplace(const char *str, const char *orig, const char *rep) {
    char *strNew;
    char *p = strstr((char *)str, orig);
    if (!p) {
        // 'orig' is not even in 'str'
        int len = (int)strlen(str) + 1;
        strNew = (char *)malloc(len);
        strncpy(strNew, str, len);
        return strNew;
    }
    int len1 = (int)(p - str);
    int len2 = (int)strlen(rep);
    int len3 = (int)strlen(str) - len1 - (int)strlen(orig);
    strNew = (char *)malloc(len1 + len2 + len3 + 1);
    if (!strNew) {
        return 0;
    }
    // use  snprintf(..)
    strncpy(strNew, str, len1);
    strncpy(strNew + len1, rep, len2);
    strncpy(strNew + len1 + len2, str + len1 + strlen(orig), len3);
    strNew[len1 + len2 + len3] = '\0';
    return strNew;
}