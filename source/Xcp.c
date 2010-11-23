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

Xcp_FifoType   g_XcpRxFifo;
Xcp_FifoType   g_XcpTxFifo;

static int            g_XcpConnected;
static const char	  g_XcpFileName[] = "XcpSer";

static Xcp_DownloadType g_Download;
static Xcp_DaqPtrStateType g_DaqState;
static Xcp_UploadType   g_Upload;
static Xcp_CmdWorkType  Xcp_Worker;

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
    Xcp_FifoInit(&g_XcpRxFifo);
    Xcp_FifoInit(&g_XcpTxFifo);
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
        DEBUG(DEBUG_HIGH, "Xcp_RxIndication - length %d too long\n", len)
        return;
    }

    if(len == 0)
        return;

    FIFO_GET_WRITE(g_XcpRxFifo, it) {
        memcpy(it->data, data, len);
        it->len = len;
    }
}


/* Process all entries in DAQ */
static void Xcp_ProcessDaq(const Xcp_DaqListType* daq)
{
    for(int o = 0; 0 < daq->XcpOdtCount; o++) {
        const Xcp_OdtType* odt = daq->XcpOdt+o;
        for(int e = 0; e < odt->XcpOdtEntriesCount; e++) {
            const Xcp_OdtEntryType* ent = odt->XcpOdtEntry+e;

            if(daq->XcpParams.Mode.bit.direction) {
                /* TODO - read dts for each entry and update memory */
            } else {
                /* TODO - create a DAQ packet */
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
        if(!daq->XcpParams.Mode.bit.running)
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
    DEBUG(DEBUG_HIGH, "Received connect mode %x\n", mode)
    g_XcpConnected = 1;

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1,  (!!XCP_FEATURE_CALPAG) << 0 /* CAL/PAG */
                              | (!!XCP_FEATURE_DAQ)    << 2 /* DAQ     */
                              | (!!XCP_FEATURE_STIM)   << 3 /* STIM    */
                              | (!!XCP_FEATURE_PGM)    << 4 /* PGM     */);
        SET_UINT8 (e->data, 2,  0 << 0 /* BYTE ORDER */
                              | 0 << 1 /* ADDRESS_GRANULARITY */
                              | (!!XCP_FEATURE_BLOCKMODE) << 6 /* SLAVE_BLOCK_MODE    */
                              | 0 << 7 /* OPTIONAL */);
        SET_UINT8 (e->data, 3, XCP_MAX_CTO);
        SET_UINT16(e->data, 4, XCP_MAX_DTO);
        SET_UINT8 (e->data, 6, XCP_PROTOCOL_MAJOR_VERSION  << 4);
        SET_UINT8 (e->data, 7, XCP_TRANSPORT_MAJOR_VERSION << 4);
        e->len = 8;
    }

    return E_OK;
}

Std_ReturnType Xcp_CmdGetStatus(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received get_status\n")

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1,  0 << 0 /* STORE_CAL_REQ */
                              | 0 << 2 /* STORE_DAQ_REQ */
                              | 0 << 3 /* CLEAR_DAQ_REQ */
                              | 0 << 6 /* DAQ_RUNNING   */
                              | 0 << 7 /* RESUME */);
        SET_UINT8 (e->data, 2,  0 << 0 /* CAL/PAG */
                              | 0 << 2 /* DAQ     */
                              | 0 << 3 /* STIM    */
                              | 0 << 4 /* PGM     */); /* Content resource protection */
        SET_UINT8 (e->data, 3, 0); /* Reserved */
        SET_UINT16(e->data, 4, 0); /* Session configuration ID */
        e->len = 6;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdGetCommModeInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received get_comm_mode_info\n");
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, 0); /* Reserved */
        SET_UINT8 (e->data, 2,  0 << 0 /* MASTER_BLOCK_MODE */
                              | 0 << 1 /* INTERLEAVED_MODE  */);
        SET_UINT8 (e->data, 3, 0); /* Reserved */
        SET_UINT8 (e->data, 4, 0); /* MAX_BS */
        SET_UINT8 (e->data, 5, 0); /* MIN_ST [100 microseconds] */
        SET_UINT8 (e->data, 6, XCP_MAX_RXTX_QUEUE-1); /* QUEUE_SIZE */
        SET_UINT8 (e->data, 7, XCP_PROTOCOL_MAJOR_VERSION << 4
                             | XCP_PROTOCOL_MINOR_VERSION); /* Xcp driver version */
        e->len = 8;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdGetId(uint8 pid, void* data, int len)
{
	uint8 idType = GET_UINT8(data, 0);
    DEBUG(DEBUG_HIGH, "Received get_id %d\n", idType);

    const char* text = NULL;

	if(idType == 0 ){
	    text = g_XcpConfig->XcpInfo.XcpCaption;
	} else if(idType == 1){
	    text = g_XcpConfig->XcpInfo.XcpMC2File;
	} else if(idType == 2 ){
        text = g_XcpConfig->XcpInfo.XcpMC2Path;
	} else if(idType == 3 ){
        text = g_XcpConfig->XcpInfo.XcpMC2Url;
	} else if(idType == 4 ){
        text = g_XcpConfig->XcpInfo.XcpMC2Upload;
	}

	if(!text)
	    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_GetId - Id type %d not supported\n", idType);

    Xcp_MtaInit((intptr_t)text, 0);
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8  (e->data, 0, XCP_PID_RES);
        SET_UINT8  (e->data, 1, 0); /* Mode TODO Check appropriate mode */
        SET_UINT16 (e->data, 2, 0); /* Reserved */
        SET_UINT32 (e->data, 4, strlen(text)); /* Length */
        e->len = 8;
    }
    return E_OK;

}

