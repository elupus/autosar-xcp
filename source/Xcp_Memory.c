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

#include "Xcp_Internal.h"

#define XCP_DEBUG_MEMORY

#ifdef XCP_DEBUG_MEMORY
uint8_t g_XcpDebugMemory[1024];
#endif


static uint8* g_XcpMTA          = NULL;
static uint8  g_XcpMTAExtension = 0xFF;

/* function pointers that get set on mta init */
unsigned char (*Xcp_MtaGet)();
void          (*Xcp_MtaPut)(unsigned char val);
void          (*Xcp_MtaWrite)(uint8* data, int len);
void          (*Xcp_MtaRead) (uint8* data, int len);


/**
 * Read a character from MTA
 * @return
 */
static uint8 Xcp_MtaGetMemory()
{
    return *(g_XcpMTA++);
}

/**
 * Write a character to MTA
 * @param val
 */
static void Xcp_MtaPutMemory(uint8 val)
{
    *(g_XcpMTA++) = val;
}

/**
 * Generic function that writes character to mta using put
 * @param val
 */
static void Xcp_MtaWriteGeneric(uint8* data, int len)
{
    while(len-- > 0) {
        Xcp_MtaPut(*(data++));
    }
}

/**
 * Generic function that reads buffer from mta using get
 * @param val
 */
static void Xcp_MtaReadGeneric(uint8* data, int len)
{
    while(len-- > 0) {
        *(data++) = Xcp_MtaGet();
    }
}

/**
 * Set the MTA pointer to given address on given extension
 * @param address
 * @param extension
 */
void Xcp_MtaInit(intptr_t address, uint8 extension)
{
    g_XcpMTA     = (uint8*)address;

#ifdef XCP_DEBUG_MEMORY
    if(extension == 0xFF) {
        g_XcpMTA     = (uint8*)g_XcpDebugMemory + address;
    }
#endif

    g_XcpMTAExtension = extension;
    Xcp_MtaGet   = Xcp_MtaGetMemory;
    Xcp_MtaPut   = Xcp_MtaPutMemory;
    Xcp_MtaRead  = Xcp_MtaReadGeneric;
    Xcp_MtaWrite = Xcp_MtaWriteGeneric;
}

