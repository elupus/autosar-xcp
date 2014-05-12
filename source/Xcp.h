/* Copyright (C) 2010 Joakim Plate, Peter Fridlund
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

#ifndef XCP_H_
#define XCP_H_

#include "Xcp_Cfg.h"
#include "MemMap.h"

#ifdef USE_DEM
#include "Dem.h"
#endif

/*********************************************
 *            MODULE PARAMETERS              *
 *********************************************/

#ifndef MODULE_ID_CANXCP
#define MODULE_ID_CANXCP 210
#endif

#define XCP_VENDOR_ID          1
#define XCP_MODULE_ID          MODULE_ID_CANXCP
#define XCP_AR_MAJOR_VERSION   3
#define XCP_AR_MINOR_VERSION   0
#define XCP_AR_PATCH_VERSION   2

#define XCP_SW_MAJOR_VERSION   0
#define XCP_SW_MINOR_VERSION   1
#define XCP_SW_PATCH_VERSION   0

#define XCP_PROTOCOL_MAJOR_VERSION 1
#define XCP_PROTOCOL_MINOR_VERSION 0

#define XCP_TRANSPORT_MAJOR_VERSION 1
#define XCP_TRANSPORT_MINOR_VERSION 0



/*********************************************
 *               MAIN FUNCTIONS              *
 *********************************************/

void Xcp_Init(const Xcp_ConfigType* Xcp_ConfigPtr);
void Xcp_Disconnect();
void Xcp_MainFunction(void);
void Xcp_MainFunction_Channel(unsigned channel);


#define XCP_E_INV_POINTER     0x01
#define XCP_E_NOT_INITIALIZED 0x02
#define XCP_E_INVALID_PDUID   0x03
#define XCP_E_NULL_POINTER    0x12

#ifndef XCP_DEV_ERROR_DETECT
#   ifdef USE_DET
#       define XCP_DEV_ERROR_DETECT STD_ON
#   else
#       define XCP_DEV_ERROR_DETECT STD_OFF
#   endif
#endif

#ifndef    XCP_VERION_INFO_API
#   define XCP_VERION_INFO_API STD_OFF
#endif

#if(XCP_VERION_INFO_API == STD_ON)
#   define Xcp_GetVersionInfo(_vi) STD_GET_VERSION_INFO(_vi,XCP)
#endif





/*********************************************
 *             OPTIONAL FEATURES             *
 *********************************************/

#ifndef    XCP_FEATURE_BLOCKMODE
#   define XCP_FEATURE_BLOCKMODE STD_ON
#endif

#ifndef    XCP_FEATURE_PGM
#   define XCP_FEATURE_PGM STD_OFF
#endif

#ifndef    XCP_FEATURE_CALPAG
#   define XCP_FEATURE_CALPAG STD_OFF
#endif

#ifndef    XCP_FEATURE_GET_SLAVE_ID
#   define XCP_FEATURE_GET_SLAVE_ID STD_OFF
#endif

#ifndef XCP_FEATURE_DAQ
#   define XCP_FEATURE_DAQ STD_ON
#endif

#ifndef XCP_FEATURE_STIM
#   define XCP_FEATURE_STIM STD_ON
#endif

#ifndef    XCP_FEATURE_DAQSTIM_DYNAMIC
#   define XCP_FEATURE_DAQSTIM_DYNAMIC STD_OFF
#endif

#ifndef    XCP_FEATURE_DIO
#   define XCP_FEATURE_DIO STD_OFF
#endif

#ifndef    XCP_FEATURE_PROTECTION
#   define XCP_FEATURE_PROTECTION STD_OFF
#endif

#ifndef    XCP_FEATURE_TRANSMIT_FAST
#   define XCP_FEATURE_TRANSMIT_FAST STD_OFF
#endif

/*********************************************
 *          PROTOCOL SETTINGS                *
 *********************************************/

#ifndef    XCP_TIMESTAMP_SIZE
#   define XCP_TIMESTAMP_SIZE 0
#endif

#ifndef    XCP_TIMESTAMP_UNIT
#   define XCP_TIMESTAMP_UNIT XCP_TIMESTAMP_UNIT_1MS
#endif


