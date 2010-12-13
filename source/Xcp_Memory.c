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

#include "Xcp.h"
#include "Xcp_Internal.h"

#if(XCP_FEATURE_DIO == STD_ON)
#include "Dio.h"
#endif

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

#if(XCP_FEATURE_DIO == STD_ON)
/**
 * Read a character from DIO
 * @return
 */
static uint8 Xcp_MtaGetDio(Xcp_MtaType* mta)
{
    unsigned int offset = mta->address % sizeof(Dio_PortType);
    Dio_PortType port   = mta->address / sizeof(Dio_PortType);

    if(offset == 0) {
        mta->buffer = Dio_ReadPort(port);
    }
    mta->address++;
    return (mta->buffer >> offset * 8) & 0xFF;
}
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

    if(extension == XCP_MTA_EXTENSION_MEMORY) {
        mta->get   = Xcp_MtaGetMemory;
        mta->put   = Xcp_MtaPutMemory;
        mta->read  = Xcp_MtaReadGeneric;
        mta->write = Xcp_MtaWriteGeneric;
#ifdef XCP_DEBUG_MEMORY
    } else if(extension == XCP_MTA_EXTENSION_DEBUG) {
        mta->address = (intptr_t)g_XcpDebugMemory + address;
        mta->get   = Xcp_MtaGetMemory;
        mta->put   = Xcp_MtaPutMemory;
        mta->read  = Xcp_MtaReadGeneric;
        mta->write = Xcp_MtaWriteGeneric;
#endif
    } else if(extension == XCP_MTA_EXTENSION_FLASH) {
        mta->get   = Xcp_MtaGetMemory;
        mta->put   = NULL;
        mta->read  = Xcp_MtaReadGeneric;
        mta->write = NULL;
    } else {
        mta->get   = NULL;
        mta->put   = NULL;
        mta->read  = NULL;
        mta->write = NULL;
    }
}

