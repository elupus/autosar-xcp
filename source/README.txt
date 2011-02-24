
***************** XCP - Calibration and Measurement Protocol ******************

XCP is a Calibration and Measurement protocol for use in embedded systems. This
implementation is designed for integration in an AUTOSAR project. It follows
AUTOSAR 4.0 Xcp specification but support integration into an AUTOSAR 3.0
system.

The requirements on the AUTOSAR infrastructure is limited, thus creating
"emulation" functions to use the XCP module standalone is not a major 
hurdle for integration.


 INTEGRATION
---------------

To integrate Xcp in a AUTOSAR project add the Xcp code files to your project as
a subdirectory. Make sure the subdirectory is in your C include path, as well
as that all the .c files are compiled and linked to your project. There is 
no complete makefile included in the project.

The application must call Xcp_MainFunction() at fast
regular intervals to process incomming packets and send queued
packets out onto the actual transport protocol. [This will be
automatically taken care of when ArticCore get Xcp support
built in]

ArticCore:
    Add the Xcp.mk file as an include directive to your projects makefile
    this should make use of ArticCore build system to build Xcp as a part
    of your project. [Currently this will default to CAN interface]

Standalone:
    Somewhat more complicated since it requires creation of a few headers
    to emulate AUTOSAR functionality, aswell as hooking XCP up to a
    communication bus (ethernet or can)

    Xcp communicate with the actual protocol layer through two defined
    entry points Xcp_<protocol>RxIndication and <protocol>_Transmit, where
    <protocol> is either SoAdIf or CanIf depending on what underlying
    protocol is in use.
    
    For timestamp support the system also need to provide:
        StatusType GetCounterValue( CounterType, TickRefType );

    You also need to provide implementation for a global mutex locking:
        void XcpStandaloneLock();
        void XcpStandaloneUnlock();


 CONFIGURATION
---------------

All configuration of the module is done through the Xcp_Cfg.h and Xcp_Cfg.c file.
These files could be automatically generated from autosar xml files or manually 
configured.


Xcp_Cfg.h defines:
    XCP_PDU_ID_TX:
        The PDU id the Xcp submodule will use when transmitting data using
        CanIf or SoAd.

    XCP_PDU_ID_RX:
        The PDU id the Xcp submodule will expect data on when it's callbacks 
        are called from CanIf or SoAd.

    XCP_E_INIT_FAILED:
        Error code for a failed initialization. Should have been defined
        by DEM.

    XCP_COUNTER_ID:
        Counter id for the master clock Xcp will use when sending DAQ lists
        this will be used as an argument to AUTOSAR GetCounterValue.

    XCP_TIMESTAMP_SIZE:
        Number of bytes used for transmitting timestamps (0;1;2;4). If clock
        has higher number of bytes, Xcp will wrap timestamps as the max
        byte size is reached. Set to 0 to disable timestamp support

    XCP_IDENTIFICATION:
        Defines how ODT's are identified when DAQ lists are sent. Possible
        values are:
            XCP_IDENTIFICATION_ABSOLUTE:
                All ODT's in the slave have a unique number.
            XCP_IDENTIFICATION_RELATIVE_BYTE:
            XCP_IDENTIFICATION_RELATIVE_WORD:
            XCP_IDENTIFICATION_RELATIVE_WORD_ALIGNED:
                ODT's identification is relative to DAQ list id.
                Where the DAQ list is either byte or word sized.
                And possibly aligned to 16 byte borders.

        Since CAN has a limit of 8 bytes per packets, this will
        modify the limit on how much each ODT can contain.

    XCP_MAX_RXTX_QUEUE:
        Number of data packets the protocol can queue up for processing.
        This should include send buffer aswell as STIM packet buffers.
        This should at the minimum be set to
            1 recieve packet + 1 send packet + number of DTO objects that
            can be configured in STIM mode + allowed interleaved queue size.
    
    XCP_FEATURE_DAQSTIM_DYNAMIC: (STD_ON; STD_OFF)   [Default: STD_OFF]
        Enables dynamic configuration of DAQ lists instead of
        statically defining the number of lists aswell as their
        number of odts/entries at compile time.
    
    XCP_FEATURE_BLOCKMODE: (STD_ON; STD_OFF)   [Default: STD_OFF]
        Enables XCP blockmode transfers which speed up Online Calibration
        transfers.

    XCP_FEATURE_PGM: (STD_ON; STD_OFF)   [Default: STD_OFF]
        Enables the programming/flashing feature of Xcp
        (NOT IMPLEMENTED)

    XCP_FEATURE_CALPAG: (STD_ON; STD_OFF)   [Default: STD_OFF]
        Enabled page switching for Online Calibration
        (NOT IMPLEMENTED)

    XCP_FEATURE_DAQ: (STD_ON; STD_OFF)   [Default: STD_OFF]
        Enabled use of DAQ lists. Requires setup of event channels
        and the calling of event channels from code:
            Xcp_MainFunction_Channel()

    XCP_FEATURE_STIM (STD_ON; STD_OFF)  [Default: STD_OFF]
        Enabled use of STIM lists. Requires setup of event channels
        and the calling of event channels from code:
            Xcp_MainFunction_Channel()
    
    XCP_FEATURE_DIO (STD_ON; STD_OFF)   [Default: STD_OFF]
        Enabled direct read/write support using Online Calibration
        to AUTOSAR DIO ports using memory exstensions:
                0x2: DIO port
                0x3: DIO channel
        All ports are considered to be of sizeof(Dio_PortLevelType)
        bytes long. So port 5 is at memory address 5 * sizeof(Dio_PortLevelType)
        Channels are of BYTE length.


    XCP_MAX_DTO: [Default: CAN=8, IP=255]
    XCP_MAX_CTO: [Default: CAN=8, IP=255]
        Define the maximum size of a data/control packet. This will also
        directly affect memory consumptions for XCP since the code will
        always allocate XCP_MAX_DTO * XCP_MAX_RXTX_QUEUE bytes for
        data buffers.


