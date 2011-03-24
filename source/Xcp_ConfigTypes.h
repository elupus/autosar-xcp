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

#ifndef XCP_CONFIGTYPES_H_
#define XCP_CONFIGTYPES_H_

#include "Std_Types.h"

typedef enum {
    XCP_TIMESTAMP_UNIT_1NS   = 0x00,
    XCP_TIMESTAMP_UNIT_10NS  = 0x01,
    XCP_TIMESTAMP_UNIT_100NS = 0x02,
    XCP_TIMESTAMP_UNIT_1US   = 0x03,
    XCP_TIMESTAMP_UNIT_10US  = 0x04,
    XCP_TIMESTAMP_UNIT_100US = 0x05,
    XCP_TIMESTAMP_UNIT_1MS   = 0x06,
    XCP_TIMESTAMP_UNIT_10MS  = 0x07,
    XCP_TIMESTAMP_UNIT_100MS = 0x08,
    XCP_TIMESTAMP_UNIT_1S    = 0x09,
} Xcp_TimestampUnitType;

#define XCP_IDENTIFICATION_ABSOLUTE              0x0
#define XCP_IDENTIFICATION_RELATIVE_BYTE         0x1
#define XCP_IDENTIFICATION_RELATIVE_WORD         0x2
#define XCP_IDENTIFICATION_RELATIVE_WORD_ALIGNED 0x3

#define XCP_PROTOCOL_TCP     0x1
#define XCP_PROTOCOL_UDP     0x2
#define XCP_PROTOCOL_CAN     0x3
#define XCP_PROTOCOL_USB     0x4
#define XCP_PROTOCOL_FLEXRAY 0x5

typedef struct {
    const uint16 XcpRxPduId;
    void*        XcpRxPduRef;
} Xcp_RxPduType;

typedef struct {
    const uint16 XcpTxConfirmationPduId;
} Xcp_TxConfirmationType;

typedef struct {
    const uint16            XcpTxPduId;
    void*                   XcpTxPduRef;
    Xcp_TxConfirmationType* XcpTxConfirmation;
} Xcp_TxPduType;

typedef struct {
    const Xcp_RxPduType* XcpRxPdu;
    const Xcp_TxPduType* XcpTxPdu;
} Xcp_PduType;

typedef struct {
          uint8        XcpDtoPid;         /* 0 .. 251 */
    const Xcp_PduType* XcpDto2PduMapping; /* XcpRxPdu, XcpTxPdu */
} Xcp_DtoType;

typedef struct Xcp_OdtEntryType {
            intptr_t    XcpOdtEntryAddress;
            uint8       XcpOdtEntryLength;
            uint8       XcpOdtEntryNumber; /* 0 .. 254 */

    /* Implementation defined */

     struct Xcp_OdtEntryType *XcpNextOdtEntry;
            uint8       BitOffSet;
            uint8       XcpOdtEntryExtension;
} Xcp_OdtEntryType;

struct Xcp_BufferType;

typedef struct Xcp_OdtType {
          uint8             XcpMaxOdtEntries;   /* XCP_MAX_ODT_ENTRIES */
          uint8             XcpOdtEntriesCount; /* 0 .. 255 */
          uint8             XcpOdtEntryMaxSize; /* 0 .. 254 */
          uint8             XcpOdtNumber;       /* 0 .. 251 */
          Xcp_DtoType       XcpOdt2DtoMapping;
          Xcp_OdtEntryType* XcpOdtEntry;        /* 0 .. * */

    /* Implementation defined */
          int               XcpOdtEntriesValid; /* Number of non zero entries */
   struct Xcp_OdtType      *XcpNextOdt;
   struct Xcp_BufferType   *XcpStim;
} Xcp_OdtType;

typedef enum {
    XCP_DAQLIST_MODE_SELECTED  = 1 << 0,
    XCP_DAQLIST_MODE_STIM      = 1 << 1,
    XCP_DAQLIST_MODE_TIMESTAMP = 1 << 4,
    XCP_DAQLIST_MODE_PIDOFF    = 1 << 5,
    XCP_DAQLIST_MODE_RUNNING   = 1 << 6,
    XCP_DAQLIST_MODE_RESUME    = 1 << 7,
} Xcp_DaqListModeEnum;

typedef enum {
    XCP_DAQLIST_PROPERTY_PREDEFINED  = 1 << 0,
    XCP_DAQLIST_PROPERTY_EVENTFIXED  = 1 << 1,
    XCP_DAQLIST_PROPERTY_DAQ         = 1 << 2,
    XCP_DAQLIST_PROPERTY_STIM        = 1 << 3
} Xcp_DaqListPropertyEnum;

