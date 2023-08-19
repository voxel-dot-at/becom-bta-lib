#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif

void println(const char* msg, ...);
int UTILfindElementInArray(int *array, int len, int element);
uint8_t UTILstartsWith(const char *pre, const char *str);
uint8_t UTILendsWith(const char *phrase, const char *str);
uint8_t UTILisNumber(char *text);
uint8_t UTILcontains(const char *phrase, const char *substring);
void UTILreplaceChar(char *str, char target, char replacement);
void UTILremoveChar(char *str, char c);
int UTILreadStringFromConsole(char* inputStr, int inputStrLen, const char* defaultValue);
int UTILreadIntFromConsole(void);
//char *UTILcreateStrReplace(const char *str, const char *orig, const char *rep);


#ifdef PLAT_WINDOWS
#   include <conio.h>
#elif defined PLAT_LINUX
extern int _kbhit();
#endif


#endif