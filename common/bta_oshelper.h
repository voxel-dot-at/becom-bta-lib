/**  @file bta_oshelper.h
*
*    @brief This header file contains wrapper functions which have to access os functions
*
*    BLT_DISCLAIMER
*
*    @author Alex Falkensteiner
*
*    @cond svn
*
*    Information of last commit
*    $Rev::               $:  Revision of last commit
*    $Author::            $:  Author of last commit
*    $Date::              $:  Date of last commit
*
*    @endcond
*/

#ifndef BTA_OSHELPER_H_INCLUDED
#define BTA_OSHELPER_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include <bta_status.h>

typedef enum BTA_SeekOrigin {
    BTA_SeekOriginBeginning = SEEK_SET,
    BTA_SeekOriginCurrent = SEEK_CUR,
    BTA_SeekOriginEnd = SEEK_END
} BTA_SeekOrigin;



BTA_Status BTAfLargeOpen(const char* filename, const char *mode, void **file);
BTA_Status BTAfLargeClose(void *file);
//int BTAfLargeFlush(void *file);
BTA_Status BTAfLargeTell(void *file, uint64_t *position);
BTA_Status BTAfLargeSeek(void *file, int64_t offset, BTA_SeekOrigin origin, uint64_t *position);
BTA_Status BTAfLargeRead(void *file, void *buffer, uint32_t bytesToReadCount, uint32_t *bytesReadCount);
BTA_Status BTAfLargeWrite(void *file, void *buffer, uint32_t bufferLen, uint32_t *bytesWrittenCount);
BTA_Status BTAfLargeReadLine(void *file, char **line);

BTA_Status BTAfopen(const char* filename, const char *mode, void **file);
BTA_Status BTAfclose(void *file);
BTA_Status BTAfflush(void *file);
BTA_Status BTAftell(void *file, uint32_t *position);
BTA_Status BTAfseek(void *file, int32_t offset, BTA_SeekOrigin origin, uint32_t *position);
BTA_Status BTAfread(void *file, void *buffer, uint32_t bytesToReadCount, uint32_t *bytesReadCount);
BTA_Status BTAfwrite(void *file, void *buffer, uint32_t bufferLen, uint32_t *bytesWrittenCount);
BTA_Status BTAfreadLine(void *file, char *line, uint32_t lineLen);

BTA_Status BTAgetCwd(uint8_t *cwd, int cwdLen);

BTA_Status BTAfwriteCsv(const char* filename, const char **headersX, const char **headersY, int *data, int xRes, int yRes);

#endif
