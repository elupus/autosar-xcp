#ifndef DEBUG_H_
#define DEBUG_H_

#define DEBUG_LOW    1
#define DEBUG_MEDIUM 2
#define DEBUG_HIGH   3
#define DEBUG_LVL    DEBUG_HIGH

/* Silent in unit tests */
#define DEBUG(lvl, ...) do {} while(0)

#endif
