#ifndef CANIF_H_
#define CANIF_H_
#include "Std_Types.h"
#include "ComStack_Types.h"

typedef uint32 Can_IdType;

/* Implemented by the test harness, records every transmitted frame */
Std_ReturnType CanIf_Transmit(PduIdType CanTxPduId, const PduInfoType* PduInfoPtr);

#endif
