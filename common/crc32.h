/**
 *    @file         crc32.h
 *    @ingroup     CRC
 *    
 *    @brief         CRC(Cyclic Redundancy Check) Functions for CRC32 (CCITT )
 *        
 *    BLT_DISCLAIMER
 *    
 *    @author     Thomas Maier
 *    
 *    @cond svn
 *    
 *    Information of last commit
 *    $Rev::               $:  Revision of last commit
 *    $Author::            $:  Author of last commit
 *    $Date::              $:  Date of last commit
 *    
 *    @endcond
 **/

#ifndef CRC32_H_INCLUDED
#define CRC32_H_INCLUDED

unsigned long CRC32ccitt(const void *pa_pBuffer, int pa_nLength);
unsigned long crc32_block(unsigned long pa_ulCrc, char *pa_pcData, unsigned long pa_ulLength);
unsigned long crc32_byte(unsigned long nCrc, unsigned char data);

#endif
