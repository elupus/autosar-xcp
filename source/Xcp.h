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

#ifndef XCP_H_
#define XCP_H_

#include "Xcp_Cfg.h"
#include "MemMap.h"

#ifdef USE_DEM
#include "Dem.h"
#endif

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


void Xcp_Init(const Xcp_ConfigType* Xcp_ConfigPtr);
void Xcp_MainFunction(void);

#if(XCP_DEV_ERROR_DETECT)
#   define XCP_E_INV_POINTER     0x01
#   define XCP_E_NOT_INITIALIZED 0x02
#   define XCP_E_INVALID_PDUID   0x03
#   define XCP_E_NULL_POINTER    0x12
#endif

#if(Xcp_VERION_INFO_API)
#define Xcp_GetVersionInfo(_vi) STD_GET_VERSION_INFO(_vi,XCP)
#endif


#ifndef    XCP_MAX_SEGMENT
#   define XCP_MAX_SEGMENT 0
#endif

#ifndef    XCP_FEATURE_BLOCKMODE
#   define XCP_FEATURE_BLOCKMODE STD_ON
#endif

#ifndef    XCP_FEATURE_PGM
#   define XCP_FEATURE_PGM STD_OFF
#endif

#ifndef XCP_FEATURE_CALPAG
#   if(XCP_MAX_SEGMENT > 0)
#       define XCP_FEATURE_CALPAG STD_ON
#   else
#       define XCP_FEATURE_CALPAG STD_OFF
#   endif
#endif

#ifndef XCP_FEATURE_DAQ
#   if(XCP_MAX_DAQ > 0)
#       define XCP_FEATURE_DAQ STD_ON
#   else
#       define XCP_FEATURE_DAQ STD_OFF
#   endif
#endif

#ifndef XCP_FEATURE_STIM
#   if(XCP_MAX_DAQ > 0)
#       define XCP_FEATURE_STIM STD_ON
#   else
#       define XCP_FEATURE_STIM STD_OFF
#   endif
#endif

#ifndef XCP_FEATURE_DAQSTIM_DYNAMIC
#define XCP_FEATURE_DAQSTIM_DYNAMIC STD_OFF
#endif

#ifndef XCP_FEATURE_PROGRAM
#define XCP_FEATURE_PROGRAM STD_OFF
#endif

#ifndef XCP_TIMESTAMP_SIZE
#define XCP_TIMESTAMP_SIZE 0
#endif

#ifndef XCP_TIMESTAMP_UNIT
#define XCP_TIMESTAMP_UNIT XCP_TIMESTAMP_UNIT_1MS
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

#ifndef XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ
#define XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ  1
#endif

#ifndef XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM
#define XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM 1
#endif


#ifndef XCP_MAX_ODT_ENTRY_SIZE_DAQ
#define XCP_MAX_ODT_ENTRY_SIZE_DAQ     XCP_MAX_ODT_SIZE
#endif

#ifndef XCP_MAX_ODT_ENTRY_SIZE_STIM
#define XCP_MAX_ODT_ENTRY_SIZE_STIM    XCP_MAX_ODT_SIZE
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

#if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_BYTE)
#   if(XCP_MAX_DAQ > 255)
#       error Incompatible number of daq with identification type
#   endif
#elif(XCP_IDENTIFICATION == XCP_IDENTIFICATION_ABSOLUTE)
#   if(XCP_MAX_DAQ * XCP_MAX_ODT_ENTRIES > 251)
#       error Incompatible number of daqs and odts with identification type
#   endif
#endif

#ifndef XCP_PDU_ID_RX
#   error XCP_PDU_ID_RX has not been defined
#endif

#ifndef XCP_PDU_ID_TX
#   error XCP_PDU_ID_TX has not been defined
#endif

/** Alignment of elements, only 1 is currently supported */
#define XCP_ELEMENT_SIZE 1

#ifndef MODULE_ID_XCP
#define MODULE_ID_XCP MODULE_ID_CANXCP // XCP Routines
#endif

#endif /* XCP_H_ */
