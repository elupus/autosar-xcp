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

#include "Std_Types.h"
#include "Xcp.h"
#include "Xcp_Cfg.h"
#include "Xcp_Internal.h"
#include "Xcp_ByteStream.h"
#include <string.h>
#include <stdlib.h>


Xcp_BufferType Xcp_Buffers[XCP_MAX_RXTX_QUEUE];
Xcp_FifoType   Xcp_FifoFree;
Xcp_FifoType   Xcp_FifoRx = { .free = &Xcp_FifoFree };
Xcp_FifoType   Xcp_FifoTx = { .free = &Xcp_FifoFree };

       int                 Xcp_Connected;
       int                 Xcp_Inited;

static Xcp_TransferType    Xcp_Download;
static Xcp_DaqPtrStateType Xcp_DaqState;
static Xcp_TransferType    Xcp_Upload;
static Xcp_CmdWorkType     Xcp_Worker;

#if(XCP_FEATURE_PROTECTION)
static Xcp_UnlockType      Xcp_Unlock;
#endif

       Xcp_MtaType         Xcp_Mta;
       Xcp_ConfigType      Xcp_Config;
const  Xcp_ConfigType*     Xcp_ConfigOriginal;

/**
 * Initializing function
 *
 * ServiceId: 0x00
 *
 * @param Xcp_ConfigPtr
 */

void Xcp_Init(const Xcp_ConfigType* Xcp_ConfigPtr)
{
    DET_VALIDATE_NRV(Xcp_ConfigPtr, 0x00, XCP_E_INV_POINTER);
#if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_BYTE)
    DET_VALIDATE_NRV(Xcp_ConfigPtr->XcpMaxDaq <= 255, 0x00, XCP_E_INIT_FAILED);
#elif(XCP_IDENTIFICATION == XCP_IDENTIFICATION_ABSOLUTE)
    DET_VALIDATE_NRV(Xcp_ConfigPtr->XcpMaxDaq * XCP_MAX_ODT_ENTRIES <= 251, 0x00, XCP_E_INIT_FAILED);
#endif
    Xcp_ConfigOriginal = Xcp_ConfigPtr;
    memcpy(&Xcp_Config, Xcp_ConfigPtr, sizeof(Xcp_Config));

    Xcp_Fifo_Init(&Xcp_FifoFree, Xcp_Buffers, Xcp_Buffers+sizeof(Xcp_Buffers)/sizeof(Xcp_Buffers[0]));

    if(Xcp_Config.XcpMaxDaq == 0) {
        Xcp_Config.XcpMaxDaq = Xcp_Config.XcpMinDaq;
    }

    unsigned pid = 0;

	for(int daqNr = 0; daqNr < Xcp_Config.XcpMaxDaq; daqNr++) {
	    Xcp_DaqListType* daq = Xcp_Config.XcpDaqList+daqNr;
	    daq->XcpDaqListNumber     = daqNr;
		if(daqNr == Xcp_Config.XcpMaxDaq -1) {
		    daq->XcpNextDaq       = NULL;
		} else {
		    daq->XcpNextDaq       = daq+1;
		}

		if(daqNr < Xcp_Config.XcpMinDaq)
		    daq->XcpParams.Properties |= XCP_DAQLIST_PROPERTY_PREDEFINED;

		for(int odtNr = 0; odtNr < daq->XcpMaxOdt; odtNr++) {
		    Xcp_OdtType* odt = daq->XcpOdt+odtNr;
            odt->XcpOdtNumber       = odtNr;
            if( odtNr == daq->XcpMaxOdt -1 ){
                odt->XcpNextOdt     = NULL;
            } else {
                odt->XcpNextOdt     = odt+1;
            }

            if(XCP_IDENTIFICATION != XCP_IDENTIFICATION_ABSOLUTE) {
                pid = 0;
            }
            odt->XcpOdtEntriesCount = odt->XcpMaxOdtEntries;
            odt->XcpOdt2DtoMapping.XcpDtoPid = pid++;

            for(int odtEntryNr = 0; odtEntryNr < odt->XcpMaxOdtEntries; odtEntryNr++){
                Xcp_OdtEntryType* ent = odt->XcpOdtEntry+odtEntryNr;
                ent->XcpOdtEntryNumber = odtEntryNr;
                if ( odtEntryNr == odt->XcpMaxOdtEntries-1 ){
                    ent->XcpNextOdtEntry = NULL;
                } else {
                    ent->XcpNextOdtEntry = ent+1;
                }
            }
        }

    }

    Xcp_Inited = 1;
}

/**
 * Function called from lower layers (CAN/Ethernet..) containing
 * a received XCP packet.
 *
 * Can be called in interrupt context.
 *
 * @param data
 * @param len
 */
void Xcp_RxIndication(const void* data, int len)
{
    if(len > XCP_MAX_DTO) {
        DEBUG(DEBUG_HIGH, "Xcp_RxIndication - length %d too long\n", len);
        return;
    }

    if(len == 0)
        return;

    FIFO_GET_WRITE(Xcp_FifoRx, it) {
        memcpy(it->data, data, len);
        it->len = len;
    }
}

static uint32 Xcp_GetTimeStamp()
{
#if(XCP_TIMESTAMP_SIZE)
    TickType counter;
    if(GetCounterValue(XCP_COUNTER_ID, &counter)) {
        counter = 0;
    }
#if(XCP_TIMESTAMP_SIZE == 1)
    return counter % 256;
#elif(XCP_TIMESTAMP_SIZE == 2)
    return counter % (256*256);
#else
    return counter;
#endif

#else
    return 0;
#endif
}

/* Process all entries in DAQ */
static void Xcp_ProcessDaq(Xcp_DaqListType* daq)
{
    if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_STIM) {
        int offset = 0;
        if       (XCP_IDENTIFICATION == XCP_IDENTIFICATION_ABSOLUTE) {
            offset = 1;
        } else if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_BYTE) {
            offset = 2;
        } else if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD) {
            offset = 3;
        } else if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD_ALIGNED) {
            offset = 4;
        }

        for(Xcp_OdtType* odt = daq->XcpOdt; odt ; odt = odt->XcpNextOdt) {
            if(odt->XcpStim == NULL) {
                continue;
            }

            uint8  len  = odt->XcpStim->len  - offset;
            uint8* data = odt->XcpStim->data + offset;

        	for(Xcp_OdtEntryType* ent = odt->XcpOdtEntry; ent; ent = ent->XcpNextOdtEntry) {
        	    if(len < ent->XcpOdtEntryLength) {
        	        break;
        	    }
        	    Xcp_MtaType mta;
        	    Xcp_MtaInit(&mta, ent->XcpOdtEntryAddress, ent->XcpOdtEntryExtension);
                Xcp_MtaWrite(&mta, data, ent->XcpOdtEntryLength);
                Xcp_MtaFlush(&mta);

        	    data += ent->XcpOdtEntryLength;
        	    len  -= ent->XcpOdtEntryLength;
        	}

        	Xcp_Fifo_Free(&Xcp_FifoRx, odt->XcpStim);
        	odt->XcpStim = NULL;
        }
        return;
	}

    uint32 ct = Xcp_GetTimeStamp();
    int    ts = daq->XcpParams.Mode & XCP_DAQLIST_MODE_TIMESTAMP;

    Xcp_OdtType* odt = daq->XcpOdt;
    for(int o = 0; o < daq->XcpOdtCount; o++) {
        if(!odt->XcpOdtEntriesValid)
            continue;

        FIFO_GET_WRITE(Xcp_FifoTx, e) {

            FIFO_ADD_U8 (e, odt->XcpOdt2DtoMapping.XcpDtoPid);

            if        (XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD) {
                FIFO_ADD_U16(e, daq->XcpDaqListNumber);
            } else if (XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD_ALIGNED) {
                FIFO_ADD_U8 (e, 0);  /* RESERVED */
                FIFO_ADD_U16(e, daq->XcpDaqListNumber);
            } else if (XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_BYTE) {
                FIFO_ADD_U8(e, daq->XcpDaqListNumber);
            }

            if(ts) {
                if     (XCP_TIMESTAMP_SIZE == 1)
                    FIFO_ADD_U8 (e, ct);
                else if(XCP_TIMESTAMP_SIZE == 2)
                    FIFO_ADD_U16(e, ct);
                else if(XCP_TIMESTAMP_SIZE == 4)
                    FIFO_ADD_U32(e, ct);
                ts = 0;
            }
            Xcp_OdtEntryType* ent = odt->XcpOdtEntry;
            for(int i = 0; i < odt->XcpOdtEntriesCount; i++) {
                uint8  len = ent->XcpOdtEntryLength;
                Xcp_MtaType mta;
                Xcp_MtaInit(&mta, ent->XcpOdtEntryAddress, ent->XcpOdtEntryExtension);
                if(len + e->len > XCP_MAX_DTO)
                    break;

                Xcp_MtaRead(&mta, e->data+e->len, len);
                e->len += len;
                ent = ent->XcpNextOdtEntry;
            }
            odt = odt->XcpNextOdt;
        }
    }
}

