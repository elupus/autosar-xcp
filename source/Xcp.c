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

#include "Std_Types.h"
#include "Xcp.h"
#include "Xcp_Cfg.h"
#include "Xcp_Internal.h"
#include "Xcp_ByteStream.h"
#include <string.h>

#if(0)
static Xcp_GeneralType g_general = 
{
    .XcpMaxDaq          = XCP_MAX_DAQ
  , .XcpMaxEventChannel = XCP_MAX_EVENT_CHANNEL
  , .XcpMinDaq          = XCP_MIN_DAQ
  , .XcpDaqCount        = XCP_MIN_DAQ
  , .XcpMaxDto          = XCP_MAX_DTO
  , .XcpMaxCto          = XCP_MAX_CTO
};
#endif

Xcp_BufferType g_XcpBuffers[XCP_MAX_RXTX_QUEUE];
Xcp_FifoType   g_XcpXxFree;
Xcp_FifoType   g_XcpRxFifo = { .free = &g_XcpXxFree };
Xcp_FifoType   g_XcpTxFifo = { .free = &g_XcpXxFree };

static int            g_XcpConnected;

static Xcp_TransferType g_Download;
static Xcp_DaqPtrStateType g_DaqState;
static Xcp_TransferType g_Upload;
static Xcp_CmdWorkType  Xcp_Worker;
Xcp_MtaType      Xcp_Mta;

const Xcp_ConfigType *g_XcpConfig;


/**
 * Initializing function
 *
 * ServiceId: 0x00
 *
 * @param Xcp_ConfigPtr
 */

