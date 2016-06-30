/***************************************************************************
 *
 * config.h for tnef decoder by Brandon Long
 * Based on config.h from S3MOD by Dan Marks and David Jeske
 *
 * (C) 1994,1995 By Daniel Marks and David Jeske
 *
 * While we retain the copyright to this code, this source code is FREE.
 * You may use it in any way you wish, in any product you wish. You may 
 * NOT steal the copyright for this code from us.
 *
 * We respectfully ask that you email one of us, if possible, if you
 * produce something significant with this code, or if you have any bug 
 * fixes to contribute.  We also request that you give credit where
 * credit is due if you include part of this code in a program of your own.
 *                                                 
 ***************************************************************************
 *
 * config.h - compile time configuration options and system specific defines
 *
 */

#ifndef _CONFIG_H
#define _CONFIG_H 1

/***************************************************************************/
/* The following are system specific settings */
/***************************************************************************/

#if defined(SUN)
#define BIT_32
#define ___TNEF_BYTE_ORDER 4321
#undef NEAR_FAR_PTR

#elif defined (HPUX)
#define BIT_32
#define ___TNEF_BYTE_ORDER 4321
#undef NEAR_FAR_PTR

#elif defined(DEC)
#undef NEAR_FAR_PTR

#elif defined(__sgi)
#define BIT_32
#define ___TNEF_BYTE_ORDER 4321
#undef NEAR_FAR_PTR

#elif defined(AIX)
#undef NEAR_FAR_PTR
#define ___TNEF_BYTE_ORDER 4321
#define BIT_32

#elif defined(LINUX)
#define BIT_32
#undef NEAR_FAR_PTR

#elif defined(MSDOS)
#define NEAR_FAR_PTR
#undef BIT_32

#else
#undef NEAR_FAR_PTR
#define BIT_32


#endif /* OS/MACH TYPE */

/***************************************************************************/
/* 16/32 Bit and Byte Order hacks */
/***************************************************************************/

#ifdef BIT_32
typedef short int int16;
typedef unsigned short int uint16;
typedef int int32;
typedef unsigned int uint32;
typedef char int8;
typedef unsigned char uint8;
#else
typedef int int16;
typedef unsigned int uint16;
typedef long int int32;
typedef unsigned long int uint32;
typedef char int8;
typedef unsigned char uint8;
#endif /* BIT_32 */

#ifndef WIN32_TYPES
#define ULONG uint32
#define SCODE uint32
#define FAR
#define LPVOID void *
#define WORD uint16
#define DWORD uint32
#define LONG int32
#define BYTE uint8
#endif /* !WIN32_TYPES */

#define endian_switch(x) (((((uint16)(x)) & 0xFF00) >> 8) | \
			  ((((uint16)(x)) & 0xFF) << 8))

#define long_endian_switch(x) ( ((((uint32)(x)) & 0xFF00UL) << 8) | \
			        ((((uint32)(x)) & 0xFFUL) << 24) | \
			        ((((uint32)(x)) & 0xFF0000UL) >> 8) | \
			        ((((uint32)(x)) & 0xFF000000UL) >> 24))

#if ___TNEF_BYTE_ORDER == 4321
#define big_endian(x) (x)
#define long_big_endian(x) (x)
#define little_endian(x) (endian_switch(x))
#define long_little_endian(x) (long_endian_switch(x))
#else
#define big_endian(x) (endian_switch(x))
#define long_big_endian(x) (long_endian_switch(x))
#define little_endian(x) (x)
#define long_little_endian(x) (x)
#endif /* ___TNEF_BYTE_ORDER */

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#endif /* _CONFIG_H */
