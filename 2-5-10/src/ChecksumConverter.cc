/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2006 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the checksum pseudo-converter of StreamDevice.       *
* Please refer to the HTML files in ../doc/ for a detailed     *
* documentation.                                               *
*                                                              *
* If you do any changes in this file, you are not allowed to   *
* redistribute it any more. If there is a bug or a missing     *
* feature, send me an email and/or your patch. If I accept     *
* your changes, they will go to the next release.              *
*                                                              *
* DISCLAIMER: If this software breaks something or harms       *
* someone, it's your problem.                                  *
*                                                              *
***************************************************************/

#include "StreamFormatConverter.h"
#include "StreamError.h"
#if defined(__vxworks) || defined(vxWorks) || defined(_WIN32) || defined(__rtems__)
// These systems have no strncasecmp
#include <epicsVersion.h>
#ifdef BASE_VERSION
// 3.13
#include <ctype.h>
static int strncasecmp(const char *s1, const char *s2, size_t n)
{
    int r=0;
    while (n && (r = toupper(*s1)-toupper(*s2)) == 0) { n--; s1++; s2++; };
    return r;
}
#else
#include <epicsString.h>
#define strncasecmp epicsStrnCaseCmp
#endif
#endif
#include <ctype.h>
#include <stdint.h>

typedef unsigned int (*checksumFunc)(const unsigned char* data, unsigned int len,  unsigned int init);

static unsigned int sum(const unsigned char* data, unsigned int len, unsigned int sum)
{
    while (len--)
    {
        sum += *data++;
    }
    return sum;
}

static unsigned int julich(const unsigned char* data, unsigned int len, unsigned int sum)
{
	
	int original_len = len;
	int zeroes_or_hashes = 0;
	char res[3]; /* two bytes of hex = 2 characters, plus NULL terminator */
	
    while (len--)
    {
		if(*data == '#') { zeroes_or_hashes++;  sum += 0x23;}
		if(*data == '0') { zeroes_or_hashes++;  sum += 0x30;}
		if(*data == '1') sum += 0x31;
		if(*data == '2') sum += 0x32;
		if(*data == '3') sum += 0x33;
		if(*data == '4') sum += 0x34;
		if(*data == '5') sum += 0x35;
		if(*data == '6') sum += 0x36;
		if(*data == '7') sum += 0x37;
		if(*data == '8') sum += 0x38;
		if(*data == '9') sum += 0x39;
		if(*data == 'A') sum += 0x41;
		if(*data == 'B') sum += 0x42;
		if(*data == 'C') sum += 0x43;
		if(*data == 'D') sum += 0x44;
		if(*data == 'E') sum += 0x45;
		if(*data == 'F') sum += 0x46;
		// Annoyingly the MERLIN fermi also has 'G' and 'H' as "hex" characters...
		if(*data == 'G') sum += 0x47;
		if(*data == 'H') sum += 0x48;
		
		data++;
    }
	
	if(original_len == zeroes_or_hashes)
	{
		sum = 0;
	}
	
	// Only care about least significant 2 nibbles (1 nibble = 4 bits = 1 hex character)
	int sum_modulo_256 = sum % 256;
	
	sprintf(&res[0], "%02X", sum_modulo_256);
	return res[1] + res[0]*256;
}

/**
 *  Checksum function for the SKF MB350PC chopper.
 *
 *  Code is taken directly from appendix A of the manual, modified very slightly to make it work in stream device.
 */
static unsigned int skf_modbus(const unsigned char* data, unsigned int len, unsigned int sum)
{
    int i; /* Count through bitshifts */
    uint16_t CRC_16 = 0xa001;
    uint16_t crc = 0xffff; /* Initalize crc to 0xffff for modbus */
    while (len--) {
        crc ^= *data++; 
        i = 8;
        do {
            crc = (uint16_t)((crc & 1) ? crc >> 1 ^ CRC_16 : crc >> 1);
        } while (--i);
    }
    return crc; 
}

static unsigned int xor8(const unsigned char* data, unsigned int len, unsigned int sum)
{
    while (len--)
    {
        sum ^= *data++;
    }
    return sum;
}

static unsigned int xor7(const unsigned char* data, unsigned int len, unsigned int sum)
{
    return xor8(data, len, sum) & 0x7F;
}