void Xcp_Init(const Xcp_ConfigType* Xcp_ConfigPtr)
{
#if(XCP_DEV_ERROR_DETECT)
    if(!Xcp_ConfigPtr) {
        Det_ReportError(XCP_MODULE_ID, 0, 0x00, XCP_E_INV_POINTER)
        return;
    }
#endif
    g_XcpConfig = Xcp_ConfigPtr;
    Xcp_Fifo_Init(&g_XcpXxFree, g_XcpBuffers, g_XcpBuffers+sizeof(g_XcpBuffers)/sizeof(g_XcpBuffers[0]));

#if(XCP_FEATURE_DAQSTIM_DYNAMIC == STD_OFF)
    uint8 pid = 0;
    Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList;
    for (int i = 0; i < XCP_MAX_DAQ; i++ , daq++) {
        if(XCP_IDENTIFICATION != XCP_IDENTIFICATION_ABSOLUTE) {
            pid = 0;
        }

        Xcp_OdtType* odt = daq->XcpOdt;
        for (int j = 0; j < daq->XcpMaxOdt; j++, odt++) {
            odt->XcpOdtEntriesCount = odt->XcpMaxOdtEntries;
            odt->XcpOdt2DtoMapping.XcpDtoPid = pid++;
        }
    }
#endif
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

    FIFO_GET_WRITE(g_XcpRxFifo, it) {
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
static void Xcp_ProcessDaq(const Xcp_DaqListType* daq)
{
    uint32 ct = Xcp_GetTimeStamp();
    int    ts = daq->XcpParams.Mode & XCP_DAQLIST_MODE_TIMESTAMP;

    if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_STIM) /* STIM unsupported */
        return;
    for(int o = 0; o < daq->XcpOdtCount; o++) {
        const Xcp_OdtType* odt = daq->XcpOdt+o;

        if(!odt->XcpOdtEntriesValid)
            continue;

        FIFO_GET_WRITE(g_XcpTxFifo, e) {

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

            for(int i = 0; i < odt->XcpOdtEntriesCount; i++) {
                const Xcp_OdtEntryType* ent = odt->XcpOdtEntry+i;
                uint8  len = ent->XcpOdtEntryLength;
                Xcp_MtaType mta;
                Xcp_MtaInit(&mta, ent->XcpOdtEntryAddress, ent->XcpOdtEntryExtension);

                if(len + e->len > XCP_MAX_DTO)
                    break;

                Xcp_MtaRead(&mta, e->data+e->len, len);
                e->len += len;
            }

        }

    }
}

/* Process all entries in event channel */
static void Xcp_ProcessChannel(Xcp_EventChannelType* ech)
{
    for(int d = 0; d < ech->XcpEventChannelMaxDaqList; d++) {
        Xcp_DaqListType* daq = ech->XcpEventChannelTriggeredDaqListRef[d];
        if(!daq)
            continue;
        if(!(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING))
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
void Xcp_TxError(unsigned int code)
{
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
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
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        e->len = 1;
    }
}

/**************************************************************************/
/**************************************************************************/
/**************************** GENERIC COMMANDS ****************************/
/**************************************************************************/
/**************************************************************************/

Std_ReturnType Xcp_CmdConnect(uint8 pid, void* data, int len)
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

    g_XcpConnected = 1;

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
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

Std_ReturnType Xcp_CmdGetStatus(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received get_status\n");

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0 << 0 /* STORE_CAL_REQ */
                      | 0 << 2 /* STORE_DAQ_REQ */
                      | 0 << 3 /* CLEAR_DAQ_REQ */
                      | 0 << 6 /* DAQ_RUNNING   */
                      | 0 << 7 /* RESUME */);
        FIFO_ADD_U8 (e, 0 << 0 /* CAL/PAG */
                      | 0 << 2 /* DAQ     */
                      | 0 << 3 /* STIM    */
                      | 0 << 4 /* PGM     */); /* Content resource protection */
        FIFO_ADD_U8 (e, 0); /* Reserved */
        FIFO_ADD_U16(e, 0); /* Session configuration ID */
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdGetCommModeInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received get_comm_mode_info\n");
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0); /* Reserved */
        FIFO_ADD_U8 (e, 0 << 0 /* MASTER_BLOCK_MODE */
                      | 0 << 1 /* INTERLEAVED_MODE  */);
        FIFO_ADD_U8 (e, 0); /* Reserved */
        FIFO_ADD_U8 (e, 0); /* MAX_BS */
        FIFO_ADD_U8 (e, 0); /* MIN_ST [100 microseconds] */
        FIFO_ADD_U8 (e, XCP_MAX_RXTX_QUEUE-1); /* QUEUE_SIZE */
        FIFO_ADD_U8 (e, XCP_PROTOCOL_MAJOR_VERSION << 4
                      | XCP_PROTOCOL_MINOR_VERSION); /* Xcp driver version */
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdGetId(uint8 pid, void* data, int len)
{
	uint8 idType = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received get_id %d\n", idType);

    const char* text = NULL;

	if(idType == 0 ) {
	    text = g_XcpConfig->XcpInfo.XcpCaption;
	} else if(idType == 1) {
	    text = g_XcpConfig->XcpInfo.XcpMC2File;
	} else if(idType == 2 ) {
        text = g_XcpConfig->XcpInfo.XcpMC2Path;
	} else if(idType == 3 ) {
        text = g_XcpConfig->XcpInfo.XcpMC2Url;
	} else if(idType == 4 ) {
        text = g_XcpConfig->XcpInfo.XcpMC2Upload;
	}

	if(!text)
	    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_GetId - Id type %d not supported\n", idType);

    Xcp_MtaInit(&Xcp_Mta, (intptr_t)text, XCP_MTA_EXTENSION_MEMORY);
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8  (e, XCP_PID_RES);
        FIFO_ADD_U8  (e, 0); /* Mode TODO Check appropriate mode */
        FIFO_ADD_U16 (e, 0); /* Reserved */
        FIFO_ADD_U32 (e, strlen(text)); /* Length */
    }
    return E_OK;

}