Std_ReturnType Xcp_CmdDisconnect(uint8 pid, void* data, int len)
{
    if(g_XcpConnected) {
        DEBUG(DEBUG_HIGH, "Received disconnect\n")
    } else {
        DEBUG(DEBUG_HIGH, "Invalid disconnect without connect\n")
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
            SET_UINT8 (e->data, i+1+off, Xcp_Mta.get());
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

    Xcp_MtaInit(addr, ext);
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        if(XCP_ELEMENT_SIZE > 1)
            memset(e->data+1, 0, XCP_ELEMENT_SIZE - 1);
        Xcp_Mta.read(e->data + XCP_ELEMENT_SIZE, count);
        e->len = count + XCP_ELEMENT_SIZE;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdSetMTA(uint8 pid, void* data, int len)
{
    int ext = GET_UINT8 (data, 2);
    int ptr = GET_UINT32(data, 3);
    DEBUG(DEBUG_HIGH, "Received set_mta 0x%x, %d\n", ptr, ext);

    Xcp_MtaInit(ptr, ext);
    RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdDownload(uint8 pid, void* data, int len)
{
    unsigned rem = GET_UINT8(data, 0) * XCP_ELEMENT_SIZE;
    unsigned off = XCP_ELEMENT_OFFSET(2) + 1;
    DEBUG(DEBUG_HIGH, "Received download %d, %d\n", pid, len);

    if(!Xcp_Mta.put) {
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
            SET_UINT8 (e->data, 0, XCP_PID_ERR);
            SET_UINT8 (e->data, 1, XCP_ERR_SEQUENCE);
            SET_UINT8 (e->data, 2, g_Download.rem / XCP_ELEMENT_SIZE);
            e->len = 3;
        }
        return E_OK;
    }

    /* write what we got this packet */
    if(rem > len - off) {
        rem = len - off;
    }

    Xcp_Mta.write((uint8*)data + off, rem);

    g_Download.rem -= rem;

    if(g_Download.rem)
        return E_OK;

    RETURN_SUCCESS();
}

static uint32 Xcp_CmdBuildChecksum_Add11(uint32 block)
{
    uint8 res  = 0;
    for(int i = 0; i < block; i++) {
        res += Xcp_Mta.get();
    }
    return res;
}

Std_ReturnType Xcp_CmdBuildChecksum(uint8 pid, void* data, int len)
{
    uint32 block = GET_UINT32(data, 3);

    DEBUG(DEBUG_HIGH, "Received build_checksum %ul\n", (unsigned int)block);

    uint8 type = XCP_CHECKSUM_ADD_11;
    uint8 res  = Xcp_CmdBuildChecksum_Add11(block);

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, type);
        SET_UINT8 (e->data, 2, 0); /* reserved */
        SET_UINT8 (e->data, 3, 0); /* reserved */
        SET_UINT32(e->data, 4, res);
        e->len = 8;
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
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, 0); /* reserved */
        SET_UINT8 (e->data, 2, 0); /* reserved */
        SET_UINT8 (e->data, 3, page);
        e->len = 4;
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





	RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Error: Command not done!\n");
}

