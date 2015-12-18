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

#include "Xcp.h"

#if(XCP_PROTOCOL != XCP_PROTOCOL_CAN)
#   error Invalid protocol defined for this source file
#endif

#include "Xcp_Internal.h"
#include "Xcp_ByteStream.h"
#include "XcpOnCan_Cfg.h"
#include "ComStack_Types.h"
#include "CanIf.h"

/**
 * Receive callback from CAN network layer
 *
 * This function is called by the lower layers (i.e. FlexRay Interface, TTCAN Interface
 * and Socket Adaptor or CDD) when an AUTOSAR XCP PDU has been received
 *
 * Reentrant for different XcpRxPduIds,
 * non reentrant for the same XcpRxPduId
 *
 * The function Xcp_<module>RxIndication might be called
 * by the Xcp module’s environment in an interrupt context
 *
 * ServiceId: 0x03
 *
 * @param XcpRxPduId  PDU-ID that has been received
 * @param XcpRxPduPtr Pointer to SDU (Buffer of received payload)
 */

void Xcp_CanIfRxIndication(
        PduIdType    XcpRxPduId,
        PduInfoType* XcpRxPduPtr)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!Xcp_Inited) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x03, XCP_E_NOT_INITIALIZED);
        return;
    }

    if(!XcpRxPduPtr) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x03, XCP_E_INV_POINTER);
        return;
    }

    if(XcpRxPduId != XCP_PDU_ID_RX && XcpRxPduId != XCP_PDU_ID_BROADCAST) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x03, XCP_E_INVALID_PDUID);
        return;
    }
#endif

    /* TODO - How should the callback here be?
     *        it should be general enough to
     *        support all types of Xcp interfaces */
    Xcp_RxIndication(XcpRxPduPtr->SduDataPtr, XcpRxPduPtr->SduLength);
}

/**
 * Wrapper callback function for use in AUTOSAR 3.0 specification
 *
 * @param XcpRxPduId PDU-ID that has been received
 * @param data       Data that has been received
 * @param len        Length of data pointed to by data
 * @param type       The CAN id of the received data
 */
void Xcp_CanIfRxSpecial(uint8 channel, PduIdType XcpRxPduId, const uint8 * data, uint8 len, Can_IdType type)
{
    PduInfoType info = {
            .SduDataPtr = (uint8*)data,
            .SduLength  = len,
    };
    Xcp_CanRxIndication(XcpRxPduId, &info);
}


/**
 * Callback for finished transmit of PDU
 *
 * This function is called by the lower layers (i.e. FlexRay Interface, TTCAN Interface
 * and Socket Adaptor or CDD) when an AUTOSAR XCP PDU has been transmitted
 *
 * Reentrant for different XcpTxPduIds, non reentrant for the same XcpTxPduId
 *
 * ServiceId: 0x02
 *
 * @param XcpRxPduId PDU-ID that has been transmitted
 */

void Xcp_CanIfTxConfirmation(
        PduIdType XcpTxPduId)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!Xcp_Inited) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x02, XCP_E_INV_POINTER);
        /* return E_NOT_OK */
        return;
    }
#endif

    /* TODO - mark message as transmitted and make place for a new one */
}

/**
 * Transport protocol agnostic transmit function called by Xcp core system
 *
 * @param data
 * @param len
 * @return
 */
Std_ReturnType Xcp_Transmit(const void* data, int len)
{
    PduInfoType pdu;
    pdu.SduDataPtr = (uint8*)data;
    pdu.SduLength  = len;
    return CanIf_Transmit(XCP_PDU_ID_TX, &pdu);
}


/**
 * Command that can be used for a master to discover
 * all connected XCP slaves on a CAN bus
 * @param data
 * @param len
 * @return
 */

#if(XCP_FEATURE_GET_SLAVE_ID == STD_ON)
static Std_ReturnType Xcp_CmdGetSlaveId(void* data, int len)
{
    char p[4];
    uint8 mode;

    if(len < 4) {
        RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Invalid length for get_slave_id %d", len);
    }

    p[0] = GET_UINT8(data, 0);
    p[1] = GET_UINT8(data, 1);
    p[2] = GET_UINT8(data, 2);
    p[3] = 0;
    mode = GET_UINT8(data, 3);

    if(strcmp(p, "XCP")) {
        RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Unknown get_id pattern %s", p);
    }

    if(mode == 0) {
        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8 (e, XCP_PID_RES);
            FIFO_ADD_U8 (e, p[0]);
            FIFO_ADD_U8 (e, p[1]);
            FIFO_ADD_U8 (e, p[2]);
            FIFO_ADD_U32(e, XCP_CAN_ID_RX);
        }
    } else if(mode == 1) {
        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8 (e, XCP_PID_RES);
            FIFO_ADD_U8 (e, ~p[0]);
            FIFO_ADD_U8 (e, ~p[1]);
            FIFO_ADD_U8 (e, ~p[2]);
            FIFO_ADD_U32(e, XCP_CAN_ID_RX);
        }
    } else {
        RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Invalid mode for get_slave_id: %u", mode);
    }

    RETURN_SUCCESS();
}
#endif //XCP_FEATURE_GET_SLAVE_ID

/**
 * Called when the core of xcp have received a transport layer command
 * @param pid
 * @param data
 * @param len
 * @return
 */
extern Std_ReturnType Xcp_CmdTransportLayer(uint8 pid, void* data, int len)
{
#if(XCP_FEATURE_GET_SLAVE_ID == STD_ON)
    uint8 id = GET_UINT8(data, 0);
#endif

    typedef enum {
        XCP_CAN_CMD_SET_DAQ_ID   = 0xFD,
        XCP_CAN_CMD_GET_DAQ_ID   = 0xFE,
        XCP_CAN_CMD_GET_SLAVE_ID = 0xFF,
    } Xcp_CanCmdType;

#if(XCP_FEATURE_GET_SLAVE_ID == STD_ON)
    if(id == XCP_CAN_CMD_GET_SLAVE_ID && len >= 5) {
        return Xcp_CmdGetSlaveId(data+1, len-1);
    }
#endif

    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Unknown transport cmd:%u, len:%u", id, len);
}