Std_ReturnType Xcp_CmdDisconnect(uint8 pid, void* data, int len)
{
    if(g_XcpConnected) {
        DEBUG(DEBUG_HIGH, "Received disconnect\n");
    } else {
        DEBUG(DEBUG_HIGH, "Invalid disconnect without connect\n");
    }
    g_XcpConnected = 0;
    RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdSync(uint8 pid, void* data, int len)
{
    RETURN_ERROR(XCP_ERR_CMD_SYNCH, "Xcp_CmdSync\n");
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
void Xcp_CmdUpload_Worker(void)
{
    unsigned len = g_Upload.rem;
    unsigned off = XCP_ELEMENT_OFFSET(1);
    unsigned max = XCP_MAX_CTO - off - 1;

    if(len > max)
        len = max;

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        for(unsigned int i = 0; i < off; i++)
            SET_UINT8 (e->data, i+1, 0);
        for(unsigned int i = 0; i < len; i++)
            SET_UINT8 (e->data, i+1+off, Xcp_MtaGet(&Xcp_Mta));
        e->len = len+1+off;
    }
    g_Upload.rem -= len;

    if(g_Upload.rem == 0)
        Xcp_Worker = NULL;
}

Std_ReturnType Xcp_CmdUpload(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received upload\n");
	g_Upload.len = GET_UINT8(data, 0) * XCP_ELEMENT_SIZE;;
	g_Upload.rem = g_Upload.len;

#ifndef XCP_FEATURE_BLOCKMODE
	if(g_Upload.len + 1 > XCP_MAX_CTO) {
	    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_CmdUpload - Block mode not supported\n");
	}
#endif

    Xcp_Worker = Xcp_CmdUpload_Worker;
    Xcp_Worker();
    return E_OK;
}

Std_ReturnType Xcp_CmdShortUpload(uint8 pid, void* data, int len)
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

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        if(XCP_ELEMENT_SIZE > 1)
            memset(e->data+1, 0, XCP_ELEMENT_SIZE - 1);
        Xcp_MtaRead(&Xcp_Mta, e->data + XCP_ELEMENT_SIZE, count);
        e->len = count + XCP_ELEMENT_SIZE;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdSetMTA(uint8 pid, void* data, int len)
{
    int ext = GET_UINT8 (data, 2);
    int ptr = GET_UINT32(data, 3);
    DEBUG(DEBUG_HIGH, "Received set_mta 0x%x, %d\n", ptr, ext);

    Xcp_MtaInit(&Xcp_Mta, ptr, ext);
    RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdDownload(uint8 pid, void* data, int len)
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
        g_Download.len = rem;
        g_Download.rem = rem;
    }

    /* check for sequence error */
    if(g_Download.rem != rem) {
        DEBUG(DEBUG_HIGH, "Xcp_Download - Invalid next state (%u, %u)\n", rem, g_Download.rem);
        FIFO_GET_WRITE(g_XcpTxFifo, e) {
            FIFO_ADD_U8 (e, XCP_PID_ERR);
            FIFO_ADD_U8 (e, XCP_ERR_SEQUENCE);
            FIFO_ADD_U8 (e, g_Download.rem / XCP_ELEMENT_SIZE);
        }
        return E_OK;
    }

    /* write what we got this packet */
    if(rem > len - off) {
        rem = len - off;
    }

    Xcp_MtaWrite(&Xcp_Mta, (uint8*)data + off, rem);
    g_Download.rem -= rem;

    if(g_Download.rem)
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

Std_ReturnType Xcp_CmdBuildChecksum(uint8 pid, void* data, int len)
{
    uint32 block = GET_UINT32(data, 3);

    DEBUG(DEBUG_HIGH, "Received build_checksum %ul\n", (unsigned int)block);

    if(!Xcp_Mta.get) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Xcp_CmdBuildChecksum - Mta not inited\n");
    }

    uint8 type = XCP_CHECKSUM_ADD_11;
    uint8 res  = Xcp_CmdBuildChecksum_Add11(block);

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
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

Std_ReturnType Xcp_CmdSetCalPage(uint8 pid, void* data, int len)
{
    unsigned int mode = GET_UINT8(data, 0);
    unsigned int segm = GET_UINT8(data, 1);
    unsigned int page = GET_UINT8(data, 2);
    DEBUG(DEBUG_HIGH, "Received SetCalPage(0x%x, %u, %u)\n", mode, segm, page);

    Xcp_SegmentType* begin = NULL, *end = NULL;
    if(mode & 0x80) {
        begin = g_XcpConfig->XcpSegment;
        end   = begin + XCP_MAX_SEGMENT;
    } else {
        if(segm >= XCP_MAX_SEGMENT) {
            RETURN_ERROR(XCP_ERR_SEGMENT_NOT_VALID, "Xcp_CmdSetCalPage(0x%x, %u, %u) - invalid segment\n", mode, segm, page);
        }

        begin = g_XcpConfig->XcpSegment+segm;
        end   = begin + 1;
    }

    for(Xcp_SegmentType* s = begin; s != end; s++) {
        if(page >= s->XcpMaxPage) {
            RETURN_ERROR(XCP_ERR_PAGE_NOT_VALID, "Xcp_CmdSetCalPage(0x%x, %u, %u) - invalid page\n", mode, s-g_XcpConfig->XcpSegment, page);
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

Std_ReturnType Xcp_CmdGetCalPage(uint8 pid, void* data, int len)
{
    unsigned int mode = GET_UINT8(data, 0);
    unsigned int segm = GET_UINT8(data, 1);
    unsigned int page = 0;
    DEBUG(DEBUG_HIGH, "Received GetCalPage(0x%x, %u)\n", mode, segm);

    if(segm >= XCP_MAX_SEGMENT) {
        RETURN_ERROR(XCP_ERR_SEGMENT_NOT_VALID, "Xcp_CmdGetCalPage(0x%x, %u, %u) - invalid segment\n", mode, segm, page);
    }

    if(mode == 0x01) {
        page = g_XcpConfig->XcpSegment[segm].XcpPageEcu;
    } else if(mode == 0x02) {
        page = g_XcpConfig->XcpSegment[segm].XcpPageXcp;
    } else {
        RETURN_ERROR(XCP_ERR_CMD_SYNTAX, "Xcp_CmdGetCalPage(0x%x, %u) - invalid mode\n", mode, segm);
    }

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0); /* reserved */
        FIFO_ADD_U8 (e, 0); /* reserved */
        FIFO_ADD_U8 (e, page);
    }
    return E_OK;
}

/**************************************************************************/
/**************************************************************************/
/*************************** DAQ/STIM COMMANDS ****************************/
/**************************************************************************/
/**************************************************************************/

Std_ReturnType Xcp_CmdClearDaqList(uint8 pid, void* data, int len)
{
    uint16 daqListNumber = GET_UINT16(data, 1);
    if(daqListNumber >= XCP_MAX_DAQ)
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Daqlist number out of range\n");

    Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;

    if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
        RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");

    Xcp_OdtEntryType* entry;

    Xcp_OdtType* odt = daq->XcpOdt;
    for( int i = 0; i < daq->XcpOdtCount ;  i++, odt++ ) {
        odt->XcpOdtEntriesValid = 0;
        entry = odt->XcpOdtEntry;
        for(int j = 0; j < odt->XcpOdtEntriesCount ;  j++, entry++) {
            entry->XcpOdtEntryAddress   = 0;
            entry->XcpOdtEntryExtension = 0;
            entry->XcpOdtEntryLength    = 0;
            entry->BitOffSet            = 0xFF;
        }
    }
	RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdSetDaqPtr(uint8 pid, void* data, int len)
{
	uint16 daqListNumber = GET_UINT16(data, 1);
    uint8 odtNumber      = GET_UINT8(data, 3);
    uint8 odtEntryNumber = GET_UINT8(data, 4);
    DEBUG(DEBUG_HIGH, "Received SetDaqPtr %u, %u, %u\n", daqListNumber, odtNumber, odtEntryNumber );


	if(daqListNumber >= XCP_MAX_DAQ)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");

	Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;

    if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
        RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");

	if(odtNumber >= daq->XcpMaxOdt)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: odt number out of range (%u, %u)\n", odtNumber, daq->XcpMaxOdt);

	if(odtEntryNumber >= (daq->XcpOdt+odtNumber)->XcpOdtEntriesCount)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: odt entry number out of range\n");

	g_DaqState.daq = daq;
	g_DaqState.odt = daq->XcpOdt+odtNumber;
	g_DaqState.ptr = (daq->XcpOdt+odtNumber)->XcpOdtEntry+odtEntryNumber;

	RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdWriteDaq(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received WriteDaq\n");

#if 0 //
	if(g_DaqState.daq->XcpDaqListNumber < XCP_MIN_DAQ){ /* Check if DAQ list is write protected */
	    RETURN_ERROR(XCP_ERR_WRITE_PROTECTED, "Error: DAQ-list is read only\n");
	}
#endif

	if(g_DaqState.ptr == NULL)
	    RETURN_ERROR(XCP_ERR_DAQ_CONFIG, "Error: No more ODT entries in this ODT\n");

	if(g_DaqState.daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
	    RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");



	uint8 maxOdtEntrySize;
	uint8 granularityOdtEntrySize;

	if(g_DaqState.daq->XcpParams.Mode & XCP_DAQLIST_MODE_STIM) /* Get DAQ list Direction */
	{
	    /* TODO Connect with Parameters in GetDaqResolutionInfo */
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
            g_DaqState.ptr->BitOffSet  =  bitOffSet;
        } else
        {
            RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Element size and granularity don't match\n");
        }
    } else
    {
        g_DaqState.ptr->BitOffSet  = 0xFF;
    }

	g_DaqState.ptr->XcpOdtEntryExtension  = GET_UINT8(data, 2);
	g_DaqState.ptr->XcpOdtEntryAddress    = GET_UINT32(data, 3);

	// Increment and decrement the count of valid odt entries
	if(daqElemSize && !g_DaqState.ptr->XcpOdtEntryLength)
	    g_DaqState.odt->XcpOdtEntriesValid++;
    if(!daqElemSize && g_DaqState.ptr->XcpOdtEntryLength)
        g_DaqState.odt->XcpOdtEntriesValid--;

	g_DaqState.ptr->XcpOdtEntryLength  = daqElemSize;

	//DEBUG(DEBUG_HIGH, "Address: %d\n", (int) g_DaqState.ptr->XcpOdtEntryAddress);

    if(++g_DaqState.ptr == g_DaqState.odt->XcpOdtEntry + g_DaqState.odt->XcpOdtEntriesCount) {
        g_DaqState.ptr = NULL;
        g_DaqState.daq = NULL;
        g_DaqState.odt = NULL;
    }

	RETURN_SUCCESS();
}

static void Xcp_CmdSetDaqListMode_EventChannel(Xcp_DaqListType* daq, uint16 newEventChannelNumber) {

    uint16 oldEventChannelNumber = daq->XcpParams.EventChannel;
    Xcp_EventChannelType* oldEventChannel = g_XcpConfig->XcpEventChannel+oldEventChannelNumber;
    Xcp_EventChannelType* newEventChannel = g_XcpConfig->XcpEventChannel+newEventChannelNumber;

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

    newEventChannel->XcpEventChannelTriggeredDaqListRef[newEventChannel->XcpEventChannelDaqCount] = daq;
    newEventChannel->XcpEventChannelDaqCount++;
    daq->XcpParams.EventChannel = newEventChannelNumber;
}

Std_ReturnType Xcp_CmdSetDaqListMode(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received SetDaqListMode\n");
	uint16 list = GET_UINT16(data, 1);
	if(list >= XCP_MAX_DAQ)
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");

	uint8 prio = GET_UINT8(data, 6);
	if(prio)
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Priority %d of DAQ lists is not supported\n", prio);

	Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+list;

	if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING)
	        RETURN_ERROR(XCP_ERR_DAQ_ACTIVE, "Error: DAQ running\n");


	/* TODO Check to see if the event channel supports the direction of the DAQ list. */
	if(g_XcpConfig->XcpEventChannel+GET_UINT16(data, 3))

	daq->XcpParams.Mode         = (GET_UINT8 (data, 0) & 0x32) | (daq->XcpParams.Mode & ~0x32);
	Xcp_CmdSetDaqListMode_EventChannel(daq,GET_UINT16(data, 3));
	daq->XcpParams.Prescaler	= GET_UINT8 (data, 5);
	daq->XcpParams.Priority		= prio;
	RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdGetDaqListMode(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqListMode\n");
	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= XCP_MAX_DAQ) {
	    RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: DAQ list number out of range\n");
	}
	const Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, daq->XcpParams.Mode);         /* Mode */
        FIFO_ADD_U16(e, 0); 						  /* Reserved */
        FIFO_ADD_U16(e, daq->XcpParams.EventChannel); /* Current Event Channel Number */
        FIFO_ADD_U8 (e, daq->XcpParams.Prescaler);	  /* Current Prescaler */
        FIFO_ADD_U8 (e, daq->XcpParams.Priority);	  /* Current DAQ list Priority */
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdStartStopDaqList(uint8 pid, void* data, int len)
{
	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= XCP_MAX_DAQ) {
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");
	}
	Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;

	uint8 mode = GET_UINT8(data, 0);
	if ( mode == 0) {
		/* START */
		daq->XcpParams.Mode |= XCP_DAQLIST_MODE_RUNNING;
	} else if ( mode == 1) {
		/* STOP */
		daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_RUNNING;
	} else if ( mode == 2) {
		/* SELECT */
		daq->XcpParams.Mode |= XCP_DAQLIST_MODE_SELECTED;
	} else {
		RETURN_ERROR(XCP_ERR_MODE_NOT_VALID,"Error mode not valid\n"); // TODO Error
	}

	FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8(e, XCP_PID_RES);
        FIFO_ADD_U8(e, daq->XcpOdt->XcpOdt2DtoMapping.XcpDtoPid);
    }
	return E_OK;
}

Std_ReturnType Xcp_CmdStartStopSynch(uint8 pid, void* data, int len)
{
    uint8 mode = GET_UINT8(data, 0);
    Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList;
    if ( mode == 0) {
        /* STOP ALL */
        for( int i = 0; i < XCP_MAX_DAQ ; i++, daq++) {
            daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_RUNNING;
            daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_SELECTED;
        }
    } else if ( mode == 1) {
        /* START SELECTED */
        for( int i = 0; i < XCP_MAX_DAQ ; i++, daq++) {
            if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_SELECTED) {
                daq->XcpParams.Mode |=  XCP_DAQLIST_MODE_RUNNING;
                daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_SELECTED;
            }
        }
    } else if ( mode == 2) {
        /* STOP SELECTED */
        for( int i = 0; i < XCP_MAX_DAQ ; i++, daq++) {
            if(daq->XcpParams.Mode & XCP_DAQLIST_MODE_SELECTED) {
                daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_RUNNING;
                daq->XcpParams.Mode &= ~XCP_DAQLIST_MODE_SELECTED;
            }
        }
    } else {
        RETURN_ERROR(XCP_ERR_MODE_NOT_VALID,"Error mode not valid\n");
    }
    DEBUG(DEBUG_HIGH, "EC1: %d EC2: %d\n" , g_XcpConfig->XcpEventChannel->XcpEventChannelDaqCount, (g_XcpConfig->XcpEventChannel+1)->XcpEventChannelDaqCount);
    RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdGetDaqClock(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqClock\n");

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, 0); /* Alignment */
        FIFO_ADD_U8 (e, 0); /* Alignment */
        FIFO_ADD_U8 (e, 0); /* Alignment */
        FIFO_ADD_U32(e, Xcp_GetTimeStamp());
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdReadDaq(uint8 pid, void* data, int len)
{
    if(!g_DaqState.ptr) {
        RETURN_ERROR(XCP_ERR_DAQ_CONFIG, "Error: No more ODT entries in this ODT\n");
    }
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8 (e, g_DaqState.ptr->BitOffSet);
        FIFO_ADD_U8 (e, g_DaqState.ptr->XcpOdtEntryLength);
        FIFO_ADD_U8 (e, g_DaqState.ptr->XcpOdtEntryExtension);
        FIFO_ADD_U32(e, g_DaqState.ptr->XcpOdtEntryAddress);
    }

    if(++g_DaqState.ptr == g_DaqState.odt->XcpOdtEntry + g_DaqState.odt->XcpOdtEntriesCount) {
        g_DaqState.ptr = NULL;
        g_DaqState.daq = NULL;
        g_DaqState.odt = NULL;
    }

    return E_OK;
}

