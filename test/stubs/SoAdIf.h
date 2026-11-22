#ifndef SOADIF_H_
#define SOADIF_H_
#include "Std_Types.h"
#include "ComStack_Types.h"

/* Implemented by the test harness, records every transmitted frame */
Std_ReturnType SoAdIf_Transmit(PduIdType SoAdSrcPduId, const PduInfoType* SoAdSrcPduInfoPtr);

#endif
