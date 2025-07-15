#pragma once
#ifndef _MSC_VER
#error local C99 type definition should only be used in Visual C++ before 2010
#endif
#if _MSC_VER > 1500
#error local C99 type definition should only be used in Visual C++ before 2010
#endif

/* a minimal set of C99 types for use with MSVC */

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef __int64 int64_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
