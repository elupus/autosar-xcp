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


#ifdef XCP_STANDALONE
#   define USE_DEBUG_PRINTF
#   define USE_LDEBUG_PRINTF
#   include <stdio.h>
#   include <assert.h>
#   include <string.h>
#else
#   include "Os.h"
#   if(XCP_DEV_ERROR_DETECT)
#       include "Dem.h"
#   endif
#   include "ComStack_Types.h"
#endif

#include "debug.h"
#undef  DEBUG_LVL
#define DEBUG_LVL DEBUG_HIGH


#define RETURN_ERROR(code, ...) do {      \
        DEBUG(DEBUG_HIGH,## __VA_ARGS__ ) \
        Xcp_TxError(code);                \
        return E_NOT_OK;                  \
    } while(0)

#define RETURN_SUCCESS() do { \
        Xcp_TxSuccess();      \
        return E_OK;          \
    } while(0)

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

static Xcp_FifoType   g_XcpRxFifo;
static Xcp_FifoType   g_XcpTxFifo;

static int            g_XcpConnected;
static const char	  g_XcpFileName[] = "XcpSer";

static Xcp_DownloadType g_Download;

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
void Xcp_RxIndication(void* data, int len)
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

#if(0)
/* Process all entriesin DAQ */
static void Xcp_ProcessDaq(const Xcp_DaqListType* daq)
{
    //DEBUG(DEBUG_HIGH, "Processing DAQ %d\n", daq->XcpDaqListNumber);

    for(int o = 0; 0 < daq->XcpOdtCount; o++) {
        const Xcp_OdtType* odt = daq->XcpOdt+o;
        for(int e = 0; e < odt->XcpOdtEntriesCount; e++) {
            //const Xcp_OdtEntryType* ent = odt->XcpOdtEntry+e;

            if(daq->XcpDaqListType == DAQ) {
                /* TODO - create a DAQ packet */
            } else {
                /* TODO - read dts for each entry and update memory */
            }
        }
    }
}

/* Process all entries in event channel */
static void Xcp_ProcessChannel(const Xcp_EventChannelType* ech)
{
    //DEBUG(DEBUG_HIGH, "Processing Channel %d\n",  ech->XcpEventChannelNumber);

    for(int d = 0; d < ech->XcpEventChannelMaxDaqList; d++) {
        if(!ech->XcpEventChannelTriggeredDaqListRef[d])
            continue;
        Xcp_ProcessDaq(ech->XcpEventChannelTriggeredDaqListRef[d]);
    }
}
#endif

/**
 * Xcp_TxError sends an error message back to master
 * @param code is the error code requested
 */
static inline void Xcp_TxError(unsigned int code)
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
static inline void Xcp_TxSuccess()
{
    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        e->len = 1;
    }
}


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
        SET_UINT8 (e->data, 5, 0); /* MIN_ST */
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

	if(idType == 0 ){
		/* TODO Id Type: Implement ASCII text */
		RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_GetId - Id type 0 not supported\n");

	} else if(idType == 1){
		/* Id Type: ASAM-MC2 filename without path and extension */
	    Xcp_MtaInit((uint32)g_XcpFileName, 0);
		FIFO_GET_WRITE(g_XcpTxFifo, e) {
	        SET_UINT8  (e->data, 0, XCP_PID_RES);
	        SET_UINT8  (e->data, 1, 0); /* Mode TODO Check appropriate mode */
	        SET_UINT16 (e->data, 2, 0); /* Reserved */
	        SET_UINT32 (e->data, 4, strlen(g_XcpFileName)); /* Length */
	        e->len = 8;
	    }
		return E_OK;

	} else if(idType == 2 ){
		/* TODO: Id Type: Implement ASAM-MC2 filename with path and extension  */
		RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_GetId - Id type 2 not supported\n");

	} else if(idType == 3 ){
		/* TODO: Id Type: Implement URL where the ASAM-MC2 file can be found */
		RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_GetId - Id type 3 not supported\n");

	} else if(idType == 4 ){
		/* TODO: Id Type: ASAM-MC2 file to upload */
		RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_GetId - Id type 4 not supported\n");

	} else {
		/* Id Type doesn't exist */
		RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_GetId - Id type %d not supported\n", idType);

	}
}

