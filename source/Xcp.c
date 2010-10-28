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

#ifdef XCP_STANDALONE
#   include <stdio.h>
#else
#   include "Os.h"
#   if(XCP_DEV_ERROR_DETECT)
#       include "Dem.h"
#   endif
#   include "ComStack_Types.h"
#endif

static Xcp_GeneralType g_general = 
{
    .XcpMaxDaq          = XCP_MAX_DAQ
  , .XcpMaxEventChannel = XCP_MAX_EVENT_CHANNEL
  , .XcpMinDaq          = XCP_MIN_DAQ
  , .XcpDaqCount        = XCP_MIN_DAQ
  , .XcpMaxDto          = XCP_MAX_DTO
  , .XcpMaxCto          = XCP_MAX_CTO
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


/* Process all entriesin DAQ */
static void Xcp_ProcessDaq(const Xcp_DaqListType* daq)
{
#ifdef XCP_STANDALONE
    printf("Processing DAQ %d\n", daq->XcpDaqListNumber);
#endif

    for(int o = 0; 0 < daq->XcpOdtCount; o++) {
        const Xcp_OdtType* odt = daq->XcpOdt+o;
        for(int e = 0; e < odt->XcpOdtEntriesCount; e++) {
            const Xcp_OdtEntryType* ent = odt->XcpOdtEntry+e;

            if(daq->XcpDaqListType == DAQ) {
                /* TODO - create a DAQ packet
            } else {
                /* TODO - read dts for each entry and update memory */
            }
        }
    }
}

/* Process all entries in event channel */
static void Xcp_ProcessChannel(const Xcp_EventChannelType* ech)
{
#ifdef XCP_STANDALONE
    printf("Processing Channel %d\n",  ech->XcpEventChannelNumber);
#endif

    for(int d = 0; d < ech->XcpEventChannelMaxDaqList; d++) {
        if(!ech->XcpEventChannelTriggeredDaqListRef[d])
            continue;
        Xcp_ProcessDaq(ech->XcpEventChannelTriggeredDaqListRef[d]);
    }
}


/* Scheduled function of the XCP module */
void Xcp_MainFunction(void)
{
    for(int c = 0; c < g_general.XcpMaxEventChannel; c++)
        Xcp_ProcessChannel(g_config->XcpEventChannel+c);

    for(int d = 0; d < g_general.XcpDaqCount; d++)
        Xcp_ProcessDaq(g_config->XcpDaqList+d);
}




