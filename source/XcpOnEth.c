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

#if(XCP_PROTOCOL != XCP_PROTOCOL_TCP && XCP_PROTOCOL != XCP_PROTOCOL_UDP)
#   error Invalid protocol defined for this source file
#endif

#include "XcpOnEth_Cfg.h"
#include "Xcp_Internal.h"
#include "ComStack_Types.h"
#include "SoAdIf.h"

static uint16_t Xcp_EthCtrRx = 0;
static uint16_t Xcp_EthCtrTx = 0;


/**
 * Receive callback from Eth network layer
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
void Xcp_SoAdRxIndication   (PduIdType XcpRxPduId, PduInfoType* XcpRxPduPtr)
{
    DET_VALIDATE_NRV(g_XcpConfig                   , 0x03, XCP_E_NOT_INITIALIZED);
    DET_VALIDATE_NRV(XcpRxPduPtr                   , 0x03, XCP_E_INV_POINTER);
    DET_VALIDATE_NRV(XcpRxPduId == CANIF_PDU_ID_XCP, 0x03, XCP_E_INVALID_PDUID);
    DET_VALIDATE_NRV(XcpRxPduPtr->SduLength > 4    , 0x03, XCP_E_INVALID_PDUID);

    uint16 ctr = (XcpRxPduPtr->SduDataPtr[3] << 8) | XcpRxPduPtr->SduDataPtr[2];
    if(Xcp_Connected && ctr && ctr != Xcp_EthCtrRx) {
        DEBUG(DEBUG_HIGH, "Xcp_SoAdRxIndication - ctr:%d differs from expected: %d\n", ctr, Xcp_EthCtrRx);
    }

    Xcp_EthCtrRx = ctr+1;
    Xcp_RxIndication(XcpRxPduPtr->SduDataPtr+4, XcpRxPduPtr->SduLength-4);
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
void Xcp_SoAdTxConfirmation (PduIdType XcpRxPduId)
{
    DET_VALIDATE_NRV(g_XcpConfig, 0x02, XCP_E_NOT_INITIALIZED);
}

/**
 * Called by core Xcp to transmit data
 * @param data
 * @param len
 * @return
 */
Std_ReturnType Xcp_Transmit(const void* data, int len)
{
    uint8 buf[len+4];
    PduInfoType pdu;
    pdu.SduDataPtr = buf;
    pdu.SduLength  = len+4;

    SET_UINT16(buf, 0, len);
    SET_UINT16(buf, 2, ++Xcp_EthCtrTx);
    memcpy(buf+4, data, len);

    return SoAdIf_Transmit(XCP_PDU_ID_TX, &pdu);
}

/**
 * Called when the core of xcp have received a transport layer command
 * @param pid
 * @param data
 * @param len
 * @return
 */
extern Std_ReturnType Xcp_CmdTransportLayer(uint8 pid, void* data, int len)
{
}