/* Process all entries in event channel */
static void Xcp_ProcessChannel(Xcp_EventChannelType* ech)
{
    for(int d = 0; d < ech->XcpEventChannelDaqCount; d++) {
        Xcp_DaqListType* daq = ech->XcpEventChannelTriggeredDaqListRef[d];
        if(!daq)
            continue;

        if(!(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING))
            continue;

        if(!(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RESUME) && !Xcp_Connected)
            continue;

        if((ech->XcpEventChannelCounter % daq->XcpParams.Prescaler) != 0)
            continue;
        Xcp_ProcessDaq(ech->XcpEventChannelTriggeredDaqListRef[d]);
    }
    ech->XcpEventChannelCounter++;
}



/**
 * Xcp_TxError sends an error message back to master
 * @param code is the error code requested
 */
void Xcp_TxEvent(Xcp_EventType code)
{
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        SET_UINT8 (e->data, 0, XCP_PID_EV);
        SET_UINT8 (e->data, 1, code);
        e->len = 2;
    }
}

/**
 * Xcp_TxError sends an error message back to master
 * @param code is the error code requested
 */
void Xcp_TxError(Xcp_ErrorType code)
{
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        SET_UINT8 (e->data, 0, XCP_PID_ERR);
        SET_UINT8 (e->data, 1, code);
        e->len = 2;
    }
}

/**
 * Xcp_TxSuccess sends a basic RES response without
 * extra data to master
 */
void Xcp_TxSuccess()
{
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        e->len = 1;
    }
}


void Xcp_Disconnect()
{
    if(!Xcp_Inited)
        return;

    if(!Xcp_Connected)
        return;

    Xcp_MainFunction(); /* make sure nothing is buffered */
    Xcp_TxEvent(XCP_EV_SESSION_TERMINATED);
    Xcp_MainFunction(); /* make sure event is transmitted directly */

    Xcp_Connected = 0;
}

/**************************************************************************/
/**************************************************************************/
/**************************** GENERIC COMMANDS ****************************/
/**************************************************************************/
/**************************************************************************/

static Std_ReturnType Xcp_CmdConnect(uint8 pid, void* data, int len)
{
    uint8 mode = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received connect mode %x\n", mode);

    if(mode != 0) {
        RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_CmdConnect\n");
    }

#if(BYTE_ORDER == BIG_ENDIAN)
    int endian = 1;
#else
    int endian = 0;
#endif

    Xcp_Connected = 1;

    if(!Xcp_Connected) {
        /* restore varius state on a new connections */
        Xcp_Config.XcpProtect = Xcp_ConfigOriginal->XcpProtect;
    }

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        /* RESSOURCE */
        FIFO_ADD_U8 (e, (!!XCP_FEATURE_CALPAG) << 0 /* CAL/PAG */
                      | (!!XCP_FEATURE_DAQ)    << 2 /* DAQ     */
                      | (!!XCP_FEATURE_STIM)   << 3 /* STIM    */
                      | (!!XCP_FEATURE_PGM)    << 4 /* PGM     */);
        /* COMM_MODE_BASIC */
        FIFO_ADD_U8 (e, endian << 0 /* BYTE ORDER */
                      | 0 << 1 /* ADDRESS_GRANULARITY */
                      | (!!XCP_FEATURE_BLOCKMODE) << 6 /* SLAVE_BLOCK_MODE    */
                      | 0 << 7 /* OPTIONAL */);
        FIFO_ADD_U8 (e, XCP_MAX_CTO);
        FIFO_ADD_U16(e, XCP_MAX_DTO);
        FIFO_ADD_U8 (e, XCP_PROTOCOL_MAJOR_VERSION  << 4);
        FIFO_ADD_U8 (e, XCP_TRANSPORT_MAJOR_VERSION << 4);
    }

    return E_OK;
}

static Std_ReturnType Xcp_CmdGetStatus(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received get_status\n");

    /* find if any lists are running */
    int running = 0;
    for(Xcp_DaqListType *daq = Xcp_Config.XcpDaqList; daq; daq = daq->XcpNextDaq) {
        if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING) {
            running = 1;
            break;
        }
    }

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0 << 0 /* STORE_CAL_REQ */
                      | 0 << 2 /* STORE_DAQ_REQ */
                      | 0 << 3 /* CLEAR_DAQ_REQ */
                      | running << 6 /* DAQ_RUNNING */
                      | 0 << 7 /* RESUME */);
#if(XCP_FEATURE_PROTECTION)
        FIFO_ADD_U8 (e, Xcp_Config.XcpProtect); /* Content resource protection */
#else
        FIFO_ADD_U8 (e, 0);                     /* Content resource protection */
#endif
        FIFO_ADD_U8 (e, 0); /* Reserved */
        FIFO_ADD_U16(e, 0); /* Session configuration ID */
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdGetCommModeInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received get_comm_mode_info\n");
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0); /* Reserved */
        FIFO_ADD_U8 (e, (!!XCP_FEATURE_BLOCKMODE) << 0 /* MASTER_BLOCK_MODE */
                      | 0 << 1 /* INTERLEAVED_MODE  */);
        FIFO_ADD_U8 (e, 0); /* Reserved */
        FIFO_ADD_U8 (e, XCP_MAX_RXTX_QUEUE-1); /* MAX_BS */
        FIFO_ADD_U8 (e, 0); /* MIN_ST [100 microseconds] */
        FIFO_ADD_U8 (e, XCP_MAX_RXTX_QUEUE-1); /* QUEUE_SIZE */
        FIFO_ADD_U8 (e, XCP_PROTOCOL_MAJOR_VERSION << 4
                      | XCP_PROTOCOL_MINOR_VERSION); /* Xcp driver version */
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdGetId(uint8 pid, void* data, int len)
{
	uint8 idType = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received get_id %d\n", idType);

    const char* text = NULL;

	if(idType == 0 ) {
	    text = Xcp_Config.XcpInfo.XcpCaption;
	} else if(idType == 1) {
	    text = Xcp_Config.XcpInfo.XcpMC2File;
	} else if(idType == 2 ) {
        text = Xcp_Config.XcpInfo.XcpMC2Path;
	} else if(idType == 3 ) {
        text = Xcp_Config.XcpInfo.XcpMC2Url;
	} else if(idType == 4 ) {
        text = Xcp_Config.XcpInfo.XcpMC2Upload;
	}

	unsigned text_len = 0;
	if(text)
	    text_len = strlen(text);

	if(text_len + 8 < XCP_MAX_CTO) {
	    FIFO_GET_WRITE(Xcp_FifoTx, e) {
	        FIFO_ADD_U8  (e, XCP_PID_RES);
            FIFO_ADD_U8  (e, 1);        /* Mode */
	        FIFO_ADD_U16 (e, 0);        /* Reserved */
	        FIFO_ADD_U32 (e, text_len); /* Length */
	        if(text) {
                Xcp_MtaType mta;
                Xcp_MtaInit(&mta, (intptr_t)text, XCP_MTA_EXTENSION_MEMORY);
                Xcp_MtaRead(&mta, e->data+e->len, text_len);
                e->len += text_len;
	        }
	    }
	} else {
        Xcp_MtaInit(&Xcp_Mta, (intptr_t)text, XCP_MTA_EXTENSION_MEMORY);
        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8  (e, XCP_PID_RES);
            FIFO_ADD_U8  (e, 0);        /* Mode */
            FIFO_ADD_U16 (e, 0);        /* Reserved */
            FIFO_ADD_U32 (e, text_len); /* Length */
        }
	}
    return E_OK;

}

static Std_ReturnType Xcp_CmdDisconnect(uint8 pid, void* data, int len)
{
    if(Xcp_Connected) {
        DEBUG(DEBUG_HIGH, "Received disconnect\n");
    } else {
        DEBUG(DEBUG_HIGH, "Invalid disconnect without connect\n");
    }
    Xcp_Connected = 0;
    RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdSync(uint8 pid, void* data, int len)
{
    RETURN_ERROR(XCP_ERR_CMD_SYNCH, "Xcp_CmdSync\n");
}

static Std_ReturnType Xcp_CmdUser(uint8 pid, void* data, int len)
{
    if(Xcp_Config.XcpUserFn) {
        return Xcp_Config.XcpUserFn((uint8*)data+1, len-1);
    } else {
        RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_CmdUser\n");
    }
}

/**************************************************************************/
/**************************************************************************/
/*********************** UPLOAD/DOWNLOAD COMMANDS *************************/
/**************************************************************************/
/**************************************************************************/

/**
 * Worker function for blockmode uploads
 *
 * This function will be called once every main function run and send off
 * a upload package, when done it will unregister itself from main process
 *
 */
static void Xcp_CmdUpload_Worker(void)
{
    unsigned len = Xcp_Upload.rem;
    unsigned off = XCP_ELEMENT_OFFSET(1);
    unsigned max = XCP_MAX_CTO - off - 1;

    if(len > max)
        len = max;

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        for(unsigned int i = 0; i < off; i++)
            SET_UINT8 (e->data, i+1, 0);
        for(unsigned int i = 0; i < len; i++)
            SET_UINT8 (e->data, i+1+off, Xcp_MtaGet(&Xcp_Mta));
        e->len = len+1+off;
    }
    Xcp_Upload.rem -= len;

    if(Xcp_Upload.rem == 0)
        Xcp_Worker = NULL;
}

static Std_ReturnType Xcp_CmdUpload(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received upload\n");
	Xcp_Upload.len = GET_UINT8(data, 0) * XCP_ELEMENT_SIZE;;
	Xcp_Upload.rem = Xcp_Upload.len;

#ifndef XCP_FEATURE_BLOCKMODE
	if(Xcp_Upload.len + 1 > XCP_MAX_CTO) {
	    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_CmdUpload - Block mode not supported\n");
	}
#endif

    Xcp_Worker = Xcp_CmdUpload_Worker;
    Xcp_Worker();
    return E_OK;
}

