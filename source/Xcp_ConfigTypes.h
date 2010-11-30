/* Copyright (C) 2010 Joakim Plate
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef XCP_CONFIGTYPES_H_
#define XCP_CONFIGTYPES_H_

#include "Std_Types.h"

typedef enum {
    XCP_TIMESTAMP_UNIT_1NS   = 0x00,
    XCP_TIMESTAMP_UNIT_10NS  = 0x01,
    XCP_TIMESTAMP_UNIT_100NS = 0x02,
    XCP_TIMESTAMP_UNIT_1US   = 0x03,
    XCP_TIMESTAMP_UNIT_10US  = 0x04,
    XCP_TIMESTAMP_UNIT_100US = 0x05,
    XCP_TIMESTAMP_UNIT_1MS   = 0x06,
    XCP_TIMESTAMP_UNIT_10MS  = 0x07,
    XCP_TIMESTAMP_UNIT_100MS = 0x08,
    XCP_TIMESTAMP_UNIT_1S    = 0x09,
} Xcp_TimestampUnitType;

typedef enum {
    DAQ
,   DAQ_STIM
} Xcp_DaqListTypeEnum;

typedef struct {
    const uint16 XcpRxPduId;
    void*        XcpRxPduRef;
} Xcp_RxPduType;

typedef struct {
    const uint16 XcpTxConfirmationPduId;
} Xcp_TxConfirmationType;

typedef struct {
    const uint16            XcpTxPduId;
    void*                   XcpTxPduRef;
    Xcp_TxConfirmationType* XcpTxConfirmation;
} Xcp_TxPduType;

typedef struct {
    const Xcp_RxPduType* XcpRxPdu;
    const Xcp_TxPduType* XcpTxPdu;
} Xcp_PduType;

typedef struct {
          uint8        XcpDtoPid;         /* 0 .. 251 */
    const Xcp_PduType* XcpDto2PduMapping; /* XcpRxPdu, XcpTxPdu */
} Xcp_DtoType;

typedef struct {
    intptr_t XcpOdtEntryAddress;
    uint8    XcpOdtEntryLength;
    uint8    XcpOdtEntryNumber; /* 0 .. 254 */

    /* Implementation defined */

    uint8  BitOffSet;
    uint8  XcpOdtEntryExtension;

} Xcp_OdtEntryType;

typedef struct {
    const uint8             XcpMaxOdtEntries;   /* XCP_MAX_ODT_ENTRIES */
    const uint8             XcpOdtEntriesCount; /* 0 .. 255 */
    const uint8             XcpOdtEntryMaxSize; /* 0 .. 254 */
    const uint8             XcpOdtNumber;       /* 0 .. 251 */
          Xcp_DtoType       XcpOdt2DtoMapping;  /* 1 UNUSED */
          Xcp_OdtEntryType* XcpOdtEntry;        /* 1 .. * */

    /* Implementation defined */
          int               XcpOdtEntriesValid; /* Number of non zero entries */
} Xcp_OdtType;

typedef union {
    uint8 u8;
    struct {
        unsigned selected  : 1;
        unsigned direction : 1;
        unsigned reserved  : 2;
        unsigned timestamp : 1;
        unsigned pidoff    : 1;
        unsigned resume    : 1;
        unsigned running   : 1;
    } bit;
} Xcp_DaqModeType;

typedef struct {
    Xcp_DaqModeType Mode;   /************************************************
							 * bit# 0	Selected							*
							 *		1	Direction: Daq/Stim 				*
	 	 	 	 	 	 	 *	 	2	----								*
	 	 	 	 	 	 	 *	 	3	----								*
	 	  	 	 	 	 	 *	 	4	Timestamp Enabled					*
	 	  	 	 	 	 	 *	 	5	Pid off: 1 = true					*
	 	  	 	 	 	 	 *	 	6	Part of config used in Resume mode	*
	 	 	 	 	 	 	 *	 	7	Running								*
	 	  	 	 	 	 	 ************************************************/
	uint16	EventChannel;	/* */
	uint8 	Prescaler;		/* */
	uint8	Priority;		/* */
	uint8	Properties;		/************************************************
	 	 	 	 	 	 	 * bit# 0	Predefined							*
	 	 	 	 	 	 	 *	 	1	Event_fixed							*
	 	 	 	 	 	 	 *	 	2	Daq mode possible  |daq or stim must*
	 	 	 	 	 	 	 *	 	3	Stim mode possible |be possible!!	*
	 	 	 	 	 	 	 *	 	4	----								*
	 	 	 	 	 	 	 *	 	5	----								*
	 	 	 	 	 	 	 *	 	6	----								*
	 	 	 	 	 	 	 *	 	7	----								*
	 	 	 	 	 	 	 ************************************************/
} Xcp_DaqListParams;

