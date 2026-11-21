/* Minimal host stub of the AUTOSAR Std_Types.h, only for unit tests */
#ifndef STD_TYPES_H_
#define STD_TYPES_H_

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;

typedef uint8 Std_ReturnType;

#define E_OK     ((Std_ReturnType)0)
#define E_NOT_OK ((Std_ReturnType)1)

#define STD_HIGH 1
#define STD_LOW  0
#define STD_ON   1
#define STD_OFF  0

#endif
