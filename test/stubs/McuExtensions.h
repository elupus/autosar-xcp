#ifndef MCUEXTENSIONS_H_
#define MCUEXTENSIONS_H_
#include <stdint.h>

/* Tests are single threaded, so critical sections do nothing */
typedef uintptr_t imask_t;

static inline imask_t McuE_EnterCriticalSection(void) { return 0; }
static inline void    McuE_ExitCriticalSection(imask_t m) { (void)m; }

#endif
