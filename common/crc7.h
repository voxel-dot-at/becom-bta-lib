/**
 *	@file 		CRC7.h
 *	@ingroup 	CRC
 *
 *	@brief 		7-bit Cyclic Redundancy Check
 *
 *
 *
 *	BLT_DISCLAIMER
 *
 *	@author 	James Walmsley
 *
 *	@cond svn
 *
 *	Information of last commit
 *	$Rev::               $:  Revision of last commit
 *	$Author::            $:  Author of last commit
 *	$Date::              $:  Date of last commit
 *
 *	@endcond
 **/

/** @defgroup CRC
 *  @ingroup devapi
 *  Cyclic Redundancy Checks
 */
 
#ifndef _CRC7_H_
#define _CRC7_H_

unsigned char crc7		(const void *buf, int len);
unsigned char crc7_byte	(unsigned char crc, unsigned char data);
 
#endif
