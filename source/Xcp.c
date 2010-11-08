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
#include "Xcp.h"
#include "Xcp_Cfg.h"
#include "Xcp_Internal.h"


#ifdef XCP_STANDALONE
#   define USE_DEBUG_PRINTF
#   define USE_LDEBUG_PRINTF
#   include <stdio.h>
#   include <assert.h>
#   include <string.h>
#else
#   include "Os.h"
#   if(XCP_DEV_ERROR_DETECT)
#       include "Dem.h"
#   endif
#   include "ComStack_Types.h"
#endif

#include "debug.h"
#undef  DEBUG_LVL
#define DEBUG_LVL DEBUG_HIGH


static Xcp_GeneralType g_general = 
{
    .XcpMaxDaq          = XCP_MAX_DAQ
  , .XcpMaxEventChannel = XCP_MAX_EVENT_CHANNEL
  , .XcpMinDaq          = XCP_MIN_DAQ
  , .XcpDaqCount        = XCP_MIN_DAQ
  , .XcpMaxDto          = XCP_MAX_DTO
  , .XcpMaxCto          = XCP_MAX_CTO
};

static Xcp_FifoType   g_XcpRxFifo;
static Xcp_FifoType   g_XcpTxFifo;

static int            g_XcpConnected;

const Xcp_ConfigType *g_XcpConfig;

/**
 * Initializing function
 *
 * ServiceId: 0x00
 *
 * @param Xcp_ConfigPtr
 */

void Xcp_Init(const Xcp_ConfigType* Xcp_ConfigPtr)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!Xcp_ConfigPtr) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x00, XCP_E_INV_POINTER)
        return;
    }
#endif
    g_XcpConfig = Xcp_ConfigPtr;
    Xcp_FifoInit(&g_XcpRxFifo);
    Xcp_FifoInit(&g_XcpTxFifo);
}

void Xcp_RxIndication(void* data, int len)
{
    if(len > XCP_MAX_DTO) {
        DEBUG(DEBUG_HIGH, "Xcp_RxIndication - length %d too long\n", len)
        return;
    }

    FIFO_GET_WRITE(g_XcpRxFifo, it) {
        memcpy(it->data, data, len);
        it->len = len;
    }
}

/* Process all entriesin DAQ */
static void Xcp_ProcessDaq(const Xcp_DaqListType* daq)
{
    //DEBUG(DEBUG_HIGH, "Processing DAQ %d\n", daq->XcpDaqListNumber);

    for(int o = 0; 0 < daq->XcpOdtCount; o++) {
        const Xcp_OdtType* odt = daq->XcpOdt+o;
        for(int e = 0; e < odt->XcpOdtEntriesCount; e++) {
            const Xcp_OdtEntryType* ent = odt->XcpOdtEntry+e;

            if(daq->XcpDaqListType == DAQ) {
                /* TODO - create a DAQ packet */
            } else {
                /* TODO - read dts for each entry and update memory */
            }
        }
    }
}

/* Process all entries in event channel */
static void Xcp_ProcessChannel(const Xcp_EventChannelType* ech)
{
    //DEBUG(DEBUG_HIGH, "Processing Channel %d\n",  ech->XcpEventChannelNumber);

    for(int d = 0; d < ech->XcpEventChannelMaxDaqList; d++) {
        if(!ech->XcpEventChannelTriggeredDaqListRef[d])
            continue;
        Xcp_ProcessDaq(ech->XcpEventChannelTriggeredDaqListRef[d]);
    }
}