typedef struct {
          Xcp_DaqListModeEnum     Mode;           /**< bitfield for the current mode of the DAQ list */
          uint16                  EventChannel;   /*TODO: Fixed channel vs current */
          uint8                   Prescaler;      /* */
          uint8                   Priority;       /* */
          Xcp_DaqListPropertyEnum Properties;     /**< bitfield for the properties of the DAQ list */
} Xcp_DaqListParams;



typedef struct Xcp_DaqListType {
    /**
     * Index number of DAQ list
     *   [INTERNAL]
     *
     * 0 .. 65534
     */
            uint16               XcpDaqListNumber; /* 0 .. 65534 */

     /**
      * Maximum number of ODT's in XcpOdt array
      *   [USER]    : When static DAQ configuration
      *   [INTERNAL]: Dynamic DAQ configuration.
      * 0 .. 252
      */
            uint8                XcpMaxOdt;

     /**
      * Number of currently configured ODT's
      *   [USER]    : If you have predefined DAQ lists
      *   [INTERNAL]: If daq lists are configured by master
      */
            uint8                XcpOdtCount;      /* 0 .. 252 */

     /**
      * Pointer to an array of ODT structures this DAQ list will use
      *   [USER]: With static DAQ lists, this needs to be set
      *           to an array of XcpMaxOdt size.
      *   [INTERNAL]: With dynamic DAQ configuration.
      */
            Xcp_OdtType         *XcpOdt;           /**< reference to an array of Odt's configured for this Daq list */


     /**
      * Holds parameters for the DAQ list
      *   [INTERNAL/USER]
      *   TODO: Move the parameters into the DAQ list structure instead
      */
            Xcp_DaqListParams    XcpParams;

     /**
      * Pointer to next allocated DAQ list
      *   [INTERNAL]
      */
     struct Xcp_DaqListType     *XcpNextDaq;
} Xcp_DaqListType;

typedef struct {
} Xcp_DemEventParameterRefs;

typedef enum {
    XCP_EVENTCHANNEL_PROPERTY_DAQ         = 1 << 2,
    XCP_EVENTCHANNEL_PROPERTY_STIM        = 1 << 3,
    XCP_EVENTCHANNEL_PROPERTY_ALL         = XCP_EVENTCHANNEL_PROPERTY_DAQ
                                          | XCP_EVENTCHANNEL_PROPERTY_STIM,
} Xcp_EventChannelPropertyEnum;

typedef struct {
    /**
     * Event channel number.
     *   [USER]
     *
     * Should match the order in the array of event channels
     */
    const  uint16            			XcpEventChannelNumber;     /**< 0 .. 65534 */

    /**
     * Priority of event channel (0 .. 255)
     *   [IGNORED]
     */
    const  uint8             			XcpEventChannelPriority;

    /**
     * Maximum number of entries in XcpEventChannelTriggeredDaqListRef
     *   [USER]
     *
     * 1 .. 255
     * 0 = Unlimited
     */
    const  uint8                        XcpEventChannelMaxDaqList;

    /**
     * Pointer to an array of pointers to daqlists
     *   [USER]
     */
           Xcp_DaqListType** 			XcpEventChannelTriggeredDaqListRef;

    /**
     * Set to the name of the eventchannel or NULL
     *   [USER]
     */
    const  char*                        XcpEventChannelName;


    /**
     * Bitfield defining supported features
     *   [USER]
     */
    const  Xcp_EventChannelPropertyEnum	XcpEventChannelProperties;


    /**
     * Cycle unit of event channel
     *   [USER]
     *
     * Set to 0 (XCP_TIMESTAMP_UNIT_1NS) if channel is not
     * cyclic.
     */
    const Xcp_TimestampUnitType 		XcpEventChannelUnit;

    /**
     * Number of cycle units between each trigger of event
     *   [USER]
     *
     * 0 .. 255
     * 0 -> non cyclic
     */
    const uint8                 		XcpEventChannelRate;

    /**
     * Counter used to implement prescaling
     *   [INTERNAL]
     */
          uint8                         XcpEventChannelCounter;

    /**
     * Number of daq lists currently assigned to event channel
     *   [INTERNAL]
     */
          uint8                         XcpEventChannelDaqCount;

} Xcp_EventChannelType;