Std_ReturnType Xcp_CmdGetDaqProcessorInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqProcessorInfo\n");
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        FIFO_ADD_U8 (e, XCP_PID_RES);
        FIFO_ADD_U8 (e, (XCP_MAX_DAQ > XCP_MIN_DAQ ? 1 : 0) << 0 /* DAQ_CONFIG_TYPE     */
                      | 1 << 1 /* PRESCALER_SUPPORTED */
                      | 0 << 2 /* RESUME_SUPPORTED    */
                      | 0 << 3 /* BIT_STIM_SUPPORTED  */
                      | (XCP_TIMESTAMP_SIZE > 0 ? 1 : 0) << 4 /* TIMESTAMP_SUPPORTED */
                      | 0 << 5 /* PID_OFF_SUPPORTED   */
                      | 0 << 6 /* OVERLOAD_MSB        */
                      | 0 << 7 /* OVERLOAD_EVENT      */);
        FIFO_ADD_U16(e, XCP_MAX_DAQ);
        FIFO_ADD_U16(e, XCP_MAX_EVENT_CHANNEL);
        FIFO_ADD_U8 (e, XCP_MIN_DAQ);
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

Std_ReturnType Xcp_CmdGetDaqResolutionInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqResolutionInfo\n");
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, XCP_GRANULARITY_ODT_ENTRY_SIZE_DAQ);  /* GRANULARITY_ODT_ENTRY_SIZE_DAQ */
        SET_UINT8 (e->data, 2, XCP_MAX_ODT_ENTRY_SIZE_DAQ); 		 /* MAX_ODT_ENTRY_SIZE_DAQ */             /* TODO */
        SET_UINT8 (e->data, 3, XCP_GRANULARITY_ODT_ENTRY_SIZE_STIM); /* GRANULARITY_ODT_ENTRY_SIZE_STIM */
        SET_UINT8 (e->data, 4, XCP_MAX_ODT_ENTRY_SIZE_STIM); 		 /* MAX_ODT_ENTRY_SIZE_STIM */            /* TODO */
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

