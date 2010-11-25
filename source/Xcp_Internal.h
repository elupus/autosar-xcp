/* Copyright (C) 2010 Peter Fridlund
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


#ifndef XCP_INTERNAL_H_
#define XCP_INTERNAL_H_

#include "Xcp_Cfg.h"
#include "Xcp_ConfigTypes.h"
#include "Xcp_ByteStream.h"

#ifdef XCP_STANDALONE
#   define USE_DEBUG_PRINTF
#   define USE_LDEBUG_PRINTF
#   include <stdio.h>
#   include <assert.h>
#   include <string.h>
#   include "XcpStandalone.h"
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

#define XCP_PID_RES  							0xFF
#define XCP_PID_ERR  							0xFE
#define XCP_PID_EV								0xFD
#define XCP_PID_SERV 		   					0xFC


/*********************	  COMMANDS	  *****************/

/* STANDARD COMMANDS */
													/* OPTIONAL */
#define XCP_PID_CMD_STD_CONNECT 				0xFF	// N
#define XCP_PID_CMD_STD_DISCONNECT 				0xFE	// N
#define XCP_PID_CMD_STD_GET_STATUS				0xFD	// N
#define XCP_PID_CMD_STD_SYNCH					0xFC	// N
#define XCP_PID_CMD_STD_GET_COMM_MODE_INFO		0xFB	// Y
#define XCP_PID_CMD_STD_GET_ID					0xFA	// Y
#define XCP_PID_CMD_STD_SET_REQUEST				OxF9	// Y
#define XCP_PID_CMD_STD_GET_SEED				0xF8	// Y
#define XCP_PID_CMD_STD_UNLOCK					0xF7	// Y
#define XCP_PID_CMD_STD_SET_MTA					0xF6	// Y
#define XCP_PID_CMD_STD_UPLOAD					0xF5	// Y
#define XCP_PID_CMD_STD_SHORT_UPLOAD			0xF4	// Y
#define XCP_PID_CMD_STD_BUILD_CHECKSUM			0xF3	// Y
#define XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD 	0xF2	// Y
#define XCP_PID_CMD_STD_USER_CMD				0xF1	// Y

/* CALIBRATION COMMANDS */
													/* OPTIONAL */
#define XCP_PID_CMD_CAL_DOWNLOAD				0xF0	// N
#define XCP_PID_CMD_CAL_DOWNLOAD_NEXT			0xEF	// Y
#define XCP_PID_CMD_CAL_DOWNLOAD_MAX			0xEE	// Y
#define XCP_PID_CMD_CAL_SHORT_DOWNLOAD			0xED	// Y
#define XCP_PID_CMD_CAL_MODIFY_BITS				0xEC	// Y

/* PAGE SWITCHING COMMANDS */
													/* OPTIONAL */
#define XCP_PID_CMD_PAG_SET_CAL_PAGE			0xEB	// N
#define XCP_PID_CMD_PAG_GET_CAL_PAGE			0xEA	// N
#define XCP_PID_CMD_PAG_GET_PAG_PROCESSOR_INFO	0xE9	// Y
#define XCP_PID_CMD_PAG_GET_SEGMENT_INFO		0xE8	// Y
#define XCP_PID_CMD_PAG_GET_PAGE_INFO			0xE7	// Y
#define XCP_PID_CMD_PAG_SET_SEGMENT_MODE		0xE6	// Y
#define XCP_PID_CMD_PAG_GET_SEGMENT_MODE		0xE5	// Y
#define XCP_PID_CMD_PAG_COPY_CAL_PAGE			0xE4	// Y

/* DATA SCQUISITION AND STIMULATION COMMANDS */
													/* OPTIONAL */
#define XCP_PID_CMD_DAQ_CLEAR_DAQ_LIST			0xE3	// N
#define XCP_PID_CMD_DAQ_SET_DAQ_PTR				0xE2	// N
#define XCP_PID_CMD_DAQ_WRITE_DAQ				0xE1	// N
#define XCP_PID_CMD_DAQ_SET_DAQ_LIST_MODE		0xE0	// N
#define XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE		0xDF	// N
#define XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST		0xDE	// N
#define XCP_PID_CMD_DAQ_START_STOP_SYNCH		0xDD	// N
#define XCP_PID_CMD_DAQ_GET_DAQ_CLOCK			0xDC	// Y
#define XCP_PID_CMD_DAQ_READ_DAQ				0xDB	// Y
#define XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO	0xDA	// Y
#define XCP_PID_CMD_DAQ_GET_DAQ_RESOLUTION_INFO	0xD9	// Y
#define XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO		0xD8	// Y
#define XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO		0xD7	// Y
#define XCP_PID_CMD_DAQ_GET_FREE_DAQ			0xD6	// Y
#define XCP_PID_CMD_DAQ_GET_ALLOC_DAQ			0xD5	// Y
#define XCP_PID_CMD_DAQ_GET_ALLOC_ODT			0xD4	// Y
#define XCP_PID_CMD_DAQ_GET_ALLOC_ODT_ENTRY		0xD3	// Y

/* NON-VOLATILE MEMORY PROGRAMMING COMMANDS */
													/* OPTIONAL */