Std_ReturnType Xcp_CmdConnect(uint8 pid, void* data, int len)
{
    uint8 mode = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received connect mode %x\n", mode)
    g_XcpConnected = 1;

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1,  1 << 0 /* CAL/PAG */
                              | 1 << 2 /* DAQ     */
                              | 0 << 3 /* STIM    */
                              | 1 << 4 /* PGM     */);
        SET_UINT8 (e->data, 2,  0 << 0 /* BYTE ORDER */
                              | 0 << 1 /* ADDRESS_GRANULARITY */
                              | 1 << 6 /* SALVE_BLOCK_MODE    */
                              | 0 << 7 /* OPTIONAL */);
        SET_UINT8 (e->data, 3, XCP_MAX_CTO);
        SET_UINT16(e->data, 4, XCP_MAX_DTO);
        SET_UINT8 (e->data, 6, XCP_PROTOCOL_MAJOR_VERSION  << 4);
        SET_UINT8 (e->data, 7, XCP_TRANSPORT_MAJOR_VERSION << 4);
        e->len = 8;
    }

    return E_OK;
}

Std_ReturnType Xcp_CmdGetStatus(uint8 pid, void* data, int len)
{
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1,  0 << 0 /* STORE_CAL_REQ */
                              | 0 << 2 /* STORE_DAQ_REQ */
                              | 0 << 3 /* CLEAR_DAQ_REQ */
                              | 0 << 6 /* DAQ_RUNNING   */
                              | 0 << 7 /* RESUME */);
        SET_UINT8 (e->data, 2,  0 << 0 /* CAL/PGM */
                              | 0 << 2 /* DAQ     */
                              | 0 << 3 /* STIM    */
                              | 0 << 4 /* PGM     */); /* Content resource protection */
        SET_UINT16(e->data, 4, 0); /* Session configuration ID */
        e->len = 6;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdDisconnect(uint8 pid, void* data, int len)
{
    if(g_XcpConnected) {
        DEBUG(DEBUG_HIGH, "Received disconnect\n")
    } else {
        DEBUG(DEBUG_HIGH, "Invalid disconnect without connect\n")
    }
    g_XcpConnected = 0;
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        e->len = 1;
    }
    return E_OK;
}

static Xcp_CmdListType Xcp_CmdList[256] = {
    [XCP_PID_CMD_STD_CONNECT]    = { .fun = Xcp_CmdConnect   , .len = 1 }
  , [XCP_PID_CMD_STD_DISCONNECT] = { .fun = Xcp_CmdDisconnect, .len = 0 }
  , [XCP_PID_CMD_STD_GET_STATUS] = { .fun = Xcp_CmdGetStatus , .len = 0 }
};



void Xcp_Recieve_Main()
{
    FIFO_FOR_READ(g_XcpRxFifo, it) {
        uint8 pid = GET_UINT8(it->data,0);
        Xcp_CmdListType* cmd = Xcp_CmdList+pid;
        if(cmd->fun) {
            if(cmd->len && it->len < cmd->len) {
                DEBUG(DEBUG_HIGH, "Xcp_RxIndication_Main - Len %d to short for %u\n", it->len, pid)
                return;
            }
            cmd->fun(pid, it->data+1, it->len-1);
        } else {
            FIFO_GET_WRITE(g_XcpTxFifo, e) {
                SET_UINT8(e->data, 0, XCP_PID_ERR);
                SET_UINT8(e->data, 1, XCP_ERR_CMD_UNKNOWN);
                e->len = 2;
            }
        }
    }
}

void Xcp_Transmit_Main()
{
    FIFO_FOR_READ(g_XcpTxFifo, it) {
        if(Xcp_Transmit(it->data, it->len) != E_OK) {
            DEBUG(DEBUG_HIGH, "Xcp_Transmit_Main - failed to transmit\n")
        }
    }
}




/**
 * Scheduled function of the XCP module
 *
 * ServiceId: 0x04
 *
 */
void Xcp_MainFunction(void)
{
#if 0
    for(int c = 0; c < g_general.XcpMaxEventChannel; c++)
        Xcp_ProcessChannel(g_XcpConfig->XcpEventChannel+c);

    for(int d = 0; d < g_general.XcpDaqCount; d++)
        Xcp_ProcessDaq(g_XcpConfig->XcpDaqList+d);
#endif

    Xcp_Recieve_Main();
    Xcp_Transmit_Main();
}

