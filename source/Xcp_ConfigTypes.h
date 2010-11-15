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
    const uint8        XcpDtoPid;         /* 0 .. 251 */
    const Xcp_PduType* XcpDto2PduMapping; /* XcpRxPdu, XcpTxPdu */
} Xcp_DtoType;

typedef struct {
    void* XcpOdtEntryAddress;
    uint8 XcpOdtEntryLength;
    uint8 XcpOdtEntryNumber; /* 0 .. 254 */
} Xcp_OdtEntryType;

typedef struct {
    const uint8             XcpMaxOdtEntries;   /* XCP_MAX_ODT_ENTRIES */
    const uint8             XcpOdtEntriesCount; /* 0 .. 255 */
    const uint8             XcpOdtEntryMaxSize; /* 0 .. 254 */
    const uint8             XcpOdtNumber;       /* 0 .. 251 */
    const Xcp_DtoType*      XcpOdt2DtoMapping;
    const Xcp_OdtEntryType* XcpOdtEntry;                       /* 1 .. * */
} Xcp_OdtType;

typedef struct {
    const uint16               XcpDaqListNumber; /* 0 .. 65534 */
    const Xcp_DaqListTypeEnum  XcpDaqListType;
    const uint8                XcpMaxOdt;        /* 0 .. 252 */
    const uint8                XcpOdtCount;      /* 0 .. 252 */
    const Xcp_DtoType          XcpDto[10];       /* TODO how many */
    const Xcp_OdtType         *XcpOdt;
} Xcp_DaqListType;

typedef struct {
} Xcp_DemEventParameterRefs;

typedef struct {
    const  uint8             XcpEventChannelMaxDaqList; /* 0 .. 255 */
    const  uint16            XcpEventChannelNumber;     /* 0 .. 65534 */
    const  uint8             XcpEventChannelPriority;   /* 0 .. 255 */
    const  Xcp_DaqListType** XcpEventChannelTriggeredDaqListRef;
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
    const Xcp_DaqListType           *XcpDaqList;
    const Xcp_DemEventParameterRefs *XcpDemEventParameterRef;
    const Xcp_EventChannelType      *XcpEventChannel;
    const Xcp_PduType               *XcpPdu;
          Xcp_SegmentType           *XcpSegment;
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