Xcp_Cfg.c:
    Should define a complete Xcp_ConfigType structure that then
    will be passed to Xcp_Init().

    Example config with two event channels and dynamic DAQ lists
    follows below. The application should call Xcp_Mainfunction_Channel(0)
    once every 50 ms and Xcp_Mainfunction_Channel(1) once every second.


    ****************

        #define COUNTOF(a) (sizeof(a)/sizeof(*(a)))

        static Xcp_DaqListType* g_channels_daqlist[2][253];

        static Xcp_EventChannelType g_channels[2] = {
            {   .XcpEventChannelNumber              = 0
              , .XcpEventChannelMaxDaqList          = COUNTOF(g_channels_daqlist[0])
              , .XcpEventChannelTriggeredDaqListRef = g_channels_daqlist[0]
              , .XcpEventChannelName                = "Default 50MS"
              , .XcpEventChannelRate                = 50
              , .XcpEventChannelUnit                = XCP_TIMESTAMP_UNIT_1MS
              , .XcpEventChannelProperties          = 1 << 2 /* DAQ  */
                                                    | 0 << 3 /* STIM */
            },
            {   .XcpEventChannelNumber              = 1
              , .XcpEventChannelMaxDaqList          = COUNTOF(g_channels_daqlist[1])
              , .XcpEventChannelTriggeredDaqListRef = g_channels_daqlist[1]
              , .XcpEventChannelName                = "Default 1S"
              , .XcpEventChannelRate                = 1
              , .XcpEventChannelUnit                = XCP_TIMESTAMP_UNIT_1S
              , .XcpEventChannelProperties          = 1 << 2 /* DAQ  */
                                                    | 1 << 3 /* STIM */
            }
        };

        Xcp_ConfigType g_DefaultConfig = {
            .XcpEventChannel  = g_channels
          , .XcpSegment       = g_segments
          , .XcpInfo          = { .XcpMC2File = "XcpSer" }
          , .XcpMaxEventChannel = COUNTOF(g_channels)
          , .XcpMaxSegment      = COUNTOF(g_segments)

        };

    ****************

    

 CANAPE
--------
    Advanced settings:
        DAQ_COUNTER_HANDLING: Include command response
            Oddly CANAPE defaults to not expecting CTR value of tcp slave packets
            to be incremented by RES packets and the like which the specification
            says they should be. Atleast they allow you to follow spec.
        DAQ_PRESCALER_SUPPORTED: Yes
            Implementation support prescaler