Std_ReturnType Xcp_CmdSetDaqPtr(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received SetDaqPtr\n");

	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= XCP_MAX_DAQ)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");
	}

	Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;


	uint8 odtNumber = GET_UINT8(data, 3);
	DEBUG(DEBUG_HIGH, "Max Odt = %d\n", daq->XcpMaxOdt);
	if(odtNumber >= daq->XcpMaxOdt)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: odt number out of range\n");
	}

	uint8 odtEntryNumber = GET_UINT8(data, 4);
	if(odtEntryNumber >= (daq->XcpOdt+odtNumber)->XcpMaxOdtEntries)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: odt entry number out of range\n");
	}

	g_DaqState.daqList = daq;
	g_DaqState.ptr = (daq->XcpOdt+odtNumber)->XcpOdtEntry+odtEntryNumber;

	RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdWriteDaq(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received WriteDaq %d\n",g_DaqState.daqList->XcpDaqListNumber);

	if(g_DaqState.daqList->XcpDaqListNumber < XCP_MIN_DAQ){ /* Check if DAQ list is write protected */
	    RETURN_ERROR(XCP_ERR_WRITE_PROTECTED, "Error: DAQ-list is read only\n");
	}

	uint8 bitOffset = GET_UINT8(data, 0);

	uint8 daqElemSize = GET_UINT8(data, 1);
	uint8 maxOdtEntrySize;
	uint8 granularityOdtEntrySize;

	if(g_DaqState.daqList->XcpParams.Mode.bit.direction &= 0x02) /* Get DAQ list Direction */
	{
	    /* TODO Connect with Parameters in GetDaqResolutionInfo */
	    maxOdtEntrySize = 0;
	    granularityOdtEntrySize = 0;
	}

	if( ( 0 > daqElemSize ) || ( daqElemSize > maxOdtEntrySize ))
	{
	    RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: DAQ list element size is invalid\n");
	}

	uint8 addressExt = GET_UINT8(data, 2);
	g_DaqState.ptr->XcpOdtEntryAddress = (void*) GET_UINT32(data, 3);

	DEBUG(DEBUG_HIGH, "Address: %d\n", (int) g_DaqState.ptr->XcpOdtEntryAddress);
	/* 499602D2 */
	if(++g_DaqState.ptr->XcpOdtEntryNumber == g_DaqState.daqList->XcpOdt->XcpMaxOdtEntries)
	{
	    g_DaqState.ptr = 0; /* Should be undefined... */
	}

	RETURN_SUCCESS();
}



Std_ReturnType Xcp_CmdSetDaqListMode(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received SetDaqListMode\n");
	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= XCP_MAX_DAQ)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");
	}
	Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;
	daq->XcpParams.Mode.u8      = GET_UINT8 (data, 0);
	daq->XcpParams.EventChannel = GET_UINT16(data, 3);
	daq->XcpParams.Prescaler	= GET_UINT8 (data, 5);
	daq->XcpParams.Priority		= GET_UINT8 (data, 6);
	RETURN_SUCCESS();
}

Std_ReturnType Xcp_CmdGetDaqListMode(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqListMode\n");
	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= XCP_MAX_DAQ)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");
	}
	const Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, daq->XcpParams.Mode.u8);      /* Mode */
        SET_UINT16(e->data, 2, 0); 							 /* Reserved */
        SET_UINT16(e->data, 4, daq->XcpParams.EventChannel); /* Current Event Channel Number */
        SET_UINT8 (e->data, 6, daq->XcpParams.Prescaler);	 /* Current Prescaler */
        SET_UINT8 (e->data, 7, daq->XcpParams.Priority);	 /* Current DAQ list Priority */
        e->len = 8;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdStartStopDaqList(uint8 pid, void* data, int len)
{
	uint16 daqListNumber = GET_UINT16(data, 1);
	if(daqListNumber >= XCP_MAX_DAQ)
	{
		RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: daq list number out of range\n");
	}
	Xcp_DaqListType* daq = g_XcpConfig->XcpDaqList+daqListNumber;

	uint8 mode = GET_UINT8(data, 0);
	if ( mode == 0)
	{
		/* START */
		daq->XcpParams.Mode.bit.running = 1;
	} else if ( mode == 1)
	{
		/* STOP */
		daq->XcpParams.Mode.bit.running = 0;
	} else if ( mode == 2)
	{
		/* SELECT */
		daq->XcpParams.Mode.bit.selected = 1;
	} else
	{
		RETURN_ERROR(XCP_ERR_MODE_NOT_VALID,"Error mode not valid\n"); // TODO Error
	}

	FIFO_GET_WRITE(g_XcpTxFifo, e) {
	        SET_UINT8 (e->data, 0, XCP_PID_RES);
	        //SET_UINT8 (e->data, 1, /* FIRST_PID */); /* Ignored in current config */
	        e->len = 1;
	    }
	return E_OK;
}