Std_ReturnType Xcp_CmdGetDaqListInfo(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received GetDaqListInfo\n");
	uint16 daqListNumber = GET_UINT16(data, 1);

	if(daqListNumber >= XCP_MAX_DAQ)  // Should be XcpDaqCount?
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Xcp_GetDaqListInfo list number out of range\n");
	}

	const Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;

	FIFO_GET_WRITE(g_XcpTxFifo, e) {
		SET_UINT8  (e->data, 0, XCP_PID_RES);
		SET_UINT8  (e->data, 1, daq->XcpParams.Properties);
		SET_UINT8  (e->data, 2, daq->XcpMaxOdt); /* MAX_ODT */
		SET_UINT8  (e->data, 3, XCP_MAX_ODT_ENTRIES); /* MAX_ODT_ENTRIES */
		SET_UINT16 (e->data, 4, daq->XcpParams.EventChannel); /* FIXED_EVENT */ /*TODO EventChannel */
		e->len = 6;
	}
	return E_OK;
}

Std_ReturnType Xcp_CmdGetDaqEventInfo(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received GetDaqEventInfo\n");
	uint16 eventChannelNumber = GET_UINT16(data, 1);

	if( eventChannelNumber >= XCP_MAX_EVENT_CHANNEL)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Xcp_CmdGetDaqEventInfo event channel number out of range\n");
	}

	const Xcp_EventChannelType* eventChannel = g_XcpConfig->XcpEventChannel+eventChannelNumber;

	uint8 namelen = 0;
	if(eventChannel->XcpEventChannelName) {
	    namelen = strlen(eventChannel->XcpEventChannelName);
	    Xcp_MtaInit(&Xcp_Mta, (intptr_t)eventChannel->XcpEventChannelName, XCP_MTA_EXTENSION_MEMORY);
	}

	FIFO_GET_WRITE(g_XcpTxFifo, e) {
		SET_UINT8 (e->data, 0, XCP_PID_RES);
		SET_UINT8 (e->data, 1, eventChannel->XcpEventChannelProperties );
		SET_UINT8 (e->data, 2, eventChannel->XcpEventChannelMaxDaqList);
		SET_UINT8 (e->data, 3, namelen); /* Name length */
		SET_UINT8 (e->data, 4, eventChannel->XcpEventChannelRate);     /* Cycle time */
		SET_UINT8 (e->data, 5, eventChannel->XcpEventChannelUnit);     /* Time unit */
		SET_UINT8 (e->data, 6, eventChannel->XcpEventChannelPriority); /* Event channel priority */
		e->len = 7;
	}

	return E_OK;
}

