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

typedef struct {
    unsigned int  len;
    unsigned char data[XCP_MAX_DTO];
} Xcp_BufferType;

typedef struct {
    Xcp_BufferType  b[2];
    Xcp_BufferType* w;
    Xcp_BufferType* r;
    Xcp_BufferType* e;
} Xcp_FifoType;

void Xcp_FifoInit(Xcp_FifoType *fifo)
{
    fifo->w = fifo->b;
    fifo->r = fifo->b;
    fifo->e = fifo->b+sizeof(fifo->b)/sizeof(fifo->b[0]);
}

Xcp_BufferType* Xcp_FifoNext(Xcp_FifoType *fifo, Xcp_BufferType* p)
{
    if(p+1 == fifo->e)
        return fifo->b;
    else
        return p+1;
}

Xcp_BufferType* Xcp_FifoWrite_Get(Xcp_FifoType *fifo)
{
    Xcp_BufferType* n = Xcp_FifoNext(fifo, fifo->w);
    if(n == fifo->r)
        return NULL;
    return fifo->w;
}

Xcp_BufferType* Xcp_FifoWrite_Next(Xcp_FifoType *fifo)
{
    fifo->w = Xcp_FifoNext(fifo, fifo->w);
    assert(fifo->w != fifo->r);
    return Xcp_FifoWrite_Get(fifo);
}

Xcp_BufferType* Xcp_FifoRead_Get(Xcp_FifoType *fifo)
{
    if(fifo->r == fifo->w)
        return NULL;
    return fifo->r;
}

Xcp_BufferType* Xcp_FifoRead_Next(Xcp_FifoType *fifo)
{
    assert(fifo->r != fifo->w);
    fifo->r = Xcp_FifoNext(fifo, fifo->r);
    return Xcp_FifoRead_Get(fifo);
}

static Xcp_FifoType g_XcpRxFifo;

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



/**
 * Scheduled function of the XCP module
 *
 * ServiceId: 0x04
 *
 */
void Xcp_MainFunction(void)
{
    for(int c = 0; c < g_general.XcpMaxEventChannel; c++)
        Xcp_ProcessChannel(g_XcpConfig->XcpEventChannel+c);

    for(int d = 0; d < g_general.XcpDaqCount; d++)
        Xcp_ProcessDaq(g_XcpConfig->XcpDaqList+d);

    Xcp_RxIndication_Main();
}


Std_ReturnType Xcp_CmdConnect(uint8 pid, void* data, int len)
{
    uint8 mode = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received connect mode %x\n", mode)
    return E_NOT_OK;
}


Std_ReturnType Xcp_CmdDisconnect(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received disconnect\n")
    return E_NOT_OK;
}

static Xcp_CmdListType Xcp_CmdList[256] = {
    [XCP_PID_CMD_STD_CONNECT]    = { .fun = Xcp_CmdConnect   , .len = 1 }
  , [XCP_PID_CMD_STD_DISCONNECT] = { .fun = Xcp_CmdDisconnect, .len = 0 }
};


void Xcp_RxIndication_Main()
{
    for(Xcp_BufferType* it = Xcp_FifoRead_Get(&g_XcpRxFifo); it; it = Xcp_FifoRead_Next(&g_XcpRxFifo)) {
        uint8 pid = GET_UINT8(it->data,0);
        Xcp_CmdListType* cmd = Xcp_CmdList+pid;
        if(cmd->fun) {
            if(cmd->len && it->len < cmd->len) {
                DEBUG(DEBUG_HIGH, "Xcp_RxIndication_Main - Len %d to short for %u", it->len, pid)
                return;
            }
            cmd->fun(pid, it->data+1, it->len-1);
        }
    }
}

void Xcp_RxIndication(void* data, int len)
{
    if(len > XCP_MAX_DTO) {
        DEBUG(DEBUG_HIGH, "Xcp_RxIndication - length %d too long\n", len)
        return;
    }

    Xcp_BufferType* it = Xcp_FifoWrite_Get(&g_XcpRxFifo);
    if(!it) {
        DEBUG(DEBUG_HIGH, "Xcp_RxIndication - no free write buffer\n")
        return;
    }

    memcpy(it->data, data, len);
    it->len = len;

    Xcp_FifoWrite_Next(&g_XcpRxFifo);
}