static Std_ReturnType Xcp_CmdShortUpload(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received short upload\n");
    uint8  count = GET_UINT8 (data, 0);
    uint8  ext   = GET_UINT8 (data, 2);
    uint32 addr  = GET_UINT32(data, 3);

    if(count > XCP_MAX_CTO - XCP_ELEMENT_SIZE) {
        RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Xcp_CmdShortUpload - Too long data requested\n");
    }

    Xcp_MtaInit(&Xcp_Mta, addr, ext);
    if(Xcp_Mta.read == NULL) {
        RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Xcp_CmdShortUpload - invalid memory address\n");
    }

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        if(XCP_ELEMENT_SIZE > 1)
            memset(e->data+1, 0, XCP_ELEMENT_SIZE - 1);
        Xcp_MtaRead(&Xcp_Mta, e->data + XCP_ELEMENT_SIZE, count);
        e->len = count + XCP_ELEMENT_SIZE;
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdSetMTA(uint8 pid, void* data, int len)
{
    int ext = GET_UINT8 (data, 2);
    int ptr = GET_UINT32(data, 3);
    DEBUG(DEBUG_HIGH, "Received set_mta 0x%x, %d\n", ptr, ext);

    Xcp_MtaInit(&Xcp_Mta, ptr, ext);
    RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdDownload(uint8 pid, void* data, int len)
{
    unsigned rem = GET_UINT8(data, 0) * XCP_ELEMENT_SIZE;
    unsigned off = XCP_ELEMENT_OFFSET(2) + 1;
    DEBUG(DEBUG_HIGH, "Received download %d, %d\n", pid, len);

    if(!Xcp_Mta.write) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Xcp_Download - Mta not inited\n");
    }

#if(!XCP_FEATURE_BLOCKMODE)
    if(rem + off > len) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Xcp_Download - Invalid length (%u, %u, %d)\n", rem, off, len);
    }
#endif

    if(pid == XCP_PID_CMD_CAL_DOWNLOAD) {
        Xcp_Download.len = rem;
        Xcp_Download.rem = rem;
    }

    /* check for sequence error */
    if(Xcp_Download.rem != rem) {
        DEBUG(DEBUG_HIGH, "Xcp_Download - Invalid next state (%u, %u)\n", rem, Xcp_Download.rem);
        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8 (e, XCP_PID_ERR);
            FIFO_ADD_U8 (e, XCP_ERR_SEQUENCE);
            FIFO_ADD_U8 (e, Xcp_Download.rem / XCP_ELEMENT_SIZE);
        }
        return E_OK;
    }

    /* write what we got this packet */
    if(rem > len - off) {
        rem = len - off;
    }

    Xcp_MtaWrite(&Xcp_Mta, (uint8*)data + off, rem);
    Xcp_Download.rem -= rem;

    if(Xcp_Download.rem)
        return E_OK;

    Xcp_MtaFlush(&Xcp_Mta);
    RETURN_SUCCESS();
}

static uint32 Xcp_CmdBuildChecksum_Add11(uint32 block)
{
    uint8 res  = 0;
    for(int i = 0; i < block; i++) {
        res += Xcp_MtaGet(&Xcp_Mta);
    }
    return res;
}

static Std_ReturnType Xcp_CmdBuildChecksum(uint8 pid, void* data, int len)
{
    uint32 block = GET_UINT32(data, 3);

    DEBUG(DEBUG_HIGH, "Received build_checksum %ul\n", (unsigned int)block);

    if(!Xcp_Mta.get) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Xcp_CmdBuildChecksum - Mta not inited\n");
    }

    uint8  type = XCP_CHECKSUM_ADD_11;
    uint32 res  = Xcp_CmdBuildChecksum_Add11(block);

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, type);
        FIFO_ADD_U8 (e, 0); /* reserved */
        FIFO_ADD_U8 (e, 0); /* reserved */
        FIFO_ADD_U32(e, res);
    }
    return E_OK;
}


/**************************************************************************/
/**************************************************************************/
/*************************** CAL/PAG COMMANDS *****************************/
/**************************************************************************/
/**************************************************************************/
#if(XCP_FEATURE_CALPAG)
#warning Online Calibration page switching is not well supported

static Std_ReturnType Xcp_CmdSetCalPage(uint8 pid, void* data, int len)
{
    unsigned int mode = GET_UINT8(data, 0);
    unsigned int segm = GET_UINT8(data, 1);
    unsigned int page = GET_UINT8(data, 2);
    DEBUG(DEBUG_HIGH, "Received SetCalPage(0x%x, %u, %u)\n", mode, segm, page);

    Xcp_SegmentType* begin = NULL, *end = NULL;
    if(mode & 0x80) {
        begin = Xcp_Config.XcpSegment;
        end   = begin + Xcp_Config.XcpMaxSegment;
    } else {
        if(segm >= Xcp_Config.XcpMaxSegment) {
            RETURN_ERROR(XCP_ERR_SEGMENT_NOT_VALID, "Xcp_CmdSetCalPage(0x%x, %u, %u) - invalid segment\n", mode, segm, page);
        }

        begin = Xcp_Config.XcpSegment+segm;
        end   = begin + 1;
    }

    for(Xcp_SegmentType* s = begin; s != end; s++) {
        if(page >= s->XcpMaxPage) {
            RETURN_ERROR(XCP_ERR_PAGE_NOT_VALID, "Xcp_CmdSetCalPage(0x%x, %u, %u) - invalid page\n", mode, s-Xcp_Config.XcpSegment, page);
        }

        if(mode & 0x01) {
            s->XcpPageEcu = page;
        }
        if(mode & 0x02) {
            s->XcpPageXcp = page;
        }
    }
    RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdGetCalPage(uint8 pid, void* data, int len)
{
    unsigned int mode = GET_UINT8(data, 0);
    unsigned int segm = GET_UINT8(data, 1);
    unsigned int page = 0;
    DEBUG(DEBUG_HIGH, "Received GetCalPage(0x%x, %u)\n", mode, segm);

    if(segm >= Xcp_Config.XcpMaxSegment) {
        RETURN_ERROR(XCP_ERR_SEGMENT_NOT_VALID, "Xcp_CmdGetCalPage(0x%x, %u, %u) - invalid segment\n", mode, segm, page);
    }

    if(mode == 0x01) {
        page = Xcp_Config.XcpSegment[segm].XcpPageEcu;
    } else if(mode == 0x02) {
        page = Xcp_Config.XcpSegment[segm].XcpPageXcp;
    } else {
        RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Xcp_CmdGetCalPage(0x%x, %u) - invalid mode\n", mode, segm);
    }

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0); /* reserved */
        FIFO_ADD_U8 (e, 0); /* reserved */
        FIFO_ADD_U8 (e, page);
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdGetPagProcessorInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetPagProcessorInfo\n");
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, Xcp_Config.XcpMaxSegment);
        FIFO_ADD_U8 (e, 0 << 0 /* FREEZE_SUPPORTED */);
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdGetSegmentInfo(uint8 pid, void* data, int len)
{
    unsigned int mode = GET_UINT8(data, 0);
    unsigned int segm = GET_UINT8(data, 1);
    unsigned int info = GET_UINT8(data, 2);
    unsigned int mapi = GET_UINT8(data, 3);
    DEBUG(DEBUG_HIGH, "Received GetSegmentInfo(%u, %u, %u, %u)\n", mode, segm, info, mapi);

    if(segm >= Xcp_Config.XcpMaxSegment) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Invalid segment requested");
    }

    Xcp_SegmentType* seg = Xcp_Config.XcpSegment + segm;

    if(mode == 0) {
        uint32 data;

        if       (info == 0) {
            data = seg->XcpAddress;
        } else if(info == 1) {
            data = seg->XcpLength;
        } else {
            RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Unsupported");
        }

        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8 (e, XCP_PID_RES);
            FIFO_ADD_U8 (e, 0); /* reserved */
            FIFO_ADD_U8 (e, 0); /* reserved */
            FIFO_ADD_U8 (e, 0); /* reserved */
            FIFO_ADD_U32(e, data);
        }

    } else if (mode == 1) {

        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8 (e, XCP_PID_RES);
            FIFO_ADD_U8 (e, seg->XcpMaxPage);
            FIFO_ADD_U8 (e, seg->XcpExtension);
            FIFO_ADD_U8 (e, seg->XcpMaxMapping); /* MAX_MAPPING */
            FIFO_ADD_U8 (e, seg->XcpCompression);
            FIFO_ADD_U8 (e, seg->XcpEncryption);
        }

    } else if (mode == 2) {
        uint32 data;

        if(mapi >= seg->XcpMaxMapping) {
            RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Out or range mapping index");
        }
        Xcp_MemoryMappingType* map = seg->XcpMapping + mapi;

        if       (info == 0) {
            data = map->XcpSrc;
        } else if(info == 1) {
            data = map->XcpDst;
        } else if(info == 2) {
            data = map->XcpLen;
        } else {
            RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Unsupported");
        }

        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8 (e, XCP_PID_RES);
            FIFO_ADD_U8 (e, 0); /* reserved */
            FIFO_ADD_U8 (e, 0); /* reserved */
            FIFO_ADD_U8 (e, 0); /* reserved */
            FIFO_ADD_U32(e, data);
        }

    } else {
        RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Unsupported");
    }
    return E_OK;
}

