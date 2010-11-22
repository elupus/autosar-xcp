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

Xcp_MtaType Xcp_Mta;

/**
 * Read a character from MTA
 * @return
 */
static uint8 Xcp_MtaGetMemory()
{
    return *(uint8*)(Xcp_Mta.address++);
}

/**
 * Write a character to MTA
 * @param val
 */
static void Xcp_MtaPutMemory(uint8 val)
{
    *(uint8*)(Xcp_Mta.address++) = val;
}

#ifdef XCP_FEATURE_PROGRAM

#endif

/**
 * Generic function that writes character to mta using put
 * @param val
 */
static void Xcp_MtaWriteGeneric(uint8* data, int len)
{
    while(len-- > 0) {
        Xcp_Mta.put(*(data++));    }
}

/**
 * Generic function that reads buffer from mta using get
 * @param val
 */
static void Xcp_MtaReadGeneric(uint8* data, int len)
{
    while(len-- > 0) {
        *(data++) = Xcp_Mta.get();
    }
}

/**
 * Set the MTA pointer to given address on given extension
 * @param address
 * @param extension
 */
void Xcp_MtaInit(intptr_t address, uint8 extension)
{
    Xcp_Mta.address   = address;
    Xcp_Mta.extension = extension;

#ifdef XCP_DEBUG_MEMORY
    if(extension == 0xFF) {
        Xcp_Mta.address = (intptr_t)g_XcpDebugMemory + address;
    }
#endif

    if(extension == 0x01) {
        Xcp_Mta.get   = Xcp_MtaGetMemory;
        Xcp_Mta.put   = NULL;
        Xcp_Mta.read  = Xcp_MtaReadGeneric;
        Xcp_Mta.write = NULL;
    } else {
        Xcp_Mta.get   = Xcp_MtaGetMemory;
        Xcp_Mta.put   = Xcp_MtaPutMemory;
        Xcp_Mta.read  = Xcp_MtaReadGeneric;
        Xcp_Mta.write = Xcp_MtaWriteGeneric;
    }
}

