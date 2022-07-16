#ifndef LIBNBUSS_CRC16_H_
#define LIBNBUSS_CRC16_H_

// taken from:
// https://github.com/lammertb/libcrc

/*
 * Library: libcrc
 * File:    include/checksum.h
 * Author:  Lammert Bies
 *
 * This file is licensed under the MIT License as stated below
 *
 * Copyright (c) 1999-2018 Lammert Bies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Description
 * -----------
 * The headerfile include/checksum.h contains the definitions and prototypes
 * for routines that can be used to calculate several kinds of checksums.
 */


#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * #define CRC_POLY_xxxx
 *
 * The constants of the form CRC_POLY_xxxx define the polynomials for some well
 * known CRC calculations.
 */

#define		CRC_POLY_16		0xA001
#define		CRC_POLY_32		0xEDB88320ul
#define		CRC_POLY_64		0x42F0E1EBA9EA3693ull
#define		CRC_POLY_CCITT		0x1021
#define		CRC_POLY_DNP		0xA6BC
#define		CRC_POLY_KERMIT		0x8408
#define		CRC_POLY_SICK		0x8005

/*
 * #define CRC_START_xxxx
 *
 * The constants of the form CRC_START_xxxx define the values that are used for
 * initialization of a CRC value for common used calculation methods.
 */

#define		CRC_START_8		0x00
#define		CRC_START_16		0x0000
#define		CRC_START_MODBUS	0xFFFF
#define		CRC_START_XMODEM	0x0000
#define		CRC_START_CCITT_1D0F	0x1D0F
#define		CRC_START_CCITT_FFFF	0xFFFF
#define		CRC_START_KERMIT	0x0000
#define		CRC_START_SICK		0x0000
#define		CRC_START_DNP		0x0000
#define		CRC_START_32		0xFFFFFFFFul
#define		CRC_START_64_ECMA	0x0000000000000000ull
#define		CRC_START_64_WE		0xFFFFFFFFFFFFFFFFull

/*
 * Prototype list of global functions
 */

//unsigned char *		checksum_NMEA(      const unsigned char *input_str, unsigned char *result  );
//uint8_t			crc_8(              const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_16(             const unsigned char *input_str, size_t num_bytes       );
//uint32_t		crc_32(             const unsigned char *input_str, size_t num_bytes       );
//uint64_t		crc_64_ecma(        const unsigned char *input_str, size_t num_bytes       );
//uint64_t		crc_64_we(          const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_ccitt_1d0f(     const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_ccitt_ffff(     const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_dnp(            const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_kermit(         const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_modbus(         const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_sick(           const unsigned char *input_str, size_t num_bytes       );
//uint16_t		crc_xmodem(         const unsigned char *input_str, size_t num_bytes       );
//uint8_t			update_crc_8(       uint8_t  crc, unsigned char c                          );
//uint16_t		update_crc_16(      uint16_t crc, unsigned char c                          );
//uint32_t		update_crc_32(      uint32_t crc, unsigned char c                          );
//uint64_t		update_crc_64_ecma( uint64_t crc, unsigned char c                          );
//uint16_t		update_crc_ccitt(   uint16_t crc, unsigned char c                          );
//uint16_t		update_crc_dnp(     uint16_t crc, unsigned char c                          );
//uint16_t		update_crc_kermit(  uint16_t crc, unsigned char c                          );
//uint16_t		update_crc_sick(    uint16_t crc, unsigned char c, unsigned char prev_byte );

/*
 * Global CRC lookup tables
 */

//extern const uint32_t	crc_tab32[];
//extern const uint64_t	crc_tab64[];

#ifdef __cplusplus
}// Extern C
#endif





namespace nbuss_server {




class Crc16 {
	bool             crc_tab16_init          = false;
	uint16_t         crc_tab16[256];

	/*
	 * static void init_crc16_tab( void );
	 *
	 * For optimal performance uses the CRC16 routine a lookup table with values
	 * that can be used directly in the XOR arithmetic in the algorithm. This
	 * lookup table is calculated by the init_crc16_tab() routine, the first time
	 * the CRC function is called.
	 */
	void init_crc16_tab( void );

public:
	Crc16() { init_crc16_tab(); }
	virtual ~Crc16();



	/*
	 * uint16_t crc_16( const unsigned char *input_str, size_t num_bytes );
	 *
	 * The function crc_16() calculates the 16 bits CRC16 in one pass for a byte
	 * string of which the beginning has been passed to the function. The number of
	 * bytes to check is also a parameter. The number of the bytes in the string is
	 * limited by the constant SIZE_MAX.
	 */

	uint16_t crc_16( const unsigned char *input_str, size_t num_bytes );

	uint16_t update_crc_16( uint16_t crc, const unsigned char *input_str, size_t num_bytes );

	/*
	 * uint16_t crc_modbus( const unsigned char *input_str, size_t num_bytes );
	 *
	 * The function crc_modbus() calculates the 16 bits Modbus CRC in one pass for
	 * a byte string of which the beginning has been passed to the function. The
	 * number of bytes to check is also a parameter.
	 */

	uint16_t crc_modbus( const unsigned char *input_str, size_t num_bytes );


	/*
	 * uint16_t update_crc_16( uint16_t crc, unsigned char c );
	 *
	 * The function update_crc_16() calculates a new CRC-16 value based on the
	 * previous value of the CRC and the next byte of data to be checked.
	 */

	uint16_t update_crc_16( uint16_t crc, unsigned char c );

};

} /* namespace nbuss_server */

#endif /* LIBNBUSS_CRC16_H_ */
