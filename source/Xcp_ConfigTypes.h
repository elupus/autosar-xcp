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

#define XCP_IDENTIFICATION_ABSOLUTE              0x0
#define XCP_IDENTIFICATION_RELATIVE_BYTE         0x1
#define XCP_IDENTIFICATION_RELATIVE_WORD         0x2
#define XCP_IDENTIFICATION_RELATIVE_WORD_ALIGNED 0x3

typedef enum {
    DAQ
,   DAQ_STIM
} Xcp_DaqListTypeEnum;

typedef enum {
    XCP_DYNAMIC_STATE_UNDEFINED = 0,
	XCP_DYNAMIC_STATE_FREE_DAQ,
	XCP_DYNAMIC_STATE_ALLOC_DAQ,
	XCP_DYNAMIC_STATE_ALLOC_ODT,
	XCP_DYNAMIC_STATE_ALLOC_ODT_ENTRY,
}Xcp_DaqListConfigStateEnum;

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

typedef struct OdtEntry{
            intptr_t    XcpOdtEntryAddress;
            uint8       XcpOdtEntryLength;
            uint8       XcpOdtEntryNumber; /* 0 .. 254 */

    /* Implementation defined */

     struct OdtEntry    *XcpNextOdtEntry;
            uint8       BitOffSet;
            uint8       XcpOdtEntryExtension;
} Xcp_OdtEntryType;

typedef struct Xcp_StimBufferType {
    uint8               len;
    unsigned char       data[8];
    struct Xcp_StimBufferType*  next;
} Xcp_StimBufferType;

typedef struct Odt{
          uint8             XcpMaxOdtEntries;   /* XCP_MAX_ODT_ENTRIES */
          uint8             XcpOdtEntriesCount; /* 0 .. 255 */
          uint8             XcpOdtEntryMaxSize; /* 0 .. 254 */
          uint8             XcpOdtNumber;       /* 0 .. 251 */
          Xcp_DtoType       XcpOdt2DtoMapping;
          Xcp_OdtEntryType* XcpOdtEntry;        /* 0 .. * */

    /* Implementation defined */
          int               XcpOdtEntriesValid; /* Number of non zero entries */
   struct Odt              *XcpNextOdt;
   struct Xcp_StimBufferType    XcpStimBuffer;
} Xcp_OdtType;

typedef enum {
    XCP_DAQLIST_MODE_SELECTED  = 1 << 0,
    XCP_DAQLIST_MODE_STIM      = 1 << 1,
    XCP_DAQLIST_MODE_TIMESTAMP = 1 << 4,
    XCP_DAQLIST_MODE_PIDOFF    = 1 << 5,
    XCP_DAQLIST_MODE_RUNNING   = 1 << 6,
    XCP_DAQLIST_MODE_RESUME    = 1 << 7,
} Xcp_DaqListModeEnum;

typedef enum {
    XCP_DAQLIST_PROPERTY_PREDEFINED  = 1 << 0,
    XCP_DAQLIST_PROPERTY_EVENTFIXED  = 1 << 1,
    XCP_DAQLIST_PROPERTY_DAQ         = 1 << 2,
    XCP_DAQLIST_PROPERTY_STIM        = 1 << 3
} Xcp_DaqListPropertyEnum;

typedef struct {
          Xcp_DaqListModeEnum     Mode;           /**< bitfield for the current mode of the DAQ list */
          uint16                  EventChannel;   /* */
          uint8                   Prescaler;      /* */
          uint8                   Priority;       /* */
          Xcp_DaqListPropertyEnum Properties;     /**< bitfield for the properties of the DAQ list */
} Xcp_DaqListParams;



typedef struct DaqList{
            uint16               XcpDaqListNumber; /* 0 .. 65534 */
            Xcp_DaqListTypeEnum  XcpDaqListType;
            uint8                XcpMaxOdt;        /* 0 .. 252 */
            uint8                XcpOdtCount;      /* 0 .. 252 */
            Xcp_OdtType         *XcpOdt;           /**< reference to an array of Odt's configured for this Daq list */

    /* Weird container in specification that should hold all Dto of this  *
     * daqlist. But since it contains the Odt id which needs to be unique *
     * for each odt, it makes no sense to hold this here. So we define it *
     * in the XcpOdt type instead                                         */
    /* const Xcp_DtoType XcpDto[?]; */

    /* Implementation defined */
            Xcp_DaqListParams          XcpParams;
     struct DaqList                   *XcpNextDaq;
} Xcp_DaqListType;

typedef struct {
} Xcp_DemEventParameterRefs;

typedef enum {
    XCP_EVENTCHANNEL_PROPERTY_DAQ         = 1 << 2,
    XCP_EVENTCHANNEL_PROPERTY_STIM        = 1 << 3,
} Xcp_EventChannelPropertyEnum;

typedef struct {
    const  uint8             			XcpEventChannelMaxDaqList; /* 0 .. 255 | 0 = Unlimited */
    const  uint16            			XcpEventChannelNumber;     /* 0 .. 65534 */
    const  uint8             			XcpEventChannelPriority;   /* 0 .. 255 */
           Xcp_DaqListType** 			XcpEventChannelTriggeredDaqListRef;

    /* Implementation defined */
           uint8             			XcpEventChannelCounter;
    const  char*           			  	XcpEventChannelName;
           uint8             			XcpEventChannelDaqCount;
    const  Xcp_EventChannelPropertyEnum	XcpEventChannelProperties;

    const Xcp_TimestampUnitType 		XcpEventChannelUnit;
    const uint8                 		XcpEventChannelRate;
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
          uint16  XcpDaqCount;
    const boolean XcpDevErrorDetect;
    const float   XcpMainFunctionPeriod;
    const uint8   XcpMaxCto;             /* 8 .. 255 */
          uint16  XcpMaxDaq;             /* 0 .. 65535, XCP_MAX_DAQ */
    const uint16  XcpMaxDto;             /* 8 .. 65535 */
    const uint16  XcpMaxEventChannel;    /* 0 .. 65535, XCP_MAX_EVENT_CHANNEL */
    const uint16  XcpMinDaq;             /* 0 .. 255  , XCP_MIN_DAQ */
    const boolean XcpOnCddEnabled;
    const boolean XcpOnEthernetEnabled;
    const boolean XcpOnFlexRayEnabled;
    const boolean XcpVersionInfoApi;
} Xcp_GeneralType;



#endif /* XCP_CONFIGTYPES_H_ */