Std_ReturnType Xcp_CmdFreeDaq(uint8 pid, void* data, int len)
{
    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Free DAQ not implemented.\n");
}

Std_ReturnType Xcp_CmdAllocDaq(uint8 pid, void* data, int len)
{

//    uint16 nrDaqs = GET_UINT16(data, 1);
//    Xcp_DaqListType *daqs[nrDaqs]; /* Global? */
//    for( int i = 0 ; i < nrDaqs ; i++ ) {
//        daqs[i] = malloc( sizeof( Xcp_DaqListType ) );
//    }
//
//    daqs[1]->XcpDaqListNumber = 12;
//
    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Allocate DAQ not implemented.\n");
}

Std_ReturnType Xcp_CmdAllocOdt(uint8 pid, void* data, int len)
{
    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Allocate ODT not implemented.\n");
}

Std_ReturnType Xcp_CmdAllocOdtEntry(uint8 pid, void* data, int len)
{
    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Allocate ODT Entry not implemented.\n");
}

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

#if(XCP_FEATURE_PROGRAM)
  , [XCP_PID_CMD_PGM_PROGRAM_START]           = { .fun = Xcp_CmdProgramStart        , .len = 0 }
  , [XCP_PID_CMD_PGM_PROGRAM_CLEAR]           = { .fun = Xcp_CmdProgramClear        , .len = 8 }
  , [XCP_PID_CMD_PGM_PROGRAM]                 = { .fun = Xcp_CmdProgram             , .len = 3 }