static unsigned int crc_0x07(const unsigned char* data, unsigned int len, unsigned int crc)
{
    // x^8 + x^2 + x^1 + x^0 (0x07)
    const static unsigned char table[256] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
        0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
        0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
        0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
        0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
        0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
        0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
        0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
        0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
        0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
        0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
        0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
        0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
        0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
        0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
        0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
        0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
        0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
        0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
        0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
        0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
        0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
        0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
        0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
        0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
        0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
        0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
        0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
        0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
        0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3 };

    while (len--) crc = table[(crc ^ *data++) & 0xFF];
    return crc;
}

static unsigned int crc_0x31(const unsigned char* data, unsigned int len, unsigned int crc)
{
    // x^8 + x^5 + x^4 + x^0 (0x31)
    const static unsigned char table[256] = {
        0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83,
        0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
        0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e,
        0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
        0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0,
        0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
        0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d,
        0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
        0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5,
        0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
        0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58,
        0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
        0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6,
        0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
        0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b,
        0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
        0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f,
        0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
        0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92,
        0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
        0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c,
        0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
        0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1,
        0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
        0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49,
        0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
        0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4,
        0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
        0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a,
        0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
        0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7,
        0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35 };

    while (len--) crc = table[(crc ^ *data++) & 0xFF];
    return crc;
}

static unsigned int crc_0x8005(const unsigned char* data, unsigned int len, unsigned int crc)
{
    // x^16 + x^15 + x^2 + x^0  (0x8005)
    const static unsigned short table[256] = {
        0x0000, 0x8005, 0x800f, 0x000a, 0x801b, 0x001e, 0x0014, 0x8011,
        0x8033, 0x0036, 0x003c, 0x8039, 0x0028, 0x802d, 0x8027, 0x0022,
        0x8063, 0x0066, 0x006c, 0x8069, 0x0078, 0x807d, 0x8077, 0x0072,
        0x0050, 0x8055, 0x805f, 0x005a, 0x804b, 0x004e, 0x0044, 0x8041,
        0x80c3, 0x00c6, 0x00cc, 0x80c9, 0x00d8, 0x80dd, 0x80d7, 0x00d2,
        0x00f0, 0x80f5, 0x80ff, 0x00fa, 0x80eb, 0x00ee, 0x00e4, 0x80e1,
        0x00a0, 0x80a5, 0x80af, 0x00aa, 0x80bb, 0x00be, 0x00b4, 0x80b1,
        0x8093, 0x0096, 0x009c, 0x8099, 0x0088, 0x808d, 0x8087, 0x0082,
        0x8183, 0x0186, 0x018c, 0x8189, 0x0198, 0x819d, 0x8197, 0x0192,
        0x01b0, 0x81b5, 0x81bf, 0x01ba, 0x81ab, 0x01ae, 0x01a4, 0x81a1,
        0x01e0, 0x81e5, 0x81ef, 0x01ea, 0x81fb, 0x01fe, 0x01f4, 0x81f1,
        0x81d3, 0x01d6, 0x01dc, 0x81d9, 0x01c8, 0x81cd, 0x81c7, 0x01c2,
        0x0140, 0x8145, 0x814f, 0x014a, 0x815b, 0x015e, 0x0154, 0x8151,
        0x8173, 0x0176, 0x017c, 0x8179, 0x0168, 0x816d, 0x8167, 0x0162,
        0x8123, 0x0126, 0x012c, 0x8129, 0x0138, 0x813d, 0x8137, 0x0132,
        0x0110, 0x8115, 0x811f, 0x011a, 0x810b, 0x010e, 0x0104, 0x8101,
        0x8303, 0x0306, 0x030c, 0x8309, 0x0318, 0x831d, 0x8317, 0x0312,
        0x0330, 0x8335, 0x833f, 0x033a, 0x832b, 0x032e, 0x0324, 0x8321,
        0x0360, 0x8365, 0x836f, 0x036a, 0x837b, 0x037e, 0x0374, 0x8371,
        0x8353, 0x0356, 0x035c, 0x8359, 0x0348, 0x834d, 0x8347, 0x0342,
        0x03c0, 0x83c5, 0x83cf, 0x03ca, 0x83db, 0x03de, 0x03d4, 0x83d1,
        0x83f3, 0x03f6, 0x03fc, 0x83f9, 0x03e8, 0x83ed, 0x83e7, 0x03e2,
        0x83a3, 0x03a6, 0x03ac, 0x83a9, 0x03b8, 0x83bd, 0x83b7, 0x03b2,
        0x0390, 0x8395, 0x839f, 0x039a, 0x838b, 0x038e, 0x0384, 0x8381,
        0x0280, 0x8285, 0x828f, 0x028a, 0x829b, 0x029e, 0x0294, 0x8291,
        0x82b3, 0x02b6, 0x02bc, 0x82b9, 0x02a8, 0x82ad, 0x82a7, 0x02a2,
        0x82e3, 0x02e6, 0x02ec, 0x82e9, 0x02f8, 0x82fd, 0x82f7, 0x02f2,
        0x02d0, 0x82d5, 0x82df, 0x02da, 0x82cb, 0x02ce, 0x02c4, 0x82c1,
        0x8243, 0x0246, 0x024c, 0x8249, 0x0258, 0x825d, 0x8257, 0x0252,
        0x0270, 0x8275, 0x827f, 0x027a, 0x826b, 0x026e, 0x0264, 0x8261,
        0x0220, 0x8225, 0x822f, 0x022a, 0x823b, 0x023e, 0x0234, 0x8231,
        0x8213, 0x0216, 0x021c, 0x8219, 0x0208, 0x820d, 0x8207, 0x0202 };

    while (len--) crc = table[((crc>>8) ^ *data++) & 0xFF] ^ (crc << 8);
    return crc;
}