#endif //XCP_FEATURE_CALPAG

/**************************************************************************/
/**************************************************************************/
/*************************** DAQ/STIM COMMANDS ****************************/
/**************************************************************************/
/**************************************************************************/

static Std_ReturnType Xcp_CmdClearDaqList(uint8 pid, void* data, int len)
{
    uint16 daqListNumber = GET_UINT16(data, 1);
    if(daqListNumber >= Xcp_Config.XcpMaxDaq || daqListNumber < Xcp_Config.XcpMinDaq )
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Daqlist number out of range\n");

    Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;
    for( int i = 0 ; i < daqListNumber ; i++ ) {
        daq = daq->XcpNextDaq;
    }

    if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
        RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");

    Xcp_OdtEntryType* entry;

    Xcp_OdtType* odt = daq->XcpOdt;
    for( int i = 0; i < daq->XcpOdtCount ;  i++ ) {
        odt->XcpOdtEntriesValid = 0;
        entry = odt->XcpOdtEntry;
        for(int j = 0; j < odt->XcpOdtEntriesCount ;  j++ ) {
            entry->XcpOdtEntryAddress   = 0;
            entry->XcpOdtEntryExtension = 0;
            entry->XcpOdtEntryLength    = 0;
            entry->BitOffSet            = 0xFF;
            entry = entry->XcpNextOdtEntry;
        }
        odt = odt->XcpNextOdt;
    }
	RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdSetDaqPtr(uint8 pid, void* data, int len)
{
	uint16 daqListNumber = GET_UINT16(data, 1);
    uint8 odtNumber      = GET_UINT8(data, 3);
    uint8 odtEntryNumber = GET_UINT8(data, 4);
    DEBUG(DEBUG_HIGH, "Received SetDaqPtr %u, %u, %u\n", daqListNumber, odtNumber, odtEntryNumber );

    Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;
    for( int i = 0 ; i < daqListNumber ; i++ ) {
        daq = daq->XcpNextDaq;
    }
    if(daqListNumber >= Xcp_Config.XcpMaxDaq)
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");

    if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
        RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");

    if(odtNumber >= daq->XcpMaxOdt)
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: odt number out of range (%u, %u)\n", odtNumber, daq->XcpMaxOdt);

    Xcp_OdtType* odt = daq->XcpOdt;
    for( int i = 0 ; i < odtNumber ; i ++ ) {
        odt = odt->XcpNextOdt;
    }

    Xcp_OdtEntryType* odtEntry = odt->XcpOdtEntry;
    for( int j = 0 ; j < odtEntryNumber ; j++ ) {
        odtEntry = odtEntry->XcpNextOdtEntry;
    }

    if(odtEntryNumber >= odt->XcpOdtEntriesCount)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: odt entry number out of range\n");

    Xcp_DaqState.daq = daq;
	Xcp_DaqState.odt = odt;
	Xcp_DaqState.ptr = odtEntry;

	RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdWriteDaq(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received WriteDaq\n");

	if(Xcp_DaqState.daq->XcpDaqListNumber < Xcp_Config.XcpMinDaq) /* Check if DAQ list is write protected */
	    RETURN_ERROR(XCP_ERR_WRITE_PROTECTED, "Error: DAQ-list is read only\n");

	if(Xcp_DaqState.ptr == NULL)
	    RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: No more ODT entries in this ODT\n");

	if(Xcp_DaqState.daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
	    RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");

	uint8 maxOdtEntrySize;
	uint8 granularityOdtEntrySize;

	if(Xcp_DaqState.daq->XcpParams.Mode & XCP_DAQLIST_MODE_STIM) /* Get DAQ list Direction */
	{
	    maxOdtEntrySize         = XCP_MAX_ODT_ENTRY_SIZE_STIM;
	    granularityOdtEntrySize = XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM;
	} else {
        maxOdtEntrySize         = XCP_MAX_ODT_ENTRY_SIZE_DAQ;
        granularityOdtEntrySize = XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ;
	}

	uint8 daqElemSize = GET_UINT8(data, 1);

	if( daqElemSize > maxOdtEntrySize )
	{
	    RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: DAQ list element size is invalid\n");
	}

    uint8 bitOffSet = GET_UINT8(data, 0);

    if( bitOffSet <= 0x1F)
    {
        if( daqElemSize == granularityOdtEntrySize )
        {
            Xcp_DaqState.ptr->BitOffSet  =  bitOffSet;
        } else
        {
            RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Element size and granularity don't match\n");
        }
    } else
    {
        Xcp_DaqState.ptr->BitOffSet  = 0xFF;
    }

	Xcp_DaqState.ptr->XcpOdtEntryExtension  = GET_UINT8(data, 2);
	Xcp_DaqState.ptr->XcpOdtEntryAddress    = GET_UINT32(data, 3);

	// Increment and decrement the count of valid odt entries
	if(daqElemSize && !Xcp_DaqState.ptr->XcpOdtEntryLength)
	    Xcp_DaqState.odt->XcpOdtEntriesValid++;
    if(!daqElemSize && Xcp_DaqState.ptr->XcpOdtEntryLength)
        Xcp_DaqState.odt->XcpOdtEntriesValid--;

	Xcp_DaqState.ptr->XcpOdtEntryLength  = daqElemSize;

	Xcp_DaqState.ptr = Xcp_DaqState.ptr->XcpNextOdtEntry;
	if(Xcp_DaqState.ptr == NULL){
	    Xcp_DaqState.daq = NULL;
	    Xcp_DaqState.odt = NULL;
	}

	RETURN_SUCCESS();
}

static void Xcp_CmdSetDaqListMode_EventChannel(Xcp_DaqListType* daq, uint16 newEventChannelNumber) {
	uint16 oldEventChannelNumber = daq->XcpParams.EventChannel;
	Xcp_EventChannelType* newEventChannel = Xcp_Config.XcpEventChannel+newEventChannelNumber;
	if(oldEventChannelNumber != 0xFFFF){
		Xcp_EventChannelType* oldEventChannel = Xcp_Config.XcpEventChannel+oldEventChannelNumber;
		for (int i = 0 ; i < oldEventChannel->XcpEventChannelDaqCount ; i++ ) {
			if( oldEventChannel->XcpEventChannelTriggeredDaqListRef[i] == daq) {
				oldEventChannel->XcpEventChannelTriggeredDaqListRef[i] = NULL;
				for( ; i < oldEventChannel->XcpEventChannelDaqCount - 1 ; i++) {
					oldEventChannel->XcpEventChannelTriggeredDaqListRef[i] =
							oldEventChannel->XcpEventChannelTriggeredDaqListRef[i + 1];
				}
				oldEventChannel->XcpEventChannelDaqCount--;
				break;
			}
		}
	}
    newEventChannel->XcpEventChannelTriggeredDaqListRef[newEventChannel->XcpEventChannelDaqCount] = daq;
    newEventChannel->XcpEventChannelDaqCount++;
    daq->XcpParams.EventChannel = newEventChannelNumber;
}

static Std_ReturnType Xcp_CmdSetDaqListMode(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received SetDaqListMode\n");
	uint16 list = GET_UINT16(data, 1);
	if(list >= Xcp_Config.XcpMaxDaq)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");

	uint8 prio = GET_UINT8(data, 6);
	if(prio)
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Priority %d of DAQ lists is not supported\n", prio);

	Xcp_DaqListType *daq = Xcp_Config.XcpDaqList;
	for( int i = 0 ; i < list ; i++ ){
	    daq = daq->XcpNextDaq;
	}

	if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
	    RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");

	Xcp_EventChannelType* newEventChannel = Xcp_Config.XcpEventChannel+GET_UINT16(data, 3);

	/* Check to see if the event channel supports the direction of the DAQ list.
	 * Can DAQ list be set to requested direction.
	 * Is the DAQ Predefined or Event_fixed
	 * */
	if(!(
		 (
		  (GET_UINT8(data, 0) & XCP_DAQLIST_MODE_STIM) 		   	&&
	      (daq->XcpParams.Properties & XCP_DAQLIST_PROPERTY_STIM) 	&&
	      (newEventChannel->XcpEventChannelProperties & XCP_EVENTCHANNEL_PROPERTY_STIM)
	     )
	     ||
	     (
	      (!(GET_UINT8(data, 0) & XCP_DAQLIST_MODE_STIM)) 		   	&&
	      (daq->XcpParams.Properties & XCP_DAQLIST_PROPERTY_DAQ) 	&&
	   	  (newEventChannel->XcpEventChannelProperties & XCP_EVENTCHANNEL_PROPERTY_DAQ)
	   	 )
	   	)
	  )
	{
		RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Error: direction not allowed.\n");
	}

	if(daq->XcpParams.Properties & XCP_DAQLIST_PROPERTY_PREDEFINED)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: DAQ list is Predefined\n");

	if((daq->XcpParams.Properties & XCP_DAQLIST_PROPERTY_EVENTFIXED) &&
	   (newEventChannel->XcpEventChannelNumber != daq->XcpParams.EventChannel)) {
				RETURN_ERROR(XCP_ERR_DAQ_CONFIG, "Error: DAQ list has a fixed event channel\n");
	}

	daq->XcpParams.Mode         = (GET_UINT8 (data, 0) & 0x32) | (daq->XcpParams.Mode & ~0x32);
	Xcp_CmdSetDaqListMode_EventChannel(daq,GET_UINT16(data, 3));
	daq->XcpParams.Prescaler	= GET_UINT8 (data, 5);
	daq->XcpParams.Priority		= prio;

	RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdGetDaqListMode(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqListMode\n");
	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= Xcp_Config.XcpMaxDaq) {
	    RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: DAQ list number out of range\n");
	}
	Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;
	for( int i = 0 ; i < daqListNumber ; i++ ){
	    daq = daq->XcpNextDaq;
	}

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, daq->XcpParams.Mode);         /* Mode */
        FIFO_ADD_U16(e, 0); 						  /* Reserved */
        FIFO_ADD_U16(e, daq->XcpParams.EventChannel); /* Current Event Channel Number */
        FIFO_ADD_U8 (e, daq->XcpParams.Prescaler);	  /* Current Prescaler */
        FIFO_ADD_U8 (e, daq->XcpParams.Priority);	  /* Current DAQ list Priority */
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdStartStopDaqList(uint8 pid, void* data, int len)
{
	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= Xcp_Config.XcpMaxDaq) {
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");
	}
	Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;
    for( int i = 0 ; i < daqListNumber ; i++ ){
        daq = daq->XcpNextDaq;
    }

	uint8 mode = GET_UINT8(data, 0);
	if ( mode == 0) {
	    /* STOP */
	    daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_RUNNING;
	} else if ( mode == 1) {
		/* START */
		daq->XcpParams.Mode |= XCP_DAQLIST_MODE_RUNNING;
	} else if ( mode == 2) {
		/* SELECT */
		daq->XcpParams.Mode |= XCP_DAQLIST_MODE_SELECTED;
	} else {
		RETURN_ERROR(XCP_ERR_MODE_NOT_VALID,"Error mode not valid\n");
	}

	FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8(e, XCP_PID_RES);
        FIFO_ADD_U8(e, daq->XcpOdt->XcpOdt2DtoMapping.XcpDtoPid);
    }
	return E_OK;
}

static Std_ReturnType Xcp_CmdStartStopSynch(uint8 pid, void* data, int len)
{
    uint8 mode = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received StartStopSynch %u\n", mode);
    Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;

    if ( mode == 0) {
        /* STOP ALL */
        for( int i = 0; i < Xcp_Config.XcpMaxDaq ; i++ ) {
            daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_RUNNING;
            daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_SELECTED;
            daq = daq->XcpNextDaq;
        }
    } else if ( mode == 1) {
        /* START SELECTED */
        for( int i = 0; i < Xcp_Config.XcpMaxDaq ; i++ ) {
            if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_SELECTED) {
                daq->XcpParams.Mode |=  XCP_DAQLIST_MODE_RUNNING;
                daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_SELECTED;
            }
            daq = daq->XcpNextDaq;
        }
    } else if ( mode == 2) {
        /* STOP SELECTED */
        for( int i = 0; i < Xcp_Config.XcpMaxDaq ; i++ ) {
        	if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_SELECTED) {
        		daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_RUNNING;
                daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_SELECTED;
            }
            daq = daq->XcpNextDaq;
        }
    } else {
        RETURN_ERROR(XCP_ERR_MODE_NOT_VALID,"Error mode not valid\n");
    }
    RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdGetDaqClock(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqClock\n");

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0); /* Alignment */
        FIFO_ADD_U8 (e, 0); /* Alignment */
        FIFO_ADD_U8 (e, 0); /* Alignment */
        FIFO_ADD_U32(e, Xcp_GetTimeStamp());
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdReadDaq(uint8 pid, void* data, int len)
{
    if(!Xcp_DaqState.ptr) {
        RETURN_ERROR(XCP_ERR_DAQ_CONFIG, "Error: No more ODT entries in this ODT\n");
    }
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, Xcp_DaqState.ptr->BitOffSet);
        FIFO_ADD_U8 (e, Xcp_DaqState.ptr->XcpOdtEntryLength);
        FIFO_ADD_U8 (e, Xcp_DaqState.ptr->XcpOdtEntryExtension);
        FIFO_ADD_U32(e, Xcp_DaqState.ptr->XcpOdtEntryAddress);
    }

    Xcp_DaqState.ptr = Xcp_DaqState.ptr->XcpNextOdtEntry;
    if(Xcp_DaqState.ptr == NULL){
        Xcp_DaqState.daq = NULL;
        Xcp_DaqState.odt = NULL;
    }

    return E_OK;
}