#if(XCP_FEATURE_BLOCKMODE)
  , [XCP_PID_CMD_PGM_PROGRAM_NEXT]            = { .fun = Xcp_CmdProgram             , .len = 3 }
#endif
  , [XCP_PID_CMD_PGM_PROGRAM_RESET]           = { .fun = Xcp_CmdProgramReset        , .len = 0 }
#endif // XCP_FEATURE_PROGRAM

#if(XCP_FEATURE_CALPAG)
  , [XCP_PID_CMD_PAG_SET_CAL_PAGE]            = { .fun = Xcp_CmdSetCalPage          , .len = 4 }
  , [XCP_PID_CMD_PAG_GET_CAL_PAGE]            = { .fun = Xcp_CmdGetCalPage          , .len = 3 }
  , [XCP_PID_CMD_CAL_DOWNLOAD]                = { .fun = Xcp_CmdDownload            , .len = 3 }
#if(XCP_FEATURE_BLOCKMODE)
  , [XCP_PID_CMD_CAL_DOWNLOAD_NEXT]           = { .fun = Xcp_CmdDownload            , .len = 3 }
#endif
#endif // XCP_FEATURE_CALPAG
  , [XCP_PID_CMD_DAQ_CLEAR_DAQ_LIST]          = { .fun = Xcp_CmdClearDaqList        , .len = 3 }
  , [XCP_PID_CMD_DAQ_SET_DAQ_PTR]			  = { .fun = Xcp_CmdSetDaqPtr         	, .len = 5 }
  , [XCP_PID_CMD_DAQ_WRITE_DAQ]               = { .fun = Xcp_CmdWriteDaq            , .len = 7 }
  , [XCP_PID_CMD_DAQ_SET_DAQ_LIST_MODE]       = { .fun = Xcp_CmdSetDaqListMode      , .len = 7 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE]       = { .fun = Xcp_CmdGetDaqListMode      , .len = 3 }
  , [XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST]     = { .fun = Xcp_CmdStartStopDaqList    , .len = 3 }
  , [XCP_PID_CMD_DAQ_START_STOP_SYNCH]        = { .fun = Xcp_CmdStartStopSynch      , .len = 1 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_CLOCK]           = { .fun = Xcp_CmdGetDaqClock         , .len = 0 }
  , [XCP_PID_CMD_DAQ_READ_DAQ]                = { .fun = Xcp_CmdReadDaq             , .len = 0 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO]  = { .fun = Xcp_CmdGetDaqProcessorInfo , .len = 0 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_RESOLUTION_INFO] = { .fun = Xcp_CmdGetDaqResolutionInfo, .len = 0 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO]       = { .fun = Xcp_CmdGetDaqListInfo      , .len = 3 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO]      = { .fun = Xcp_CmdGetDaqEventInfo     , .len = 3 }
  , [XCP_PID_CMD_DAQ_FREE_DAQ]                = { .fun = Xcp_CmdFreeDaq             , .len = 0 }
  , [XCP_PID_CMD_DAQ_ALLOC_DAQ]               = { .fun = Xcp_CmdAllocDaq            , .len = 3 }
  , [XCP_PID_CMD_DAQ_ALLOC_ODT]               = { .fun = Xcp_CmdAllocOdt            , .len = 4 }
  , [XCP_PID_CMD_DAQ_ALLOC_ODT_ENTRY]         = { .fun = Xcp_CmdAllocOdtEntry       , .len = 5 }
};


