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
    const uint8             XcpMaxOdtEntries;
    const uint8             XcpOdtEntriesCount; /* 0 .. 255 */
    const uint8             XcpOdtEntryMaxSize; /* 0 .. 254 */
    const uint8             XcpOdtNumber;       /* 0 .. 251 */
    const Xcp_DtoType*      XcpOdt2DtoMapping;
    const Xcp_OdtEntryType* XcpOdtEntry;        /* 1 .. * */
} Xcp_OdtType;

/* Init Structure */
typedef struct {
    const uint16               XcpDaqListNumber; /* 0 .. 65534 */
    const Xcp_DaqListTypeEnum  XcpDaqListType;
    const uint8                XcpMaxOdt;        /* 0 .. 252 */
    const uint8                XcpOdtCount;      /* 0 .. 252 */
    const Xcp_DtoType         *XcpDto;
    const Xcp_OdtType         *XcpOto;
} Xcp_DaqListType;

typedef struct {
} Xcp_DemEventParameterRefs;

typedef struct {
    const  uint8            XcpEventChannelMaxDaqList; /* 0 .. 255 */
    const  uint16           XcpEventChannelNumber;     /* 0 .. 65534 */
    const  uint8            XcpEventChannelPriority;   /* 0 .. 255 */
    const  Xcp_DaqListType* XcpEventChannelTriggeredDaqListRef;
} Xcp_EventChannelType;

typedef struct {
    const Xcp_DaqListType          *XcpDaqList;
    const Xcp_DemEventParameterRefs XcpDemEventParameterRef[2];
    const Xcp_EventChannelType     *XcpEventChannel;
    const Xcp_PduType              *XcpPdu;
} Xcp_ConfigType;

typedef struct {
    const uint16  XcpDaqCount;
    const boolean XcpDevErrorDetect;
    const float   XcpMainFunctionPerios;
    const uint8   XcpMaxCto;             /* 8 .. 255 */
    const uint16  XcpMaxDaq;             /* 0 .. 65535 */
    const uint16  XcpMaxDto;             /* 8 .. 65535 */
    const uint16  XcpMaxEventChannel;    /* 0 .. 65535 */
    const uint16  XcpMinDaq;             /* 0 .. 255 */
    const boolean XcpOnCddEnabled;
    const boolean XcpOnEthernetEnabled;
    const boolean XcpOnFlexRayEnabled;
    const boolean XcpVersionInfoApi;
} Xcp_GeneralType;

#endif /* XCP_CONFIGTYPES_H_ */