static Std_ReturnType Xcp_CmdGetDaqProcessorInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqProcessorInfo\n");
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, (XCP_FEATURE_DAQSTIM_DYNAMIC > 0 ? 1 : 0) << 0 /* DAQ_CONFIG_TYPE     */
                      | 1 << 1 /* PRESCALER_SUPPORTED */
                      | 0 << 2 /* RESUME_SUPPORTED    */
                      | 0 << 3 /* BIT_STIM_SUPPORTED  */
                      | (XCP_TIMESTAMP_SIZE > 0 ? 1 : 0) << 4 /* TIMESTAMP_SUPPORTED */
                      | 0 << 5 /* PID_OFF_SUPPORTED   */
                      | 0 << 6 /* OVERLOAD_MSB        */
                      | 0 << 7 /* OVERLOAD_EVENT      */);
        FIFO_ADD_U16(e, Xcp_Config.XcpMaxDaq);
        FIFO_ADD_U16(e, Xcp_Config.XcpMaxEventChannel);
        FIFO_ADD_U8 (e, Xcp_Config.XcpMinDaq);
        FIFO_ADD_U8 (e, 0 << 0 /* Optimisation_Type_0 */
                      | 0 << 1 /* Optimisation_Type_1 */
                      | 0 << 2 /* Optimisation_Type_2 */
                      | 0 << 3 /* Optimisation_Type_3 */
                      | 0 << 4 /* Address_Extension_ODT */
                      | 0 << 5 /* Address_Extension_DAQ */
                      | XCP_IDENTIFICATION << 6 /* Identification_Field_Type_0 and 1  */);
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdGetDaqResolutionInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqResolutionInfo\n");
    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ);  /* GRANULARITY_ODT_ENTRY_SIZE_DAQ */
        SET_UINT8 (e->data, 2, XCP_MAX_ODT_ENTRY_SIZE_DAQ); 		 /* MAX_ODT_ENTRY_SIZE_DAQ */
        SET_UINT8 (e->data, 3, XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM); /* GRANULARITY_ODT_ENTRY_SIZE_STIM */
        SET_UINT8 (e->data, 4, XCP_MAX_ODT_ENTRY_SIZE_STIM); 		 /* MAX_ODT_ENTRY_SIZE_STIM */
#if(XCP_TIMESTAMP_SIZE)
        SET_UINT8 (e->data, 5, XCP_TIMESTAMP_SIZE << 0  /* TIMESTAMP_SIZE  */
                             | 0                  << 3  /* TIMESTAMP_FIXED */
                             | XCP_TIMESTAMP_UNIT << 4  /* TIMESTAMP_UNIT  */);
        SET_UINT16(e->data, 6, 1); /* TIMESTAMP_TICKS */
#else
        SET_UINT8 (e->data, 5, 0); /* TIMESTAMP_MODE  */
        SET_UINT16(e->data, 6, 0); /* TIMESTAMP_TICKS */
#endif

        e->len = 8;
    }
    return E_OK;
}

static Std_ReturnType Xcp_CmdGetDaqListInfo(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received GetDaqListInfo\n");
	uint16 daqListNumber = GET_UINT16(data, 1);

	if(daqListNumber >= Xcp_Config.XcpMaxDaq)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Xcp_GetDaqListInfo list number out of range\n");


    Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;
    for( int i = 0 ; i < daqListNumber ; i++ ){
        daq = daq->XcpNextDaq;
    }

	FIFO_GET_WRITE(Xcp_FifoTx, e) {
		SET_UINT8  (e->data, 0, XCP_PID_RES);
		SET_UINT8  (e->data, 1, daq->XcpParams.Properties);
		SET_UINT8  (e->data, 2, daq->XcpMaxOdt); /* MAX_ODT */
		SET_UINT8  (e->data, 3, XCP_MAX_ODT_ENTRIES); /* MAX_ODT_ENTRIES */
		SET_UINT16 (e->data, 4, daq->XcpParams.EventChannel); /* FIXED_EVENT */
		e->len = 6;
	}
	return E_OK;
}

static Std_ReturnType Xcp_CmdGetDaqEventInfo(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received GetDaqEventInfo\n");
	uint16 eventChannelNumber = GET_UINT16(data, 1);

	if( eventChannelNumber >= Xcp_Config.XcpMaxEventChannel)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Xcp_CmdGetDaqEventInfo event channel number out of range\n");
	}

	const Xcp_EventChannelType* eventChannel = Xcp_Config.XcpEventChannel+eventChannelNumber;

	uint8 namelen = 0;
	if(eventChannel->XcpEventChannelName) {
	    namelen = strlen(eventChannel->XcpEventChannelName);
	    Xcp_MtaInit(&Xcp_Mta, (intptr_t)eventChannel->XcpEventChannelName, XCP_MTA_EXTENSION_MEMORY);
	}

	FIFO_GET_WRITE(Xcp_FifoTx, e) {
		SET_UINT8 (e->data, 0, XCP_PID_RES);
		SET_UINT8 (e->data, 1, eventChannel->XcpEventChannelProperties );
		SET_UINT8 (e->data, 2, eventChannel->XcpEventChannelMaxDaqList);
		SET_UINT8 (e->data, 3, namelen);                               /* Name length */
		SET_UINT8 (e->data, 4, eventChannel->XcpEventChannelRate);     /* Cycle time  */
		SET_UINT8 (e->data, 5, eventChannel->XcpEventChannelUnit);     /* Time unit   */
		SET_UINT8 (e->data, 6, eventChannel->XcpEventChannelPriority); /* Event channel priority */
		e->len = 7;
	}

	return E_OK;
}