#define XCP_PID_CMD_PGM_PROGRAM_START			0xD2	// N
#define XCP_PID_CMD_PGM_PROGRAM_CLEAR			0xD1	// N
#define XCP_PID_CMD_PGM_PROGRAM					0xD0	// N
#define XCP_PID_CMD_PGM_PROGRAM_RESET			0xCF	// N
#define XCP_PID_CMD_PGM_GET_PGM_PROCESSOR_INFO	0xCE	// Y
#define XCP_PID_CMD_PGM_GET_SECTOR_INFO			0xCD	// Y
#define XCP_PID_CMD_PGM_PROGRAM_PREPARE			0xCC	// Y
#define XCP_PID_CMD_PGM_PROGRAM_FORMAT			0xCB	// Y
#define XCP_PID_CMD_PGM_PROGRAM_NEXT			0xCA	// Y
#define XCP_PID_CMD_PGM_PROGRAM_MAX				0xC9	// Y
#define XCP_PID_CMD_PGM_PROGRAM_VERIFY			0xC8	// Y


/* ERROR CODES */

#define XCP_ERR_CMD_SYNCH						0x00

#define XCP_ERR_CMD_BUSY						0x10
#define XCP_ERR_DAQ_ACTIVE						0x11
#define XCP_ERR_PGM_ACTIVE						0x12

#define XCP_ERR_CMD_UNKNOWN                     0x20
#define XCP_ERR_CMD_SYNTAX						0x21
#define XCP_ERR_OUT_OF_RANGE					0x22
#define XCP_ERR_WRITE_PROTECTED					0x23
#define XCP_ERR_ACCESS_DENIED					0x24
#define XCP_ERR_ACCESS_LOCKED					0x25
#define XCP_ERR_PAGE_NOT_VALID					0x26
#define XCP_ERR_MODE_NOT_VALID					0x27
#define XCP_ERR_SEGMENT_NOT_VALID				0x28
#define XCP_ERR_SEQUENCE						0x29
#define XCP_ERR_DAQ_CONFIG						0x2A

#define XCP_ERR_MEMORY_OVERFLOW					0x30
#define XCP_ERR_GENERIC							0x31
#define XCP_ERR_VERIFY							0x32

typedef enum  {
    XCP_CHECKSUM_ADD_11      = 0x01,
    XCP_CHECKSUM_ADD_12      = 0x02,
    XCP_CHECKSUM_ADD_14      = 0x03,
    XCP_CHECKSUM_ADD_22      = 0x04,
    XCP_CHECKSUM_ADD_24      = 0x05,
    XCP_CHECKSUM_ADD_44      = 0x06,
    XCP_CHECKSUM_CRC_16      = 0x07,
    XCP_CHECKSUM_CRC_16_CITT = 0x08,
    XCP_CHECKSUM_CRC_32      = 0x09,
    XCP_CHECKSUM_USERDEFINE  = 0xFF,
} Xcp_ChecksumType;

/* COMMAND LIST FUNCTION CALLBACK */

typedef Std_ReturnType (*Xcp_CmdFuncType)(uint8, void*, int);
typedef void           (*Xcp_CmdWorkType)(void);

typedef struct {
    Xcp_CmdFuncType fun; /**< pointer to function to use */
    int             len; /**< minimum length of command     */
} Xcp_CmdListType;


/* INTERNAL STRUCTURES */
typedef struct {
    int len; /**< Original download size */
    int rem; /**< Remaining download size */
} Xcp_DownloadType;

typedef struct {
    int len; /**< Original upload size */
    int rem; /**< Remaining upload size */
} Xcp_UploadType;

typedef struct {
    Xcp_OdtEntryType*   ptr;
    Xcp_OdtType*            odt;
    Xcp_DaqListType*    daqList;
} Xcp_DaqPtrStateType;

typedef struct {
    intptr_t address;
    uint8    extension;

    unsigned char (*get)();
    void          (*put)(unsigned char val);
    void          (*write)(uint8* data, int len);
    void          (*read) (uint8* data, int len);
} Xcp_MtaType;

/* INTERNAL GLOBAL VARIABLES */
extern const Xcp_ConfigType *g_XcpConfig;
extern Xcp_FifoType          g_XcpRxFifo;
extern Xcp_FifoType          g_XcpTxFifo;
extern Xcp_MtaType           Xcp_Mta;

/* MTA HELPER FUNCTIONS */
void Xcp_MtaInit(intptr_t address, uint8 extension);


/* PROGRAMMING COMMANDS */
Std_ReturnType Xcp_CmdProgramStart(uint8 pid, void* data, int len);
Std_ReturnType Xcp_CmdProgramClear(uint8 pid, void* data, int len);
Std_ReturnType Xcp_CmdProgram(uint8 pid, void* data, int len);
Std_ReturnType Xcp_CmdProgramReset(uint8 pid, void* data, int len);


/* CALLBACK FUNCTIONS */

extern void           Xcp_RxIndication(const void* data, int len);
extern Std_ReturnType Xcp_Transmit    (const void* data, int len);

extern void Xcp_TxError(unsigned int code);
extern void Xcp_TxSuccess();

/* HELPER DEFINES */
#define RETURN_ERROR(code, ...) do {      \
        DEBUG(DEBUG_HIGH,## __VA_ARGS__ ) \
        Xcp_TxError(code);                \
        return E_NOT_OK;                  \
    } while(0)

#define RETURN_SUCCESS() do { \
        Xcp_TxSuccess();      \
        return E_OK;          \
    } while(0)


#define XCP_ELEMENT_OFFSET(base) (XCP_ELEMENT_SIZE - 1 - ( (base)+XCP_ELEMENT_SIZE-1 ) % XCP_ELEMENT_SIZE )


#endif /* XCP_INTERNAL_H_ */
