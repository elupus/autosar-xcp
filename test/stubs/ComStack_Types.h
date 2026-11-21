#ifndef COMSTACK_TYPES_H_
#define COMSTACK_TYPES_H_
#include "Std_Types.h"

typedef uint16 PduIdType;
typedef uint16 PduLengthType;

typedef struct {
    uint8*        SduDataPtr;
    PduLengthType SduLength;
} PduInfoType;

#endif
