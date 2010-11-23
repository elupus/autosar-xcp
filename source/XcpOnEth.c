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

#include "Xcp_Cfg.h"
#include "XcpOnEth_Cfg.h"
#include "Xcp_Internal.h"
#include "ComStack_Types.h"
#ifdef XCP_STANDALONE
Std_ReturnType SoAdIf_Transmit(PduIdType CanTxPduId, const PduInfoType *PduInfoPtr);
#else
#include "SoAdIf.h"
#endif

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
#if(XCP_DEV_ERROR_DETECT)
    if(!g_XcpConfig) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x03, XCP_E_NO_INIT)
        return;
    }

    if(!XcpRxPduPtr) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x03, XCP_E_INV_POINTER)
        return;
    }

    if(XcpRxPduId != CANIF_PDU_ID_XCP) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x03, XCP_E_INVALID_PDUID)
        return;
    }
#endif

    Xcp_RxIndication(XcpRxPduPtr->SduDataPtr, XcpRxPduPtr->SduLength);
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
#if(XCP_DEV_ERROR_DETECT)
    if(!g_XcpConfig) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x02, XCP_E_INV_POINTER)
        /* return E_NOT_OK */
        return;
    }
#endif

}

Std_ReturnType Xcp_Transmit(const void* data, int len)
{
    PduInfoType pdu;
    pdu.SduDataPtr = (uint8*)data;
    pdu.SduLength  = len;
    return SoAdIf_Transmit(CANIF_PDU_ID_XCP, &pdu);
}

void XcpOnEth_MainFunction(void)
{
}
