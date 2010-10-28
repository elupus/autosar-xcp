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

#include "Std_Types.h"
#include "Xcp_Cfg.h"

#ifndef XCP_STANDALONE
#   include "Os.h"
#   if(XCP_DEV_ERROR_DETECT)
#       include "Dem.h"
#   endif
#   include "ComStack_Types.h"
#endif

static Xcp_GeneralType general = 
{
    .XcpMaxDaq          = XCP_MAX_DAQ
  , .XcpMaxEventChannel = XCP_MAX_EVENT_CHANNEL
  , .XcpMinDaq          = XCP_MIN_DAQ
};


const Xcp_ConfigType *g_config;


#if(Xcp_VERION_INFO_API)
void Xcp_GetVersionInfo_Impl(Std_VersionInfoType* versioninfo)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!versioninfo) {
#       error TODO error XCP_E_INV_POINTER
        return
    }
#endif
#error TODO fill version info
}
#endif

void Xcp_Init(const Xcp_ConfigType* Xcp_ConfigPtr)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!Xcp_ConfigPtr) {
#       error TODO error XCP_E_INV_POINTER
        return;
    }
#endif
    g_config = Xcp_ConfigPtr;

}



/* Scheduled function of the XCP module */
void Xcp_MainFunction(void)
{
}