#if(XCP_FEATURE_DAQSTIM_DYNAMIC)

static void Xcp_CmdFreeDaq_Helper(Xcp_DaqListType* daq){
    Xcp_OdtType *odt = daq->XcpOdt;
    Xcp_OdtType *tempOdt;
    for( int j = 0 ; j < daq->XcpOdtCount ; j++ ){
        Xcp_OdtEntryType *odtEntry = odt->XcpOdtEntry;
        Xcp_OdtEntryType *tempOdtEntry;
        for( int k = 0 ; k < odt->XcpOdtEntriesCount ; k++ ){
            tempOdtEntry = odtEntry->XcpNextOdtEntry;
            free(odtEntry);
            odtEntry = tempOdtEntry;
        }
        tempOdt = odt->XcpNextOdt;
        free(odt);
        odt = tempOdt;
    }
}

/**
 * Replaces the DAQ list pointer at given index
 * with the given value
 * @param next New daq list pointer
 * @param index Will replace the linked list at this position
 * @return Old value for the pointer
 */
static Xcp_DaqListType * Xcp_ReplaceDaqLink(uint8 index, Xcp_DaqListType * next)
{
    /* find first dynamic and last predefined */
    Xcp_DaqListType *first = Xcp_Config.XcpDaqList, *daq = NULL;
    for(int i = 0; i < index; i++) {
        daq   = first;
        first = first->XcpNextDaq;
    }

    if(daq) {
        daq->XcpNextDaq       = next;
    } else {
        Xcp_Config.XcpDaqList = next;
    }

    return first;
}

static Std_ReturnType Xcp_CmdFreeDaq(uint8 pid, void* data, int len)
{
    Xcp_DaqListType *first = Xcp_ReplaceDaqLink(Xcp_Config.XcpMinDaq, NULL);

    /* we now only have minimum number of daq lists */
    Xcp_Config.XcpMaxDaq = Xcp_Config.XcpMinDaq;

    for(Xcp_DaqListType *daq = first; daq; daq = daq->XcpNextDaq){
        Xcp_CmdFreeDaq_Helper(daq);
        if(daq->XcpParams.EventChannel != 0xFFFF) {
            Xcp_EventChannelType* eventChannel = Xcp_Config.XcpEventChannel+daq->XcpParams.EventChannel;
            for (int i = 0 ; i < eventChannel->XcpEventChannelDaqCount ; i++ ) {
                if( eventChannel->XcpEventChannelTriggeredDaqListRef[i] == daq) {
                    eventChannel->XcpEventChannelTriggeredDaqListRef[i] = NULL;
                    for( ; i < eventChannel->XcpEventChannelDaqCount - 1 ; i++) {
                        eventChannel->XcpEventChannelTriggeredDaqListRef[i] =
                                eventChannel->XcpEventChannelTriggeredDaqListRef[i + 1];
                    }
                    eventChannel->XcpEventChannelDaqCount--;
                    break;
                }
            }
        }
    }

    /* deallocate the contigous block of daq's allocated */
    if(first) {
        free(first);
    }

    Xcp_DaqState.dyn = XCP_DYNAMIC_STATE_FREE_DAQ;
    RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdAllocDaq(uint8 pid, void* data, int len)
{
    if(!(Xcp_DaqState.dyn == XCP_DYNAMIC_STATE_FREE_DAQ || Xcp_DaqState.dyn == XCP_DYNAMIC_STATE_ALLOC_DAQ)) {
        Xcp_DaqState.dyn = XCP_DYNAMIC_STATE_UNDEFINED;
        RETURN_ERROR(XCP_ERR_SEQUENCE," ");
    }
    uint16 nrDaqs = GET_UINT16(data, 1);

    Xcp_DaqListType *daq = (Xcp_DaqListType*)calloc(nrDaqs, sizeof(Xcp_DaqListType));
    if(daq == NULL){
        RETURN_ERROR(XCP_ERR_MEMORY_OVERFLOW,"Error, memory overflow");
    }

    Xcp_ReplaceDaqLink(Xcp_Config.XcpMinDaq, daq);
    Xcp_Config.XcpMaxDaq = Xcp_Config.XcpMinDaq + nrDaqs;

    for( uint16 i = Xcp_Config.XcpMinDaq ; i < Xcp_Config.XcpMaxDaq ; i++ ) {
        daq->XcpDaqListNumber     = i;
        daq->XcpParams.Mode       = 0;
        daq->XcpParams.Properties = XCP_DAQLIST_PROPERTY_DAQ
                                  | XCP_DAQLIST_PROPERTY_STIM;
        daq->XcpParams.Prescaler  = 1;
        daq->XcpParams.EventChannel = 0xFFFF; // Larger than allowed.
        daq->XcpOdtCount = 0;
        daq->XcpNextDaq = NULL;
        if( i > 0 ) {
            (daq-1)->XcpNextDaq = daq;
        }
        daq++;
    }
    Xcp_DaqState.dyn = XCP_DYNAMIC_STATE_ALLOC_DAQ;
    RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdAllocOdt(uint8 pid, void* data, int len)
{
	if(!(Xcp_DaqState.dyn == XCP_DYNAMIC_STATE_ALLOC_DAQ || Xcp_DaqState.dyn == XCP_DYNAMIC_STATE_ALLOC_ODT)) {
		Xcp_DaqState.dyn = XCP_DYNAMIC_STATE_UNDEFINED;
		RETURN_ERROR(XCP_ERR_SEQUENCE," ");
	}

	uint16 daqNr  = GET_UINT16(data, 1);
    uint8 nrOdts  = GET_UINT8(data, 3);

    if(daqNr >= Xcp_Config.XcpMaxDaq || daqNr < Xcp_Config.XcpMinDaq) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Requested allocation to predefined daq list %u", daqNr);
    }

    Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;
    for( int i = 0 ; i < daqNr ; i++ ){
        daq = daq->XcpNextDaq;
    }

    Xcp_OdtType* odt;
    Xcp_OdtType *newOdt;
    newOdt = (Xcp_OdtType*)malloc(sizeof(Xcp_OdtType));
    if(newOdt == NULL){
        RETURN_ERROR(XCP_ERR_MEMORY_OVERFLOW,"Error, memory overflow");
    }
    newOdt->XcpOdtNumber = 0;
    newOdt->XcpOdtEntriesCount = 0;
    newOdt->XcpOdtEntriesValid = 0;
    newOdt->XcpOdt2DtoMapping.XcpDtoPid = 0;
    newOdt->XcpStim = NULL;
    newOdt->XcpNextOdt = NULL;

    daq->XcpOdt = newOdt;
    odt         = newOdt;

    for( uint8 i = 1 ; i < nrOdts ; i++ ){
        newOdt = (Xcp_OdtType*)malloc(sizeof(Xcp_OdtType));
        if(newOdt == 0){
            RETURN_ERROR(XCP_ERR_MEMORY_OVERFLOW,"Error, memory overflow");
        }
        newOdt->XcpOdtNumber = i;
        newOdt->XcpOdtEntriesCount = 0;
        newOdt->XcpOdtEntriesValid = 0;
        newOdt->XcpStim = NULL;
        newOdt->XcpNextOdt = NULL;
        odt->XcpNextOdt = newOdt;
        odt = newOdt;
        odt->XcpOdt2DtoMapping.XcpDtoPid = i;
    }
    daq->XcpOdtCount = nrOdts;
    daq->XcpMaxOdt   = nrOdts;
    Xcp_DaqState.dyn = XCP_DYNAMIC_STATE_ALLOC_ODT;
    RETURN_SUCCESS();
}

static Std_ReturnType Xcp_CmdAllocOdtEntry(uint8 pid, void* data, int len)
{
	if(!(Xcp_DaqState.dyn == XCP_DYNAMIC_STATE_ALLOC_ODT || Xcp_DaqState.dyn == XCP_DYNAMIC_STATE_ALLOC_ODT_ENTRY)) {
		Xcp_DaqState.dyn = XCP_DYNAMIC_STATE_UNDEFINED;
		RETURN_ERROR(XCP_ERR_SEQUENCE," ");
	}

    uint16 daqNr = GET_UINT16(data, 1);
    uint8 odtNr  = GET_UINT8 (data, 3);
    uint8 odtEntriesCount = GET_UINT8 (data, 4);

    if(daqNr >= Xcp_Config.XcpMaxDaq || daqNr < Xcp_Config.XcpMinDaq) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Requested allocation to predefined daq list %u", daqNr);
    }

    Xcp_DaqListType* daq = Xcp_Config.XcpDaqList;
    for( int i = 0 ; i < daqNr ; i++ ){
        daq = daq->XcpNextDaq;
    }

    if(odtNr >= daq->XcpMaxOdt) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Requested allocation to invalid odt for daq %u, odt %u", daqNr, odtNr);
    }

    Xcp_OdtType* odt = daq->XcpOdt;
    for(int i = 0 ; i < odtNr ; i++ ) {
        odt = odt->XcpNextOdt;
    }
    odt->XcpOdtEntriesCount = odtEntriesCount;
    Xcp_OdtEntryType *newOdtEntry;


    newOdtEntry = (Xcp_OdtEntryType*)malloc(sizeof(Xcp_OdtEntryType));
    if(newOdtEntry == 0){
        RETURN_ERROR(XCP_ERR_MEMORY_OVERFLOW,"Error, memory overflow");
    }
    newOdtEntry->XcpOdtEntryNumber = 0;
    newOdtEntry->XcpNextOdtEntry = NULL;
    Xcp_OdtEntryType *odtEntry = newOdtEntry;
    odt->XcpOdtEntry = newOdtEntry;
    for( uint8 i = 1 ; i < odtEntriesCount ; i++ ){
        newOdtEntry = (Xcp_OdtEntryType*)malloc(sizeof(Xcp_OdtEntryType));
        if(newOdtEntry == 0){
            RETURN_ERROR(XCP_ERR_MEMORY_OVERFLOW,"Error, memory overflow");
        }
        newOdtEntry->XcpOdtEntryNumber = i;
        newOdtEntry->XcpNextOdtEntry = NULL;
        odtEntry->XcpNextOdtEntry = newOdtEntry;
        odtEntry = newOdtEntry;
    }
    odt->XcpOdtEntriesCount = odtEntriesCount;
    odt->XcpOdtEntriesValid = odtEntriesCount;
    Xcp_DaqState.dyn = XCP_DYNAMIC_STATE_ALLOC_ODT_ENTRY;
    RETURN_SUCCESS();
}
#endif

