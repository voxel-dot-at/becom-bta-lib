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
