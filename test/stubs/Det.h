#ifndef DET_H_
#define DET_H_
#include "Std_Types.h"

/* Implemented by the test harness, records every reported error */
Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);

#endif
