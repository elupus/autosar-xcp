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

/**
 * Read a character from MTA
 * @return
 */
static uint8 Xcp_MtaGetMemory(Xcp_MtaType* mta)
{
    return *(uint8*)(mta->address++);
}

/**
 * Write a character to MTA
 * @param val
 */
static void Xcp_MtaPutMemory(Xcp_MtaType* mta, uint8 val)
{
    *(uint8*)(mta->address++) = val;
}

#ifdef XCP_FEATURE_PROGRAM

#endif

/**
 * Generic function that writes character to mta using put
 * @param val
 */
static void Xcp_MtaWriteGeneric(Xcp_MtaType* mta, uint8* data, int len)
{
    while(len-- > 0) {
        mta->put(mta, *(data++));    }
}

/**
 * Generic function that reads buffer from mta using get
 * @param val
 */
static void Xcp_MtaReadGeneric(Xcp_MtaType* mta, uint8* data, int len)
{
    while(len-- > 0) {
        *(data++) = mta->get(mta);
    }
}

/**
 * Set the MTA pointer to given address on given extension
 * @param address
 * @param extension
 */
void Xcp_MtaInit(Xcp_MtaType* mta, intptr_t address, uint8 extension)
{
    mta->address   = address;
    mta->extension = extension;

#ifdef XCP_DEBUG_MEMORY
    if(extension == 0xFF) {
        mta->address = (intptr_t)g_XcpDebugMemory + address;
    }
#endif

    if(extension == 0x01) {
        mta->get   = Xcp_MtaGetMemory;
        mta->put   = NULL;
        mta->read  = Xcp_MtaReadGeneric;
        mta->write = NULL;
    } else {
        mta->get   = Xcp_MtaGetMemory;
        mta->put   = Xcp_MtaPutMemory;
        mta->read  = Xcp_MtaReadGeneric;
        mta->write = Xcp_MtaWriteGeneric;
    }
}

