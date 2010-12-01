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
#define GET_UINT16(data, offset) (*(uint16*)((uint8*)(data)+(offset)))
#define GET_UINT32(data, offset) (*(uint32*)((uint8*)(data)+(offset)))


#define SET_UINT8(data, offset, value) do {           \
         ((uint8*)(data))[offset] = ((value) & 0xFF); \
        } while(0)

#define SET_UINT16(data, offset, value) do {          \
         unsigned _v = value, _o = offset;            \
         void* _data = data;                          \
         ((uint8*)_data)[_o+0] = ((_v >> 0) & 0xFF);  \
         ((uint8*)_data)[_o+1] = ((_v >> 8) & 0xFF);  \
        } while(0)

#define SET_UINT32(data, offset, value) do {          \
         unsigned _v = value, _o = offset;            \
         void* _data = data;                          \
         ((uint8*)_data)[_o+0] = ((_v >> 0 ) & 0xFF); \
         ((uint8*)_data)[_o+1] = ((_v >> 8 ) & 0xFF); \
         ((uint8*)_data)[_o+2] = ((_v >> 16) & 0xFF); \
         ((uint8*)_data)[_o+3] = ((_v >> 24) & 0xFF); \
        } while(0)

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
} Xcp_FifoType;

static inline Xcp_BufferType* Xcp_Fifo_Get(Xcp_FifoType* q)
{
    Xcp_BufferType* b = q->front;
    if(b == NULL)
        return NULL;
    q->front = b->next;
    if(q->front == NULL)
        q->back = NULL;
    b->next = NULL;
    return b;
}

static inline void Xcp_Fifo_Put(Xcp_FifoType* q, Xcp_BufferType* b)
{
    b->next = NULL;
    if(q->back)
        q->back->next = b;
    else
        q->front = b;
    q->back = b;
}

static inline void Xcp_Fifo_Init(Xcp_FifoType* q, Xcp_BufferType* b, Xcp_BufferType* e)
{
    q->front = NULL;
    q->back  = NULL;
    for(;b != e; b++)
        Xcp_Fifo_Put(q, b);
}

#define FIFO_GET_WRITE(fifo, it) \
    for(Xcp_BufferType* it = Xcp_Fifo_Get(fifo.free); it; Xcp_Fifo_Put(&fifo, it), it = NULL)

#define FIFO_FOR_READ(fifo, it) \
    for(Xcp_BufferType* it = Xcp_Fifo_Get(&fifo); it; it->len = 0, Xcp_Fifo_Put(fifo.free, it), it = Xcp_Fifo_Get(&fifo))

#define FIFO_ADD_U8(fifo, value) \
    do { SET_UINT8(fifo->data, fifo->len, value); fifo->len+=1; } while(0)

#define FIFO_ADD_U16(fifo, value) \
    do { SET_UINT16(fifo->data, fifo->len, value); fifo->len+=2; } while(0)

#define FIFO_ADD_U32(fifo, value) \
    do { SET_UINT32(fifo->data, fifo->len, value); fifo->len+=4; } while(0)





#endif /* XCP_BYTESTREAM_H_ */