static unsigned int crc_0x8005_r(const unsigned char* data, unsigned int len, unsigned int crc)
{
    // x^16 + x^15 + x^2 + x^0  (0x8005)
    // reflected
    const static unsigned short table[256] = {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040 };

    while (len--) crc = table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
    return crc;
}

static unsigned int crc_0x1021(const unsigned char* data, unsigned int len, unsigned int crc)
{
    // x^16 + x^12 + x^5 + x^0 (0x1021)
    const static unsigned short table[256] = {
      0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
      0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
      0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
      0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
      0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
      0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
      0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
      0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
      0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
      0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
      0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
      0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
      0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
      0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
      0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
      0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
      0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
      0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
      0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
      0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
      0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
      0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
      0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
      0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
      0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
      0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
      0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
      0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
      0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
      0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
      0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
      0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0 };

    while (len--) crc = table[((crc>>8) ^ *data++) & 0xFF] ^ (crc << 8);
    return crc;
}

static unsigned int crc_0x04C11DB7(const unsigned char* data, unsigned int len, unsigned int crc)
{
    // x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 +
    //    x^8 + x^7 + x^5 + x^4 + x^2 + x^1 + x^0  (0x04C11DB7)
    const static unsigned int table[] = {
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
        0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
        0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
        0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
        0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
        0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
        0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
        0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
        0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
        0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
        0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
        0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
        0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
        0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
        0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
        0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
        0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
        0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
        0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
        0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
        0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
        0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
        0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
        0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
        0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
        0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
        0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
        0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
        0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
        0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
        0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
        0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
        0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
        0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
        0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
        0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
        0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
        0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
        0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
        0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
        0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
        0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
        0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
        0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
        0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
        0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
        0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
        0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
        0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
        0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
        0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
        0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
        0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
        0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
        0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
        0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
        0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
        0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
        0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4 };

    while (len--) crc = table[((crc>>24) ^ *data++) & 0xFF] ^ (crc << 8);
    return crc;
}

static unsigned int crc_0x04C11DB7_r(const unsigned char* data, unsigned int len, unsigned int crc)
{
    // x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 +
    //    x^8 + x^7 + x^5 + x^4 + x^2 + x^1 + x^0  (0x04C11DB7)
    // reflected
    const static unsigned int table[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
        0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
        0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
        0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
        0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
        0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
        0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
        0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
        0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };

    while (len--) crc = table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
    return crc;
}

static unsigned int adler32(const unsigned char* data, unsigned int len, unsigned int init)
{
    unsigned int a = init & 0xFFFF;
    unsigned int b = (init >> 16) & 0xFFFF;

    while (len) {
        unsigned int tlen = len > 5550 ? 5550 : len;
        len -= tlen;
        do {
            a += *data++;
            b += a;
        } while (--tlen);
        a = (a & 0xFFFF) + (a >> 16) * 15;
        b = (b & 0xFFFF) + (b >> 16) * 15;
   }
   if (a >= 65521) a -= 65521;
   b = (b & 0xFFFF) + (b >> 16) * 15;
   if (b >= 65521) b -= 65521;
   return b << 16 | a;
}

