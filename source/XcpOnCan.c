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
#include "XcpOnCan_Cfg.h"
#include "Xcp_Internal.h"
#include "ComStack_Types.h"

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

void Xcp_CanRxIndication(
        PduIdType    XcpRxPduId,
        PduInfoType* XcpRxPduPtr)
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

    if(XcpRxPduId == (XcpRxPduId)-1) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x03, XCP_E_INVALID_PDUID)
        return;
    }
#endif

    /* TODO - How should the callback here be?
     *        it should be general enough to
     *        support all types of Xcp interfaces */
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

void Xcp_CanTxConfirmation(
        PduIdType XcpRxPduId)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!g_XcpConfig) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x02, XCP_E_INV_POINTER)
        /* return E_NOT_OK */
        return;
    }
#endif

    /* TODO - mark message as transmitted and make place for a new one */
}

/**
 * Callback that is called before a PDU is transmitted
 *
 * This function is called by the lower layers (i.e. FlexRay Interface, TTCAN Interface
 * and Socket Adaptor or CDD) when an AUTOSAR XCP PDU shall be transmitted.
 * The function Xcp_<module>TriggerTransmit is called by the <bus> Interface for
 * requesting the I-PDU before transmission. Whether the function
 * Xcp_<module>TriggerTransmit is called or not is statically configured for each I-PDU
 * in the configuration.
 *
 * Can be called on interrupt level
 *
 * ServiceId: 0x05
 *
 * @param XcpTxPduId
 * @param PduInfoPtr
 * @return E_NOT_OK or E_OK
 */

Std_ReturnType Xcp_CanTriggerTransmit(
        PduIdType XcpTxPduId,
        PduInfoType* PduInfoPtr)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!g_XcpConfig) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x05, XCP_E_INV_POINTER)
        return E_NOT_OK;
    }
#endif
    return E_NOT_OK;
}

void XcpOnCan_MainFunction(void)
{
}