/**
 * Xcp_Recieve_Main is the main process that executes all received commands.
 *
 * The function queues up replies for transmission. Which will be sent
 * when Xcp_Transmit_Main function is called.
 */
void Xcp_Recieve_Main()
{
    FIFO_FOR_READ(g_XcpRxFifo, it) {
        uint8 pid = GET_UINT8(it->data,0);

        /* ignore commands when we are not connected */
        if(!g_XcpConnected && pid != XCP_PID_CMD_STD_CONNECT
                           && pid != XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD) {
            continue;
        }

        Xcp_CmdListType* cmd = Xcp_CmdList+pid;
        if(cmd->fun) {
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
    FIFO_FOR_READ(g_XcpTxFifo, it) {
        if(Xcp_Transmit(it->data, it->len) != E_OK) {
            DEBUG(DEBUG_HIGH, "Xcp_Transmit_Main - failed to transmit\n");
        }
    }
}


/**
 * Scheduled function of the XCP module
 *
 * ServiceId: 0x04
 *
 */
void Xcp_MainFunction(void)
{
    /* process all channels */
    Xcp_ProcessChannel(g_XcpConfig->XcpEventChannel);

#if(XCP_MAX_EVENT_CHANNEL > 1)
    static uint8 idle = 0;
    if(idle++ % 10 == 0) {
        Xcp_ProcessChannel(g_XcpConfig->XcpEventChannel+1);
    }
#endif


    /* check if we have some queued worker */
    if(Xcp_Worker) {
        Xcp_Worker();
    } else {
        Xcp_Recieve_Main();
    }
    Xcp_Transmit_Main();
}