static unsigned int hexsum(const unsigned char* data, unsigned int len, unsigned int sum)
{
    // Add all hex digits, ignore all other bytes.
    unsigned int d; 
    while (len--)
    {
        d = toupper(*data++);
        if (isxdigit(d))
        {
            if (isdigit(d)) d -= '0';
            else d -= 'A' - 0x0A;
            sum += d;
        }
    }
    return sum;
}

struct checksum
{
    const char* name;
    checksumFunc func;
    unsigned int init;
    unsigned int xorout;
    signed char bytes;
};

static checksum checksumMap[] =
// You may add your own checksum functions to this map.
{
//    name      func              init        xorout  bytes  chk("123456789")
    {"sum",     sum,              0x00,       0x00,       1}, // 0xDD
    {"sum8",    sum,              0x00,       0x00,       1}, // 0xDD
    {"sum16",   sum,              0x0000,     0x0000,     2}, // 0x01DD
    {"sum32",   sum,              0x00000000, 0x00000000, 4}, // 0x000001DD
    {"xor",     xor8,             0x00,       0x00,       1}, // 0x31
    {"xor8",    xor8,             0x00,       0x00,       1}, // 0x31
    {"xor7",    xor7,             0x00,       0x00,       1}, // 0x31
    {"crc8",    crc_0x07,         0x00,       0x00,       1}, // 0xF4
    {"ccitt8",  crc_0x31,         0x00,       0x00,       1}, // 0xA1
    {"crc16",   crc_0x8005,       0x0000,     0x0000,     2}, // 0xFEE8
    {"crc16r",  crc_0x8005_r,     0x0000,     0x0000,     2}, // 0xBB3D
    {"modbus",  crc_0x8005_r,     0xFFFF,     0x0000,     2}, // 0x4B37
    {"ccitt16", crc_0x1021,       0xFFFF,     0x0000,     2}, // 0x29B1
    {"ccitt16a",crc_0x1021,       0x1D0F,     0x0000,     2}, // 0xE5CC
    {"ccitt16x",crc_0x1021,       0x0000,     0x0000,     2}, // 0x31C3
    {"crc16c",  crc_0x1021,       0x0000,     0x0000,     2}, // 0x31C3
    {"xmodem",  crc_0x1021,       0x0000,     0x0000,     2}, // 0x31C3
    {"crc32",   crc_0x04C11DB7,   0xFFFFFFFF, 0xFFFFFFFF, 4}, // 0xFC891918
    {"crc32r",  crc_0x04C11DB7_r, 0xFFFFFFFF, 0xFFFFFFFF, 4}, // 0xCBF43926
    {"jamcrc",  crc_0x04C11DB7_r, 0xFFFFFFFF, 0x00000000, 4}, // 0x340BC6D9
    {"adler32", adler32,          0x00000001, 0x00000000, 4}, // 0x091E01DE
    {"hexsum8", hexsum,           0x00,       0x00,       1}, // 0x2D
	{"julich",  julich,           0x00,       0x00,       2}, // 0x2D
	{"skf_modbus",  skf_modbus,   0x00,       0x00,       2}  // 0x374B
};

static unsigned int mask[5] = {0, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF};