/**
 * Helper function to find requested odt, given a daqlist number and odt number
 *
 * Function can handle absolute odt numbers given a daq list number of 0.
 *
 * @param daqNr Requested daq list.
 * @param odtNr Requested odt within daq list or following daq lists.
 * @param daq Returns found daq list. NULL if could not be found.
 * @param odt Returns found odt list. NULL if could not be found.
 */
static void Xcp_GetOdt(uint16 daqNr, uint8 odtNr, Xcp_DaqListType** daq, Xcp_OdtType** odt)
{
    *daq = Xcp_Config.XcpDaqList;
    *odt = NULL;

    for(int i = 0; i < daqNr && *daq; i++) {
        *daq = (*daq)->XcpNextDaq;
    }

    if(*daq == NULL)
        return;

    *odt = (*daq)->XcpOdt;
    for(int j = 0; j < odtNr && *odt; j++) {
        *odt = (*odt)->XcpNextOdt;
        if(*odt == NULL) {
            *odt = NULL;
            *daq = (*daq)->XcpNextDaq;
            *odt = *daq ? (*daq)->XcpOdt : NULL;
        }
    }
    return;
}

/**
 * Main processing function for stim packets
 *
 * Function will queue up received STIM packets on the odt they specify.
 *
 * @param pid Odt number this stim packet refer to.
 * @param it  Pointer to the receive buffer containing the data. This may be consumed.
 * @return E_OK, @param it have been consumed and are queued up on a odt for later processing
 *         E_NOT_OK, unable to queue STIM packet on requested odt
 */
static Std_ReturnType Xcp_Recieve_Stim(uint8 pid, Xcp_BufferType* it)
{
    uint16 daqNr = 0;
    if       (XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_BYTE) {
        daqNr = GET_UINT8(it->data, 1);
    } else if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD) {
        daqNr = GET_UINT16(it->data, 1);
    } else if(XCP_IDENTIFICATION == XCP_IDENTIFICATION_RELATIVE_WORD_ALIGNED) {
        daqNr = GET_UINT16(it->data, 2);
    }

    Xcp_DaqListType* daq;
    Xcp_OdtType*     odt;
    Xcp_GetOdt(daqNr, pid, &daq, &odt);
    if(!daq || !odt) {
        RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Unable to find daq: %u, odt:%u", daqNr, pid);
    }

    if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_STIM) {
        Xcp_Fifo_Free(&Xcp_FifoRx, odt->XcpStim);
        odt->XcpStim = it;
        RETURN_SUCCESS();
    }
    RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "daq: %u is not a STIM list", daqNr);
}

/**************************************************************************/
/**************************************************************************/
/****************************** SEED & KEY ********************************/
/**************************************************************************/
/**************************************************************************/
#if(XCP_FEATURE_PROTECTION)

static Std_ReturnType Xcp_CmdGetSeed(uint8 pid, void* data, int len)
{
    uint8 mode = GET_UINT8(data, 0);
    uint8 res  = GET_UINT8(data, 1);
    DEBUG(DEBUG_HIGH, "Received GetSeed(%u, %u)\n", mode, res);

    if(mode == 0) {
        if(res != XCP_PROTECT_CALPAG
        && res != XCP_PROTECT_DAQ
        && res != XCP_PROTECT_STIM
        && res != XCP_PROTECT_PGM) {
            RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Requested invalid resource");
        }

        Xcp_Unlock.res      = res;
        Xcp_Unlock.key_len  = 0;
        Xcp_Unlock.key_rem  = 0;

        Xcp_Unlock.seed_len = Xcp_Config.XcpSeedFn(res, Xcp_Unlock.seed);
        Xcp_Unlock.seed_rem = Xcp_Unlock.seed_len;
    } else if(mode == 1){
        if(Xcp_Unlock.res == XCP_PROTECT_NONE) {
            RETURN_ERROR(XCP_ERR_SEQUENCE, "Requested second part of seed before first");
        }
    } else {
        RETURN_ERROR(XCP_ERR_GENERIC, "Requested invalid mode");
    }

    uint8 rem;
    if(Xcp_Unlock.seed_rem > XCP_MAX_CTO - 2)
        rem = XCP_MAX_CTO - 2;
    else
        rem = Xcp_Unlock.seed_rem;

    FIFO_GET_WRITE(Xcp_FifoTx, e) {
        FIFO_ADD_U8(e, XCP_PID_RES);
        FIFO_ADD_U8(e, Xcp_Unlock.seed_rem);
        memcpy( e->data+e->len
              , Xcp_Unlock.seed + Xcp_Unlock.seed_len - Xcp_Unlock.seed_rem
              , rem);

        e->len              += rem;
        Xcp_Unlock.seed_rem -= rem;
    }

    return E_OK;
}

static Std_ReturnType Xcp_CmdUnlock(uint8 pid, void* data, int len)
{
    uint8 rem = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received Unlock(%u)\n", rem);

    if(Xcp_Unlock.res == XCP_PROTECT_NONE) {
        RETURN_ERROR(XCP_ERR_SEQUENCE, "Requested unlock without requesting a seed");
    }

    /* if this is first call, setup state */
    if(Xcp_Unlock.key_len == 0) {
        Xcp_Unlock.key_len = rem;
        Xcp_Unlock.key_rem = rem;
    }

    /* validate that we are in correct sync */
    if(Xcp_Unlock.key_rem != rem) {
        FIFO_GET_WRITE(Xcp_FifoTx, e) {
            FIFO_ADD_U8 (e, XCP_PID_ERR);
            FIFO_ADD_U8 (e, XCP_ERR_SEQUENCE);
            FIFO_ADD_U8 (e, Xcp_Unlock.key_rem);
        }
        return E_OK;
    }

    if(rem > len - 1)
        rem = len - 1;

    memcpy( Xcp_Unlock.key + Xcp_Unlock.key_len - Xcp_Unlock.key_rem
          , data+1
          , rem);


    Xcp_Unlock.key_rem -= rem;

    if(Xcp_Unlock.key_rem == 0) {
        if(Xcp_Config.XcpUnlockFn == NULL) {
            RETURN_ERROR(XCP_ERR_GENERIC, "No unlock function defines");
        }

        if(Xcp_Config.XcpUnlockFn( Xcp_Unlock.res
                                 , Xcp_Unlock.seed
                                 , Xcp_Unlock.seed_len
                                 , Xcp_Unlock.key
                                 , Xcp_Unlock.key_len) == E_OK) {
            Xcp_Config.XcpProtect &= ~Xcp_Unlock.res;
        } else {
            RETURN_ERROR(XCP_ERR_ACCESS_LOCKED, "Failed to unlock resource");
        }

    }
    RETURN_SUCCESS();
}
#endif

/**************************************************************************/
/**************************************************************************/
/*************************** COMMAND PROCESSOR ****************************/
/**************************************************************************/
/**************************************************************************/

/**
 * Structure holding a map between command codes and the function
 * implementing the command
 */
