#ifndef CRC16_H_INCLUDED
#define CRC16_H_INCLUDED

unsigned short crc16_ccitt(const void *buf, int len);

// initial: sets the initial value
unsigned short crc16_ccitt_ext(const void *buf, int len, unsigned short initial);

//calculates the crc16 of the give buffer by interpreting it as a 16bit buffer
unsigned short crc16_ccitt_in_16bit(const unsigned short *buf, int len);

#endif
