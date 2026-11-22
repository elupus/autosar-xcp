/* Hand written configuration used by the unit tests (Ethernet transport) */
#ifndef XCP_CFG_H_
#define XCP_CFG_H_

#include "Std_Types.h"
#include "Xcp_ConfigTypes.h"

#define XCP_PROTOCOL              XCP_PROTOCOL_TCP
#define XCP_DEV_ERROR_DETECT      STD_ON

#define XCP_MAX_RXTX_QUEUE        8
#define XCP_IDENTIFICATION        XCP_IDENTIFICATION_RELATIVE_WORD

#define XCP_FEATURE_BLOCKMODE     STD_ON
#define XCP_FEATURE_PROTECTION    STD_ON
#define XCP_FEATURE_CALPAG        STD_OFF
#define XCP_FEATURE_DAQ           STD_ON
#define XCP_FEATURE_STIM          STD_ON
#define XCP_FEATURE_PGM           STD_OFF
#define XCP_FEATURE_DIO           STD_OFF
#define XCP_FEATURE_GET_SLAVE_ID  STD_OFF

#define XCP_PDU_ID_RX             1
#define XCP_PDU_ID_TX             2

#include "XcpOnEth_Cfg.h"

#endif
