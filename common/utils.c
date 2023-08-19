#include "utils.h"

#ifdef PLAT_WINDOWS
#include <Windows.h>
#endif



void println(const char* msg, ...) {
    va_list arg;
    va_start(arg, msg);
    vprintf(msg, arg);
    va_end(arg);
    printf("\n");
}


int UTILfindElementInArray(int *array, int len, int element) {
    for (int i = 0; i < len; i++) {
        if (array[i] == element) {
            return i;
        }
    }
    return -1;
}


uint8_t UTILstartsWith(const char *phrase, const char *str) {
    if (!phrase || !str) return 0;
    size_t lenpre = strlen(phrase);
    size_t lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(phrase, str, lenpre) == 0;
}


uint8_t UTILendsWith(const char *phrase, const char *str) {
    if (!phrase || !str) return 0;
    size_t lenphr = strlen(phrase);
    size_t lenstr = strlen(str);
    return lenphr > lenstr ? 0 : strncmp(str + lenstr - lenphr, phrase, lenphr) == 0;
}

uint8_t UTILisNumber(char *text) {
    int j = (int)strlen(text);
    while (j--) {
        if (text[j] < '0' || text[j] > '9') {
            return 0;
        }
    }
    return 1;
}


uint8_t UTILcontains(const char *phrase, const char *substring) {
    if (!phrase || !substring) {
        return 0;
    }
    // Get the lengths of the strings
    size_t strLen = strlen(phrase);
    size_t subLen = strlen(substring);
    // If the substring is longer than the original string, it can't be contained
    if (subLen > strLen) {
        return 0;
    }
    // Loop through the original string to check for a match
    for (size_t i = 0; i <= strLen - subLen; i++) {
        size_t j;
        // Check if the substring matches starting from the current position
        for (j = 0; j < subLen; j++) {
            if (phrase[i + j] != substring[j]) {
                break; // Substring does not match at this position
            }
        }
        // If the inner loop ran through completely, we found a match
        if (j == subLen) {
            return 1;
        }
    }
    return 0;
}


//int splitString(const char *str, char delimiter, char **tokens, int *tokenLens) {
//    if (!str || !tokens || !tokenLens) {
//        return -1;
//    }
//    int count = 0;
//    size_t strLen = strlen(str);
//    // Initialize pointers to start and end of the current substring
//    const char *start = str;
//    const char *end = str;
//
//    // Loop through the string to find substrings
//    for (size_t i = 0; i <= strLen; i++) {
//        if (str[i] == delimiter || str[i] == '\0') {
//            // Allocate memory for the current substring and copy it
//            size_t substringLen = end - start;
//            result[count] = (char *)malloc(substringLen + 1); // +1 for the null terminator
//            strncpy(result[count], start, substringLen);
//            result[count][substringLen] = '\0';
//
//            count++; // Increment the substring count
//
//            // Update the start and end pointers for the next substring
//            start = end = str + i + 1;
//        }
//        else {
//            end++;
//        }
//    }
//    return count;
//}


void UTILreplaceChar(char *str, char target, char replacement) {
    int length = (int)strlen(str);
    for (int i = 0; i < length; i++) {
        if (str[i] == target) {
            str[i] = replacement;
        }
    }
}

void UTILremoveChar(char *str, char c) {
    int i, j = 0;
    int len = (int)strlen(str);

    for (i = 0; i < len; i++) {
        if (str[i] != c) {
            str[j++] = str[i];
        }
    }

    str[j] = '\0';  // Add null terminator at the end
}


int UTILreadStringFromConsole(char* inputStr, int inputStrLen, const char* defaultValue) {
    fflush(stdin);
    char* result = fgets(inputStr, inputStrLen, stdin);
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


int UTILreadIntFromConsole() {
    char inputStr[50];
    UTILreadStringFromConsole(inputStr, 50, "0");
    return atoi(inputStr);
}


//char *UTILcreateStrReplace(const char *str, const char *orig, const char *rep) {
//    char *strNew;
//    char *p = strstr((char *)str, orig);
//    if (!p) {
//        // 'orig' is not even in 'str'
//        int len = (int)strlen(str) + 1;
//        strNew = (char *)malloc(len);
//        strncpy(strNew, str, len);
//        return strNew;
//    }
//    int len1 = (int)(p - str);
//    int len2 = (int)strlen(rep);
//    int len3 = (int)strlen(str) - len1 - (int)strlen(orig);
//    strNew = (char *)malloc(len1 + len2 + len3 + 1);
//    if (!strNew) {
//        return 0;
//    }
//    // use  snprintf(..)
//    strncpy(strNew, str, len1);
//    strncpy(strNew + len1, rep, len2);
//    strncpy(strNew + len1 + len2, str + len1 + strlen(orig), len3);
//    strNew[len1 + len2 + len3] = '\0';
//    return strNew;
//}