class ChecksumConverter : public StreamFormatConverter
{
    int parse (const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printPseudo(const StreamFormat&, StreamBuffer&);
    int scanPseudo(const StreamFormat&, StreamBuffer&, long& cursor);
};

int ChecksumConverter::
parse(const StreamFormat&, StreamBuffer& info, const char*& source, bool)
{
    const char* p = strchr(source, '>');
    if (!p)
    {
        error ("Missing closing '>' in checksum format.\n");
        return false;
    }

    bool negflag=false;
    bool notflag=false;
    if (*source == '-')
    {
        source++;
        negflag = true;
    }
    if (strncasecmp(source, "neg", 3) == 0)
    {
        source+=3;
        negflag = true;
    }
    if (*source == '~')
    {
        source++;
        notflag = true;
    }
    if (strncasecmp(source, "not", 3) == 0)
    {
        source+=3;
        notflag = true;
    }
    unsigned  fnum;
    int len = p-source;
    unsigned int init, xorout;
    for (fnum = 0; fnum < sizeof(checksumMap)/sizeof(checksum); fnum++)
    {
        if ((strncasecmp(source, checksumMap[fnum].name, len) == 0) ||
            (*source == 'n' && len > 1 && strncasecmp(source+1, checksumMap[fnum].name, len-1) == 0 && (negflag = true)))
        {
            init = checksumMap[fnum].init;
            xorout = checksumMap[fnum].xorout;
            if (negflag)
            {
                init = ~init;
                xorout = ~xorout;
            }
            if (notflag)
            {
                xorout = ~xorout;
            }
            info.append(&init,  sizeof(init));
            info.append(&xorout, sizeof(xorout));
            info.append(fnum);
            source = p+1;
            return pseudo_format;
        }
    }

    error ("Unknown checksum algorithm \"%.*s\"\n", len, source);
    return false;
}

bool ChecksumConverter::
printPseudo(const StreamFormat& format, StreamBuffer& output)
{
    unsigned int sum;
    const char* info = format.info;
    unsigned int init = extract<unsigned int>(info);
    unsigned int xorout = extract<unsigned int>(info);
    int fnum = extract<char>(info);

    int start = format.width;
    int length = output.length()-format.width;
    if (format.prec > 0) length -= format.prec;

    debug("ChecksumConverter %s: output to check: \"%s\"\n",
        checksumMap[fnum].name, output.expand(start,length)());
        
    sum = (xorout ^ checksumMap[fnum].func(
        reinterpret_cast<unsigned char*>(output(start)), length, init))
        & mask[checksumMap[fnum].bytes];

    debug("ChecksumConverter %s: output checksum is 0x%X\n",
        checksumMap[fnum].name, sum);

    int i;
    unsigned outchar;
    
    if (format.flags & sign_flag) // decimal
    {
        // get number of decimal digits from number of bytes: ceil(xbytes*2.5)
        i = (checksumMap[fnum].bytes+1)*25/10-2;
        output.print("%0*d", i, sum);
        debug("ChecksumConverter %s: decimal appending %0*d\n",
            checksumMap[fnum].name, i, sum);
    }   
    else
    if (format.flags & alt_flag) // lsb first (little endian)
    {
        for (i = 0; i < checksumMap[fnum].bytes; i++)
        {
            outchar = sum & 0xff;
            debug("ChecksumConverter %s: little endian appending 0x%X\n",
                checksumMap[fnum].name, outchar);
            if (format.flags & zero_flag) // ASCII
                output.print("%02X", outchar);
            else
            if (format.flags & left_flag) // poor man's hex: 0x30 - 0x3F
                output.print("%c%c",
                    ((outchar>>4)&0x0f)|0x30, (outchar&0x0f)|0x30);
            else                          // binary
                output.append(outchar);
            sum >>= 8;
        }
    }
    else // msb first (big endian)
    {
        sum <<= 8*(4-checksumMap[fnum].bytes);
        for (i = 0; i < checksumMap[fnum].bytes; i++)
        {
            outchar = (sum >> 24) & 0xff;
            debug("ChecksumConverter %s: big endian appending 0x%X\n",
                checksumMap[fnum].name, outchar);
            if (format.flags & zero_flag) // ASCII
                output.print("%02X", outchar);
            else
            if (format.flags & left_flag) // poor man's hex: 0x30 - 0x3F
                output.print("%c%c",
                    ((outchar>>4)&0x0f)|0x30, (outchar&0x0f)|0x30);
            else                          // binary
                output.append(outchar);
            sum <<= 8;
        }
    }
    return true;
}

int ChecksumConverter::
scanPseudo(const StreamFormat& format, StreamBuffer& input, long& cursor)
{
    unsigned int sum;
    const char* info = format.info;
    unsigned int init = extract<unsigned int>(info);
    unsigned int xorout = extract<unsigned int>(info);
    int start = format.width;
    int fnum = extract<char>(info);
    int length = cursor-format.width;

    if (format.prec > 0) length -= format.prec;
    
    debug("ChecksumConverter %s: input to check: \"%s\n",
        checksumMap[fnum].name, input.expand(start,length)());

    int expectedLength =
        // get number of decimal digits from number of bytes: ceil(bytes*2.5)
        format.flags & sign_flag ? (checksumMap[fnum].bytes + 1) * 25 / 10 - 2 :
        format.flags & (zero_flag|left_flag) ? 2 * checksumMap[fnum].bytes :
        checksumMap[fnum].bytes;
    
    if (input.length() - cursor < expectedLength)
    {
        debug("ChecksumConverter %s: Input '%s' too short for checksum\n",
            checksumMap[fnum].name, input.expand(cursor)());
        return -1;
    }

    sum = (xorout ^ checksumMap[fnum].func(
        reinterpret_cast<unsigned char*>(input(start)), length, init))
        & mask[checksumMap[fnum].bytes];

    debug("ChecksumConverter %s: input checksum is 0x%0*X\n",
        checksumMap[fnum].name, 2*checksumMap[fnum].bytes, sum);

    int i, j;
    unsigned inchar;
    
    if (format.flags & sign_flag) // decimal
    {
        unsigned int sumin = 0;
        for (i = 0; i < expectedLength; i++)
        {
            inchar = input[cursor+i];
            if (isdigit(inchar)) sumin = sumin*10+inchar-'0';
            else break;
        }
        if (sumin != sum)
        {
            debug("ChecksumConverter %s: Input %0*u does not match checksum %0*u\n", 
                checksumMap[fnum].name, i, sumin, expectedLength, sum);
            return -1;
        }
    }
    else    
    if (format.flags & alt_flag) // lsb first (little endian)
    {
        for (i = 0; i < checksumMap[fnum].bytes; i++)
        {
            if (format.flags & zero_flag) // ASCII
            {
                if (sscanf(input(cursor+2*i), "%2X", &inchar) != 1)
                {
                    debug("ChecksumConverter %s: Input byte '%s' is not a hex byte\n", 
                        checksumMap[fnum].name, input.expand(cursor+2*i,2)());
                    return -1;
                }
            }
            else
            if (format.flags & left_flag) // poor man's hex: 0x30 - 0x3F
            {
                if ((input[cursor+2*i] & 0xf0) != 0x30)
                {
                    debug("ChecksumConverter %s: Input byte 0x%02X is not in range 0x30 - 0x3F\n", 
                        checksumMap[fnum].name, input[cursor+2*i]);
                    return -1;
                }
                if ((input[cursor+2*i+1] & 0xf0) != 0x30)
                {
                    debug("ChecksumConverter %s: Input byte 0x%02X is not in range 0x30 - 0x3F\n", 
                        checksumMap[fnum].name, input[cursor+2*i+1]);
                    return -1;
                }
                inchar = ((input[cursor+2*i] & 0x0f) << 4) | (input[cursor+2*i+1] & 0x0f);
            }
            else                          // binary
            {
                inchar = input[cursor+i] & 0xff;
            }
            if (inchar != ((sum >> 8*i) & 0xff))
            {
                debug("ChecksumConverter %s: Input byte 0x%02X does not match checksum 0x%0*X\n", 
                    checksumMap[fnum].name, inchar, 2*checksumMap[fnum].bytes, sum);
                return -1;
            }
        }
    }
    else // msb first (big endian)
    {
        for (i = checksumMap[fnum].bytes-1, j = 0; i >= 0; i--, j++)
        {
            if (format.flags & zero_flag) // ASCII
            {
                sscanf(input(cursor+2*i), "%2x", &inchar);
            }
            else
            if (format.flags & left_flag) // poor man's hex: 0x30 - 0x3F
            {
                if ((input[cursor+2*i] & 0xf0) != 0x30)
                {
                    debug("ChecksumConverter %s: Input byte 0x%02X is not in range 0x30 - 0x3F\n", 
                        checksumMap[fnum].name, input[cursor+2*i]);
                    return -1;
                }
                if ((input[cursor+2*i+1] & 0xf0) != 0x30)
                {
                    debug("ChecksumConverter %s: Input byte 0x%02X is not in range 0x30 - 0x3F\n", 
                        checksumMap[fnum].name, input[cursor+2*i+1]);
                    return -1;
                }
                inchar = ((input[cursor+2*i] & 0x0f) << 4) | (input[cursor+2*i+1] & 0x0f);
            }
            else                          // binary
            {
                inchar = input[cursor+i] & 0xff;
            }
            if (inchar != ((sum >> 8*j) & 0xff))
            {
                debug("ChecksumConverter %s: Input byte 0x%02X does not match checksum 0x%0*X\n",
                    checksumMap[fnum].name, inchar, 2*checksumMap[fnum].bytes, sum);
                return -1;
            }
        }
    }
    return expectedLength;
}

RegisterConverter (ChecksumConverter, "<");
