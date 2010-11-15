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

#ifndef XCP_BYTESTREAM_H_
#define XCP_BYTESTREAM_H_


#define GET_UINT8(data, offset)  (*((uint8* )(data)+(offset)))
#define GET_UINT16(data, offset) (*((uint16*)(data)+(offset)))
#define GET_UINT32(data, offset) (*((uint32*)(data)+(offset)))


#define SET_UINT8(data, offset, value) do {                 \
         ((uint8*)(data))[offset] = ((value) & 0xFF); \
        } while(0)
#define SET_UINT16(data, offset, value) do {                 \
         ((uint8*)(data))[offset+0] = (((value) >> 0) & 0xFF); \
         ((uint8*)(data))[offset+1] = (((value) >> 8) & 0xFF); \
        } while(0)
#define SET_UINT32(data, offset, value) do {                   \
         ((uint8*)(data))[offset+0] = (((value) >> 0 ) & 0xFF); \
         ((uint8*)(data))[offset+1] = (((value) >> 8 ) & 0xFF); \
         ((uint8*)(data))[offset+2] = (((value) >> 16) & 0xFF); \
         ((uint8*)(data))[offset+3] = (((value) >> 24) & 0xFF); \
        } while(0)

/* RX/TX FIFO */

typedef struct {
    unsigned int  len;
    unsigned char data[XCP_MAX_DTO];
} Xcp_BufferType;

typedef struct {
    Xcp_BufferType  b[XCP_MAX_RXTX_QUEUE+1];
    Xcp_BufferType* w;
    Xcp_BufferType* r;
    Xcp_BufferType* e;
} Xcp_FifoType;

static inline void Xcp_FifoInit(Xcp_FifoType *fifo)
{
    fifo->w = fifo->b;
    fifo->r = fifo->b;
    fifo->e = fifo->b+sizeof(fifo->b)/sizeof(fifo->b[0]);
}

static inline Xcp_BufferType* Xcp_FifoNext(Xcp_FifoType *fifo, Xcp_BufferType* p)
{
    if(p+1 == fifo->e)
        return fifo->b;
    else
        return p+1;
}

static inline Xcp_BufferType* Xcp_FifoWrite_Get(Xcp_FifoType *fifo)
{
    Xcp_BufferType* n = Xcp_FifoNext(fifo, fifo->w);
    if(n == fifo->r)
        return NULL;
    return fifo->w;
}

static inline Xcp_BufferType* Xcp_FifoWrite_Next(Xcp_FifoType *fifo)
{
    fifo->w = Xcp_FifoNext(fifo, fifo->w);
    return Xcp_FifoWrite_Get(fifo);
}

static inline Xcp_BufferType* Xcp_FifoRead_Get(Xcp_FifoType *fifo)
{
    if(fifo->r == fifo->w)
        return NULL;
    return fifo->r;
}

static inline Xcp_BufferType* Xcp_FifoRead_Next(Xcp_FifoType *fifo)
{
    fifo->r = Xcp_FifoNext(fifo, fifo->r);
    return Xcp_FifoRead_Get(fifo);
}

#define FIFO_GET_WRITE(fifo, it) \
    if(Xcp_FifoWrite_Get(&fifo) == NULL) { DEBUG(DEBUG_HIGH, "FIFO_GET_WRITE - no free buffer to write\n") } \
    for(Xcp_BufferType* it = Xcp_FifoWrite_Get(&fifo); it; it = NULL, Xcp_FifoWrite_Next(&fifo))

#define FIFO_FOR_READ(fifo, it) \
    for(Xcp_BufferType* it = Xcp_FifoRead_Get(&fifo); it; it = Xcp_FifoRead_Next(&fifo))



#endif /* XCP_BYTESTREAM_H_ */