#ifndef XCP_MAX_DTO
#   if(XCP_PROTOCOL == XCP_PROTOCOL_CAN)
#       define XCP_MAX_DTO 8
#   elif(XCP_PROTOCOL == XCP_PROTOCOL_TCP || XCP_PROTOCOL == XCP_PROTOCOL_UDP)
#       define XCP_MAX_DTO 255
#   endif
#endif

#ifndef    XCP_MAX_CTO
#   define XCP_MAX_CTO XCP_MAX_DTO
#endif


#if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_ABSOLUTE)
#   define XCP_MAX_ODT_SIZE (XCP_MAX_DTO - 1) /**< defines the maximum number of bytes that can fit in a dto packages data area*/
#elif(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_BYTE)
#   define XCP_MAX_ODT_SIZE (XCP_MAX_DTO - 2) /**< defines the maximum number of bytes that can fit in a dto packages data area*/
#elif(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD)
#   define XCP_MAX_ODT_SIZE (XCP_MAX_DTO - 3) /**< defines the maximum number of bytes that can fit in a dto packages data area*/
#elif(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD_ALIGNED)
#   define XCP_MAX_ODT_SIZE (XCP_MAX_DTO - 4) /**< defines the maximum number of bytes that can fit in a dto packages data area*/
#endif

#ifndef    XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ
#   define XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ  1
#endif

#ifndef    XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM
#   define XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM 1
#endif


#ifndef    XCP_MAX_ODT_ENTRY_SIZE_DAQ
#   define XCP_MAX_ODT_ENTRY_SIZE_DAQ     XCP_MAX_ODT_SIZE
#endif

#ifndef    XCP_MAX_ODT_ENTRY_SIZE_STIM
#   define XCP_MAX_ODT_ENTRY_SIZE_STIM    XCP_MAX_ODT_SIZE
#endif


#if(XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM > XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ)
#   define XCP_GRANULARITY_ODT_ENTRY_SIZE_MIN XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ
#   define XCP_GRANULARITY_ODT_ENTRY_SIZE_MAX XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM
#else
#   define XCP_GRANULARITY_ODT_ENTRY_SIZE_MIN XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM
#   define XCP_GRANULARITY_ODT_ENTRY_SIZE_MAX XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ
#endif

#ifndef XCP_MAX_ODT_ENTRIES
#   define XCP_MAX_ODT_ENTRIES (XCP_MAX_ODT_SIZE / XCP_GRANULARITY_ODT_ENTRY_SIZE_MIN)
#endif

#ifndef XCP_IDENTIFICATION
#   define XCP_IDENTIFICATION XCP_IDENTIFICATION_RELATIVE_WORD
#endif

#ifndef XCP_ELEMENT_SIZE
#   define XCP_ELEMENT_SIZE 1
#endif

#ifndef    MODULE_ID_XCP
#   define MODULE_ID_XCP MODULE_ID_CANXCP // XCP Routines
#endif

#ifndef    XCP_PDU_ID_BROADCAST
#   define XCP_PDU_ID_BROADCAST XCP_PDU_ID_RX
#endif

/*********************************************
 *          CONFIG ERROR CHECKING            *
 *********************************************/

#ifndef XCP_PDU_ID_RX
#   error XCP_PDU_ID_RX has not been defined
#endif

#ifndef XCP_PDU_ID_TX
#   error XCP_PDU_ID_TX has not been defined
#endif

#ifndef XCP_PROTOCOL
#   error XCP_PROTOCOL has not been defined
#endif

#if(XCP_PROTOCOL == XCP_PROTOCOL_FLEXRAY || XCP_PROTOCOL == XCP_PROTOCOL_USB)
#   error Unsupported protocol selected
#endif

#if(XCP_ELEMENT_SIZE > 1)
#   error Only element size of 1 is currently supported
#endif

#if(XCP_FEATURE_GET_SLAVE_ID == STD_ON && XCP_PROTOCOL == XCP_PROTOCOL_CAN)
#   ifndef XCP_CAN_ID_RX
#       error No recieve can id defined (XCP_CAN_ID_RX)
#   endif
#   if(XCP_PDU_ID_BROADCAST == XCP_PDU_ID_RX)
#       error Invalid XCP_PDU_ID_BROADCAST defined
#   endif
#endif

#endif /* XCP_H_ */