Std_ReturnType Xcp_CmdStartStopSynch(uint8 pid, void* data, int len)
{
	RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Error: Command not done!\n");
}

Std_ReturnType Xcp_CmdGetDaqClock(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqClock\n");
    TickType counter;

    if(GetCounterValue(XCP_COUNTER_ID, &counter)) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Error: Xcp_CmdGetDaqClock failed to get counter\n");
    }

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, 0); /* Alignment */
        SET_UINT8 (e->data, 2, 0); /* Alignment */
        SET_UINT8 (e->data, 3, 0); /* Alignment */
        SET_UINT32(e->data, 4, counter);
        e->len = 8;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdReadDaq(uint8 pid, void* data, int len)
{
	RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Error: Command not done!\n");
}

Std_ReturnType Xcp_CmdGetDaqProcessorInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqProcessorInfo\n");
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, 1 << 0 /* DAQ_CONFIG_TYPE     */
                             | 0 << 1 /* PRESCALER_SUPPORTED */
                             | 0 << 2 /* RESUME_SUPPORTED    */
                             | 0 << 3 /* BIT_STIM_SUPPORTED  */
                             | 0 << 4 /* TIMESTAMP_SUPPORTED */
                             | 0 << 5 /* PID_OFF_SUPPORTED   */
                             | 0 << 6 /* OVERLOAD_MSB        */
                             | 0 << 7 /* OVERLOAD_EVENT      */);
        SET_UINT16(e->data, 2, XCP_MAX_DAQ);
        SET_UINT16(e->data, 4, XCP_MAX_EVENT_CHANNEL);
        SET_UINT8 (e->data, 6, XCP_MIN_DAQ);
        SET_UINT8 (e->data, 7, 0 << 0 /* Optimisation_Type_0 */
                             | 0 << 1 /* Optimisation_Type_1 */
                             | 0 << 2 /* Optimisation_Type_2 */
                             | 0 << 3 /* Optimisation_Type_3 */
                             | 0 << 4 /* Address_Extension_ODT */
                             | 0 << 5 /* Address_Extension_DAQ */
                             | 0 << 6 /* Identification_Field_Type_0  */
                             | 1 << 7 /* Identification_Field_Type_1 */);
        e->len = 8;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdGetDaqResolutionInfo(uint8 pid, void* data, int len)
{
    DEBUG(DEBUG_HIGH, "Received GetDaqResolutionInfo\n");
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        SET_UINT8 (e->data, 1, 1); /* GRANULARITY_ODT_ENTRY_SIZE_DAQ */
        SET_UINT8 (e->data, 2, 0); /* MAX_ODT_ENTRY_SIZE_DAQ */             /* TODO */
        SET_UINT8 (e->data, 3, 1); /* GRANULARITY_ODT_ENTRY_SIZE_STIM */
        SET_UINT8 (e->data, 4, 0); /* MAX_ODT_ENTRY_SIZE_STIM */            /* TODO */
        SET_UINT8 (e->data, 5, 0); /* TIMESTAMP_MODE */
        SET_UINT16(e->data, 6, 0); /* TIMESTAMP_TICKS */
        e->len = 8;
    }
    return E_OK;
}

