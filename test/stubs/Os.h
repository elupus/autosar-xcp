#ifndef OS_H_
#define OS_H_
#include "Std_Types.h"

typedef uint32 TickType;
typedef uint32 CounterType;

/* Implemented by the test harness */
int GetCounterValue(CounterType id, TickType* value);

#endif