enum {
    XCP_ACCESS_ECU_ACCESS_WITHOUT_XCP       = 1 << 0,
    XCP_ACCESS_ECU_ACCESS_WITH_XCP          = 1 << 1,
    XCP_ACCESS_XCP_READ_ACCESS_WITHOUT_ECU  = 1 << 2,
    XCP_ACCESS_READ_ACCESS_WITH_ECU         = 1 << 3,
    XCP_ACCESS_XCP_WRITE_ACCESS_WITHOUT_ECU = 1 << 4,
    XCP_ACCESS_XCP_WRITE_ACCESS_WITH_ECU    = 1 << 5,
    XCP_ACCESS_ALL                          = 0x3f
} Xcp_AccessFlagsType;

typedef enum {
    XCP_PROTECT_NONE   = 0,
    XCP_PROTECT_CALPAG = 1 << 0,
    XCP_PROTECT_DAQ    = 1 << 2,
    XCP_PROTECT_STIM   = 1 << 3,
    XCP_PROTECT_PGM    = 1 << 4,
} Xcp_ProtectType;

typedef enum {
    XCP_COMPRESSION_METHOD_NONE = 0,
} Xcp_CompressType;

typedef enum {
    XCP_ENCRYPTION_METHOD_NONE = 0,
} Xcp_EncryptionType;

typedef struct {
    intptr_t  XcpSrc;
    intptr_t  XcpDst;
    uint32    XcpLen;
} Xcp_MemoryMappingType;

typedef struct {
    const uint8            XcpAccessFlags;
    uint8                  XcpMaxPage;
    uint8                  XcpPageXcp;
    uint8                  XcpPageEcu;

    intptr_t               XcpAddress;
    uint32                 XcpLength;
    uint8                  XcpExtension;

    Xcp_CompressType       XcpCompression;
    Xcp_EncryptionType     XcpEncryption;

    uint32                 XcpMaxMapping;
    Xcp_MemoryMappingType* XcpMapping;
} Xcp_SegmentType;

typedef struct {
    const char* XcpCaption;   /**< ASCII text describing device [USER] */
    const char* XcpMC2File;   /**< ASAM-MC2 filename without path and extension [USER] */
    const char* XcpMC2Path;   /**< ASAM-MC2 filename with path and extension [USER] */
    const char* XcpMC2Url;    /**< ASAM-MC2 url to file [USER] */
    const char* XcpMC2Upload; /**< ASAM-MC2 file to upload [USER] */
} Xcp_InfoType;

typedef struct {
          Xcp_DaqListType           *XcpDaqList;
    const Xcp_DemEventParameterRefs *XcpDemEventParameterRef;
          Xcp_EventChannelType      *XcpEventChannel;
    const Xcp_PduType               *XcpPdu;

    /* Implementation defined */

          Xcp_SegmentType           *XcpSegment;
    const uint16                     XcpMaxSegment;

    const Xcp_InfoType               XcpInfo;

          uint16                     XcpMaxDaq;             /* 0 .. 65535, XCP_MAX_DAQ */
    const uint16                     XcpMaxEventChannel;    /* 0 .. 65535, XCP_MAX_EVENT_CHANNEL */
    const uint16                     XcpMinDaq;             /* 0 .. 255  , XCP_MIN_DAQ */

          Xcp_ProtectType            XcpProtect;            /**< Bitfield with currently locked features (Xcp_ProtectType) */

          /**
           * Function used for Seed & Key unlock
           * @param res is the resource requested to be unlocked
           * @param seed is the seed that was sent to the master
           * @param seed_len is the length of @param seed
           * @param key is the key sent from master
           * @param key_len is the length of @param key
           * @return E_OK for success, E_ERR for failure
           */
          Std_ReturnType            (*XcpUnlockFn)( Xcp_ProtectType res
                                                  , const uint8* seed
                                                  ,       uint8  seed_len
                                                  , const uint8* key
                                                  ,       uint8  key_len );
          /**
           * Function called to retrieve seed that should be
           * sent to master in a GetSeed/Unlock exchange
           * @param res is the resource requested
           * @param seed pointer to buffer that will hold seed (255 bytes max)
           * @return number of bytes in seed
           */
          uint8                     (*XcpSeedFn)  ( Xcp_ProtectType res, uint8* seed );

          /**
           * Function called for a XCP user defined call from the master
           * @param data data recieved from master (excluding the preceding 0xF1 for user defined)
           * @param len  length of the data in buffer
           * @return
           */
          Std_ReturnType            (*XcpUserFn)  (void* data, int len);
} Xcp_ConfigType;

#endif /* XCP_CONFIGTYPES_H_ */