Std_ReturnType Xcp_CmdGetDaqListInfo(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received GetDaqListInfo\n");
	uint16 daqListNumber = GET_UINT16(data, 1);

	if(daqListNumber >= XCP_MAX_DAQ)
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

	FIFO_GET_WRITE(g_XcpTxFifo, e) {
		SET_UINT8 (e->data, 0, XCP_PID_RES);
		SET_UINT8 (e->data, 1, 1 << 2 /* DAQ  */  /* TODO Error handling etc. */
							 | 0 << 3 /* STIM */ );
		SET_UINT8 (e->data, 2, eventChannel->XcpEventChannelMaxDaqList);
		SET_UINT8 (e->data, 3, 0); /* Name length */
		SET_UINT8 (e->data, 4, 0); /* Cycle time */
		SET_UINT8 (e->data, 5, 0); /* Time unit */
		SET_UINT8 (e->data, 6, eventChannel->XcpEventChannelPriority); /* Event channel priority */
		e->len = 7;
	}
	return E_OK;
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
    [XCP_PID_CMD_STD_CONNECT]            = { .fun = Xcp_CmdConnect        , .len = 1 }
  , [XCP_PID_CMD_STD_DISCONNECT]         = { .fun = Xcp_CmdDisconnect     , .len = 0 }
  , [XCP_PID_CMD_STD_GET_STATUS]         = { .fun = Xcp_CmdGetStatus      , .len = 0 }
  , [XCP_PID_CMD_STD_GET_ID]             = { .fun = Xcp_CmdGetId          , .len = 1 }
  , [XCP_PID_CMD_STD_UPLOAD]             = { .fun = Xcp_CmdUpload         , .len = 1 }
  , [XCP_PID_CMD_STD_SHORT_UPLOAD]       = { .fun = Xcp_CmdShortUpload    , .len = 8 }
  , [XCP_PID_CMD_STD_SET_MTA]            = { .fun = Xcp_CmdSetMTA         , .len = 3 }
  , [XCP_PID_CMD_STD_SYNCH]              = { .fun = Xcp_CmdSync           , .len = 0 }
  , [XCP_PID_CMD_STD_GET_COMM_MODE_INFO] = { .fun = Xcp_CmdGetCommModeInfo, .len = 0 }
  , [XCP_PID_CMD_STD_BUILD_CHECKSUM]     = { .fun = Xcp_CmdBuildChecksum  , .len = 8 }

#if(XCP_FEATURE_PROGRAM)
  , [XCP_PID_CMD_PGM_PROGRAM_START]      = { .fun = Xcp_CmdProgramStart   , .len = 0 }
  , [XCP_PID_CMD_PGM_PROGRAM_CLEAR]      = { .fun = Xcp_CmdProgramClear   , .len = 8 }
  , [XCP_PID_CMD_PGM_PROGRAM]            = { .fun = Xcp_CmdProgram        , .len = 3 }
#if(XCP_FEATURE_BLOCKMODE)
  , [XCP_PID_CMD_PGM_PROGRAM_NEXT]       = { .fun = Xcp_CmdProgram        , .len = 3 }
#endif
  , [XCP_PID_CMD_PGM_PROGRAM_RESET]      = { .fun = Xcp_CmdProgramReset   , .len = 0 }
#endif // XCP_FEATURE_PROGRAM

#if(XCP_FEATURE_CALPAG)
  , [XCP_PID_CMD_PAG_SET_CAL_PAGE]       = { .fun = Xcp_CmdSetCalPage     , .len = 4 }
  , [XCP_PID_CMD_PAG_GET_CAL_PAGE]       = { .fun = Xcp_CmdGetCalPage     , .len = 3 }
  , [XCP_PID_CMD_CAL_DOWNLOAD]           = { .fun = Xcp_CmdDownload       , .len = 3 }
#if(XCP_FEATURE_BLOCKMODE)
  , [XCP_PID_CMD_CAL_DOWNLOAD_NEXT]      = { .fun = Xcp_CmdDownload       , .len = 3 }
#endif
#endif // XCP_FEATURE_CALPAG
  , [XCP_PID_CMD_DAQ_SET_DAQ_PTR]			  = { .fun = Xcp_CmdSetDaqPtr         	, .len = 5 }
  , [XCP_PID_CMD_DAQ_WRITE_DAQ]               = { .fun = Xcp_CmdWriteDaq            , .len = 7 }
  , [XCP_PID_CMD_DAQ_SET_DAQ_LIST_MODE]       = { .fun = Xcp_CmdSetDaqListMode      , .len = 7 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE]       = { .fun = Xcp_CmdGetDaqListMode      , .len = 3 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_CLOCK]           = { .fun = Xcp_CmdGetDaqClock         , .len = 0 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO]  = { .fun = Xcp_CmdGetDaqProcessorInfo , .len = 0 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_RESOLUTION_INFO] = { .fun = Xcp_CmdGetDaqResolutionInfo, .len = 0 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO]       = { .fun = Xcp_CmdGetDaqListInfo      , .len = 3 }
  , [XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO]      = { .fun = Xcp_CmdGetDaqEventInfo     , .len = 3 }
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
        Xcp_CmdListType* cmd = Xcp_CmdList+pid;
        if(cmd->fun) {
            if(cmd->len && it->len < cmd->len) {
                DEBUG(DEBUG_HIGH, "Xcp_RxIndication_Main - Len %d to short for %u\n", it->len, pid)
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
            DEBUG(DEBUG_HIGH, "Xcp_Transmit_Main - failed to transmit\n")
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
    for(int c = 0; c < XCP_MAX_EVENT_CHANNEL; c++)
        Xcp_ProcessChannel(g_XcpConfig->XcpEventChannel+c);


    /* check if we have some queued worker */
    if(Xcp_Worker) {
        Xcp_Worker();
    } else {
        Xcp_Recieve_Main();
    }
    Xcp_Transmit_Main();
}