Std_ReturnType Xcp_CmdUpload(uint8 pid, void* data, int len)
{
	DEBUG(DEBUG_HIGH, "Received upload\n");
	uint8 NumElem = GET_UINT8(data, 0); /* Number of Data Elements */

#ifdef XCP_FEATURE_BLOCKMODE
#warning "Block mode is not supported for UPLOAD"
	if(NumElem + 1 > XCP_MAX_CTO)
	    RETURN_ERROR(XCP_ERR_CMD_UNKNOWN, "Xcp_CmdUpload - Block mode not supported\n");
#endif

    FIFO_GET_WRITE(g_XcpTxFifo, e) {
        SET_UINT8 (e->data, 0, XCP_PID_RES);
        for(unsigned int i = 1 ; i <= NumElem ; i++){
        	SET_UINT8 (e->data, i, Xcp_MtaGet()); /*  */
        }
        e->len = NumElem + 1;
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
    if(!Xcp_MtaPut) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Xcp_Download - Mta not inited\n");
    }

    unsigned el_size = 1;
    unsigned el_len  = (int)GET_UINT8(data, 0) * el_size;
    unsigned el_offset;
    if(el_size > 2) {
        el_offset = el_size - 1;
    } else {
        el_offset = 1;
    }
    DEBUG(DEBUG_HIGH, "Received download %d, %d\n", pid, el_len);

#if(!XCP_FEATURE_BLOCKMODE)
    if(el_len + el_offset > XCP_MAX_CTO
    || el_len + el_offset > len) {
        RETURN_ERROR(XCP_ERR_OUT_OF_RANGE, "Xcp_Download - Invalid length (%d, %d, %d)\n", el_len, el_offset, len);
    }
#endif

    if(pid == XCP_PID_CMD_CAL_DOWNLOAD) {
        g_Download.len = el_len;
        g_Download.rem = el_len;
    }

    /* check for sequence error */
    if(g_Download.rem != el_len) {
        RETURN_ERROR(XCP_ERR_SEQUENCE, "Xcp_Download - Invalid next state (%d, %d)\n", el_len, g_Download.rem);
    }

    /* write what we got this packet */
    if(el_len > len - el_offset) {
        el_len = len - el_offset;
    }

    Xcp_MtaWrite((uint8*)data + el_offset, el_len);

    g_Download.rem -= el_len;

    if(g_Download.rem)
        return E_OK;

    RETURN_SUCCESS();
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
                             | 0 << 7 /* Identification_Field_Type_1 */);
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
		SET_UINT8  (e->data, 1, 1 << 0 /* PREDEFINED */
					 		  | 1 << 1 /* EVENT_FIXED */
							  | 1 << 2 /* DAQ */  /* TODO Error handling etc. */
							  | 0 << 3 /* STIM */ );
		SET_UINT8  (e->data, 2, daq->XcpMaxOdt); /* MAX_ODT */
		SET_UINT8  (e->data, 3, XCP_MAX_ODT_ENTRIES); /* MAX_ODT_ENTRIES */
		SET_UINT16 (e->data, 4, 0); /* FIXED_EVENT */ /*TODO EventChannel */
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
		SET_UINT8 (e->data, 6, 0xff); /* Event channel priority */
		e->len = 7;
	}
	return E_OK;
}

Std_ReturnType Xcp_CmdBuildChecksum(uint8 pid, void* data, int len)
{
    uint32 block = GET_UINT32(data, 3);

    DEBUG(DEBUG_HIGH, "Received build_checksum %ul\n", (unsigned int)block);

    uint8 type = 0x1; /* XCP_ADD_11 */
    uint8 res  = 0;
    for(int i = 0; i < block; i++) {
        res += Xcp_MtaGet();
    }

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
  , [XCP_PID_CMD_STD_SET_MTA]            = { .fun = Xcp_CmdSetMTA         , .len = 3 }
  , [XCP_PID_CMD_STD_SYNCH]              = { .fun = Xcp_CmdSync           , .len = 0 }
  , [XCP_PID_CMD_STD_GET_COMM_MODE_INFO] = { .fun = Xcp_CmdGetCommModeInfo, .len = 0 }
  , [XCP_PID_CMD_STD_BUILD_CHECKSUM]     = { .fun = Xcp_CmdBuildChecksum  , .len = 8 }

#if(XCP_FEATURE_CALPAG)
  , [XCP_PID_CMD_PAG_SET_CAL_PAGE]       = { .fun = Xcp_CmdSetCalPage     , .len = 4 }
  , [XCP_PID_CMD_PAG_GET_CAL_PAGE]       = { .fun = Xcp_CmdGetCalPage     , .len = 3 }
  , [XCP_PID_CMD_CAL_DOWNLOAD]           = { .fun = Xcp_CmdDownload       , .len = 3 }
#if(XCP_FEATURE_BLOCKMODE)
  , [XCP_PID_CMD_CAL_DOWNLOAD_NEXT]      = { .fun = Xcp_CmdDownload       , .len = 3 }
#endif
#endif // XCP_FEATURE_CALPAG

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
#if 0
    for(int c = 0; c < g_general.XcpMaxEventChannel; c++)
        Xcp_ProcessChannel(g_XcpConfig->XcpEventChannel+c);

    for(int d = 0; d < g_general.XcpDaqCount; d++)
        Xcp_ProcessDaq(g_XcpConfig->XcpDaqList+d);
#endif

    Xcp_Recieve_Main();
    Xcp_Transmit_Main();
}