typedef struct {
    const uint16               XcpDaqListNumber; /* 0 .. 65534 */
    const Xcp_DaqListTypeEnum  XcpDaqListType;
    const uint8                XcpMaxOdt;        /* 0 .. 252 */
    const uint8                XcpOdtCount;      /* 0 .. 252 */
    const Xcp_DtoType          XcpDto[10];       /* TODO how many */
          Xcp_OdtType         *XcpOdt;

    /* Implementation defined */
    Xcp_DaqListParams		   XcpParams;

} Xcp_DaqListType;

typedef struct {
} Xcp_DemEventParameterRefs;

typedef struct {
    const  uint8             XcpEventChannelMaxDaqList; /* 0 .. 255 */
    const  uint16            XcpEventChannelNumber;     /* 0 .. 65534 */
    const  uint8             XcpEventChannelPriority;   /* 0 .. 255 */
           Xcp_DaqListType** XcpEventChannelTriggeredDaqListRef;

    /* Implementation defined */
           uint8             XcpEventChannelCounter;
    const  char*             XcpEventChannelName;

    const Xcp_TimestampUnitType XcpEventChannelUnit;
    const uint8                 XcpEventChannelRate;
} Xcp_EventChannelType;

enum {
    XCP_ACCESS_ECU_ACCESS_WITHOUT_XCP       = 1 << 0,
    XCP_ACCESS_ECU_ACCESS_WITH_XCP          = 1 << 1,
    XCP_ACCESS_XCP_READ_ACCESS_WITHOUT_ECU  = 1 << 2,
    XCP_ACCESS_READ_ACCESS_WITH_ECU         = 1 << 3,
    XCP_ACCESS_XCP_WRITE_ACCESS_WITHOUT_ECU = 1 << 4,
    XCP_ACCESS_XCP_WRITE_ACCESS_WITH_ECU    = 1 << 5,
    XCP_ACCESS_ALL                          = 0x3f
} XcpAccessFlagsType;

typedef struct {
    const uint8 XcpAccessFlags;
    uint8       XcpMaxPage;
    uint8       XcpPageXcp;
    uint8       XcpPageEcu;
} Xcp_SegmentType;

typedef struct {
    const char* XcpCaption;   /**< ASCII text describing device */
    const char* XcpMC2File;   /**< ASAM-MC2 filename without path and extension */
    const char* XcpMC2Path;   /**< ASAM-MC2 filename with path and extension */
    const char* XcpMC2Url;    /**< ASAM-MC2 url to file */
    const char* XcpMC2Upload; /**< ASAM-MC2 file to upload */
} Xcp_InfoType;

typedef struct {
          Xcp_DaqListType           *XcpDaqList;
    const Xcp_DemEventParameterRefs *XcpDemEventParameterRef;
          Xcp_EventChannelType      *XcpEventChannel;
    const Xcp_PduType               *XcpPdu;

    /* Implementation defined */
          Xcp_SegmentType           *XcpSegment;
    const Xcp_InfoType               XcpInfo;
} Xcp_ConfigType;

typedef struct {
    const uint16  XcpDaqCount;
    const boolean XcpDevErrorDetect;
    const float   XcpMainFunctionPerios;
    const uint8   XcpMaxCto;             /* 8 .. 255 */
    const uint16  XcpMaxDaq;             /* 0 .. 65535, XCP_MAX_DAQ */
    const uint16  XcpMaxDto;             /* 8 .. 65535 */
    const uint16  XcpMaxEventChannel;    /* 0 .. 65535, XCP_MAX_EVENT_CHANNEL */
    const uint16  XcpMinDaq;             /* 0 .. 255  , XCP_MIN_DAQ */
    const boolean XcpOnCddEnabled;
    const boolean XcpOnEthernetEnabled;
    const boolean XcpOnFlexRayEnabled;
    const boolean XcpVersionInfoApi;
} Xcp_GeneralType;



#endif /* XCP_CONFIGTYPES_H_ */