static Xcp_CmdListType Xcp_CmdList[256] = {
    [XCP_PID_CMD_STD_CONNECT]                 = { .fun = Xcp_CmdConnect             , .len = 1 }
  , [XCP_PID_CMD_STD_DISCONNECT]              = { .fun = Xcp_CmdDisconnect          , .len = 0 }
  , [XCP_PID_CMD_STD_GET_STATUS]              = { .fun = Xcp_CmdGetStatus           , .len = 0 }
  , [XCP_PID_CMD_STD_GET_ID]                  = { .fun = Xcp_CmdGetId               , .len = 1 }
  , [XCP_PID_CMD_STD_UPLOAD]                  = { .fun = Xcp_CmdUpload              , .len = 1 }
  , [XCP_PID_CMD_STD_SHORT_UPLOAD]            = { .fun = Xcp_CmdShortUpload         , .len = 8 }
  , [XCP_PID_CMD_STD_SET_MTA]                 = { .fun = Xcp_CmdSetMTA              , .len = 3 }
  , [XCP_PID_CMD_STD_SYNCH]                   = { .fun = Xcp_CmdSync                , .len = 0 }
  , [XCP_PID_CMD_STD_GET_COMM_MODE_INFO]      = { .fun = Xcp_CmdGetCommModeInfo     , .len = 0 }
  , [XCP_PID_CMD_STD_BUILD_CHECKSUM]          = { .fun = Xcp_CmdBuildChecksum       , .len = 8 }
  , [XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD]     = { .fun = Xcp_CmdTransportLayer      , .len = 1 }
  , [XCP_PID_CMD_STD_USER_CMD]                = { .fun = Xcp_CmdUser                , .len = 0 }

#if(XCP_FEATURE_PROTECTION)
  , [XCP_PID_CMD_STD_GET_SEED]                = { .fun = Xcp_CmdGetSeed             , .len = 0 }
  , [XCP_PID_CMD_STD_UNLOCK]                  = { .fun = Xcp_CmdUnlock              , .len = 3 }
#endif

#if(XCP_FEATURE_PGM)
  , [XCP_PID_CMD_PGM_GET_PGM_PROCESSOR_INFO]  = { .fun = Xcp_CmdProgramInfo         , .len = 0, .lock = XCP_PROTECT_PGM }
  , [XCP_PID_CMD_PGM_PROGRAM_START]           = { .fun = Xcp_CmdProgramStart        , .len = 0, .lock = XCP_PROTECT_PGM }
  , [XCP_PID_CMD_PGM_PROGRAM_CLEAR]           = { .fun = Xcp_CmdProgramClear        , .len = 8, .lock = XCP_PROTECT_PGM }
  , [XCP_PID_CMD_PGM_PROGRAM]                 = { .fun = Xcp_CmdProgram             , .len = 3, .lock = XCP_PROTECT_PGM }
#if(XCP_FEATURE_BLOCKMODE)
  , [XCP_PID_CMD_PGM_PROGRAM_NEXT]            = { .fun = Xcp_CmdProgram             , .len = 3, .lock = XCP_PROTECT_PGM }
#endif
  , [XCP_PID_CMD_PGM_PROGRAM_RESET]           = { .fun = Xcp_CmdProgramReset        , .len = 0, .lock = XCP_PROTECT_PGM }
#endif // XCP_FEATURE_PGM

#if(XCP_FEATURE_CALPAG)
  , [XCP_PID_CMD_PAG_SET_CAL_PAGE]            = { .fun = Xcp_CmdSetCalPage          , .len = 4, .lock = XCP_PROTECT_CALPAG }
  , [XCP_PID_CMD_PAG_GET_CAL_PAGE]            = { .fun = Xcp_CmdGetCalPage          , .len = 3, .lock = XCP_PROTECT_CALPAG }
  , [XCP_PID_CMD_PAG_GET_PAG_PROCESSOR_INFO]  = { .fun = Xcp_CmdGetPagProcessorInfo , .len = 0, .lock = XCP_PROTECT_CALPAG }
  , [XCP_PID_CMD_PAG_GET_SEGMENT_INFO]        = { .fun = Xcp_CmdGetSegmentInfo      , .len = 3, .lock = XCP_PROTECT_CALPAG }
#endif // XCP_FEATURE_CALPAG
  , [XCP_PID_CMD_CAL_DOWNLOAD]                = { .fun = Xcp_CmdDownload            , .len = 3, .lock = XCP_PROTECT_CALPAG }
#if(XCP_FEATURE_BLOCKMODE)
  , [XCP_PID_CMD_CAL_DOWNLOAD_NEXT]           = { .fun = Xcp_CmdDownload            , .len = 3, .lock = XCP_PROTECT_CALPAG }
#endif
  , [XCP_PID_CMD_DAQ_CLEAR_DAQ_LIST]          = { .fun = Xcp_CmdClearDaqList        , .len = 3, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_SET_DAQ_PTR]			  = { .fun = Xcp_CmdSetDaqPtr         	, .len = 5, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_WRITE_DAQ]               = { .fun = Xcp_CmdWriteDaq            , .len = 7, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_SET_DAQ_LIST_MODE]       = { .fun = Xcp_CmdSetDaqListMode      , .len = 7, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE]       = { .fun = Xcp_CmdGetDaqListMode      , .len = 3, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST]     = { .fun = Xcp_CmdStartStopDaqList    , .len = 3, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_START_STOP_SYNCH]        = { .fun = Xcp_CmdStartStopSynch      , .len = 1, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_GET_DAQ_CLOCK]           = { .fun = Xcp_CmdGetDaqClock         , .len = 0, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_READ_DAQ]                = { .fun = Xcp_CmdReadDaq             , .len = 0, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO]  = { .fun = Xcp_CmdGetDaqProcessorInfo , .len = 0, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_GET_DAQ_RESOLUTION_INFO] = { .fun = Xcp_CmdGetDaqResolutionInfo, .len = 0, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO]       = { .fun = Xcp_CmdGetDaqListInfo      , .len = 3, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO]      = { .fun = Xcp_CmdGetDaqEventInfo     , .len = 3, .lock = XCP_PROTECT_DAQ }
#if(XCP_FEATURE_DAQSTIM_DYNAMIC)
  , [XCP_PID_CMD_DAQ_FREE_DAQ]                = { .fun = Xcp_CmdFreeDaq             , .len = 0, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_ALLOC_DAQ]               = { .fun = Xcp_CmdAllocDaq            , .len = 3, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_ALLOC_ODT]               = { .fun = Xcp_CmdAllocOdt            , .len = 4, .lock = XCP_PROTECT_DAQ }
  , [XCP_PID_CMD_DAQ_ALLOC_ODT_ENTRY]         = { .fun = Xcp_CmdAllocOdtEntry       , .len = 5, .lock = XCP_PROTECT_DAQ }
#endif
};

/**
 * Xcp_Recieve_Main is the main process that executes all received commands.
 *
 * The function queues up replies for transmission. Which will be sent
 * when Xcp_Transmit_Main function is called.
 */
void Xcp_Recieve_Main()
{
    FIFO_FOR_READ(Xcp_FifoRx, it) {
        uint8 pid = GET_UINT8(it->data,0);

        /* ignore commands when we are not connected */
        if(!Xcp_Connected && pid != XCP_PID_CMD_STD_CONNECT
                          && pid != XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD) {
            continue;
        }

        /* process stim commands */
        if(pid <= XCP_PID_CMD_STIM_LAST){

#if(XCP_FEATURE_PROTECTION)
            if(Xcp_Config.XcpProtect & XCP_PROTECT_STIM) {
                Xcp_TxError(XCP_ERR_ACCESS_LOCKED);
                continue;
            }
#endif

            if(Xcp_Recieve_Stim(pid, it) == E_OK) {
                it = NULL;
            }
            continue;
        }

        /* process standard commands */
        Xcp_CmdListType* cmd = Xcp_CmdList+pid;
        if(cmd->fun) {

#if(XCP_FEATURE_PROTECTION)
            if(cmd->lock & Xcp_Config.XcpProtect) {
                Xcp_TxError(XCP_ERR_ACCESS_LOCKED);
                continue;
            }
#endif

            if(cmd->len && it->len < cmd->len) {
                DEBUG(DEBUG_HIGH, "Xcp_RxIndication_Main - Len %d to short for %u\n", it->len, pid);
                return;
            }
            cmd->fun(pid, it->data+1, it->len-1);
        } else {
            Xcp_TxError(XCP_ERR_CMD_UNKNOWN);
        }
    }
}


/**
 * Xcp_Transmit_Main transmits queued up replies
 */
void Xcp_Transmit_Main()
{
    Xcp_BufferType* item = Xcp_Fifo_Get(&Xcp_FifoTx);
    while(item) {
        if(Xcp_Transmit(item->data, item->len) != E_OK) {
            Xcp_Fifo_Put_Front(&Xcp_FifoTx, item);
            break;
        } else {
            Xcp_Fifo_Free(&Xcp_FifoTx, item);
#if(XCP_FEATURE_TRANSMIT_FAST == STD_OFF)
            /* transmit maximum one frame */
            break;
#endif
        }
        item = Xcp_Fifo_Get(&Xcp_FifoTx);
    }
}

/**
 * Scheduled function of the event channel
 * @param channel
 */
void Xcp_MainFunction_Channel(unsigned channel)
{
    DET_VALIDATE_NRV(Xcp_Inited, 0x04, XCP_E_NOT_INITIALIZED);
    Xcp_ProcessChannel(Xcp_Config.XcpEventChannel+channel);

}

/**
 * Scheduled function of the XCP module
 *
 * ServiceId: 0x04
 *
 */
void Xcp_MainFunction(void)
{
    DET_VALIDATE_NRV(Xcp_Inited, 0x04, XCP_E_NOT_INITIALIZED);

    /* check if we have some queued worker */
    if(Xcp_Worker) {
        Xcp_Worker();
    } else {
        Xcp_Recieve_Main();
    }
    Xcp_Transmit_Main();
}

