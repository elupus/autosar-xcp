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
#if(XCP_STANDALONE == 0)
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

#ifndef XCP_MAX_SEGMENT
#define XCP_MAX_SEGMENT 0
#endif

#ifndef XCP_FEATURE_BLOCKMODE
#define XCP_FEATURE_BLOCKMODE 1
#endif

#ifndef MODULE_ID_XCP
#define MODULE_ID_XCP MODULE_ID_CANXCP // XCP Routines
#endif

#endif /* XCP_H_ */
