/* Copyright (C) 2010 Joakim Plate, Peter Fridlund
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

#ifndef XCP_BYTESTREAM_H_
#define XCP_BYTESTREAM_H_
#include "Std_Types.h"
#include "Xcp_Internal.h"

#if XCP_STANDALONE
#include "XcpStandalone.h"
#else
#include "McuExtensions.h"
#endif

/* Access to values in a byte buffer.
 *
 * The buffers hold XCP packets where values start at any offset, so they
 * can not be read or written through a pointer to a wider type, that is
 * undefined behaviour and faults on targets that require alignment. The
 * value is instead moved one byte at a time through a local union.
 *
 * The byte order is the one of the host, the same as reported to the
 * master in the connect response.
 */

static inline uint8 Xcp_GetU8(const void* data, unsigned int offset)
{
    return *((const uint8*)data + offset);
}

static inline uint16 Xcp_GetU16(const void* data, unsigned int offset)
{
    const uint8* p = (const uint8*)data + offset;
    union { uint16 v; uint8 b[2]; } u;
    u.b[0] = p[0];
    u.b[1] = p[1];
    return u.v;
}

static inline uint32 Xcp_GetU32(const void* data, unsigned int offset)
{
    const uint8* p = (const uint8*)data + offset;
    union { uint32 v; uint8 b[4]; } u;
    u.b[0] = p[0];
    u.b[1] = p[1];
    u.b[2] = p[2];
    u.b[3] = p[3];
    return u.v;
}

static inline void Xcp_SetU8(void* data, unsigned int offset, uint8 value)
{
    *((uint8*)data + offset) = value;
}

static inline void Xcp_SetU16(void* data, unsigned int offset, uint16 value)
{
    uint8* p = (uint8*)data + offset;
    union { uint16 v; uint8 b[2]; } u;
    u.v  = value;
    p[0] = u.b[0];
    p[1] = u.b[1];
}

static inline void Xcp_SetU32(void* data, unsigned int offset, uint32 value)
{
    uint8* p = (uint8*)data + offset;
    union { uint32 v; uint8 b[4]; } u;
    u.v  = value;
    p[0] = u.b[0];
    p[1] = u.b[1];
    p[2] = u.b[2];
    p[3] = u.b[3];
}

/* RX/TX FIFO */

typedef struct Xcp_BufferType {
    unsigned int           len;
    unsigned char          data[XCP_MAX_DTO];
    struct Xcp_BufferType* next;
} Xcp_BufferType;

typedef struct Xcp_FifoType {
    Xcp_BufferType*        front;
    Xcp_BufferType*        back;
    struct Xcp_FifoType*   free;
    void*                  lock;
} Xcp_FifoType;

static inline void Xcp_Fifo_Lock(Xcp_FifoType* q)
{
#if XCP_STANDALONE
    XcpStandaloneLock();
#else
    q->lock = (void*)McuE_EnterCriticalSection();
#endif
}

static inline void Xcp_Fifo_Unlock(Xcp_FifoType* q)
{
#if XCP_STANDALONE
    XcpStandaloneUnlock();
#else
    McuE_ExitCriticalSection((imask_t)q->lock);
#endif
}

static inline Xcp_BufferType* Xcp_Fifo_Get(Xcp_FifoType* q)
{
    Xcp_Fifo_Lock(q);
    Xcp_BufferType* b = q->front;
    if(b == NULL) {
        Xcp_Fifo_Unlock(q);
        return NULL;
    }
    q->front = b->next;
    if(q->front == NULL)
        q->back = NULL;
    b->next = NULL;
    Xcp_Fifo_Unlock(q);
    return b;
}

static inline void Xcp_Fifo_Put(Xcp_FifoType* q, Xcp_BufferType* b)
{
    Xcp_Fifo_Lock(q);
    b->next = NULL;
    if(q->back)
        q->back->next = b;
    else
        q->front = b;
    q->back = b;
    Xcp_Fifo_Unlock(q);
}

static inline void Xcp_Fifo_Put_Front(Xcp_FifoType* q, Xcp_BufferType* b)
{
    Xcp_Fifo_Lock(q);
    b->next = q->front;
    q->front = b;
    if(q->back == NULL)
        q->back = b;
    Xcp_Fifo_Unlock(q);
}

static inline void Xcp_Fifo_Free(Xcp_FifoType* q, Xcp_BufferType* b)
{
    if(b) {
        b->len = 0;
        Xcp_Fifo_Put(q->free, b);
    }
}

static inline void Xcp_Fifo_Init(Xcp_FifoType* q, Xcp_BufferType* b, Xcp_BufferType* e)
{
    q->front = NULL;
    q->back  = NULL;
    q->lock  = NULL;
    for(;b != e; b++)
        Xcp_Fifo_Put(q, b);
}

#define FIFO_GET_WRITE(fifo, it) \
    for(Xcp_BufferType* it = Xcp_Fifo_Get(fifo.free); it; Xcp_Fifo_Put(&fifo, it), it = NULL)

#define FIFO_FOR_READ(fifo, it) \
    for(Xcp_BufferType* it = Xcp_Fifo_Get(&fifo); it; Xcp_Fifo_Free(&fifo, it), it = Xcp_Fifo_Get(&fifo))

/* Append a value to the data of a buffer */

static inline void Xcp_Buffer_AddU8(Xcp_BufferType* buf, uint8 value)
{
    Xcp_SetU8(buf->data, buf->len, value);
    buf->len += 1;
}

static inline void Xcp_Buffer_AddU16(Xcp_BufferType* buf, uint16 value)
{
    Xcp_SetU16(buf->data, buf->len, value);
    buf->len += 2;
}

static inline void Xcp_Buffer_AddU32(Xcp_BufferType* buf, uint32 value)
{
    Xcp_SetU32(buf->data, buf->len, value);
    buf->len += 4;
}





#endif /* XCP_BYTESTREAM_H_ */
