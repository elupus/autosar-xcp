
***************** XCP - Calibration and Measurement Protocol ******************

XCP is a Calibration and Measurement protocol for use in embedded systems. This
implementation is designed for integration in an AUTOSAR project. It follows
AUTOSAR 4.0 Xcp specification but support integration into an AUTOSAR 3.0
system.


 INTEGRATION
---------------

To integrate Xcp in a AUTOSAR project add the Xcp code files to your project as
a subdirectory. Make sure the subdirectory is in your C include path, as well
as that all the .c files are compiled and linked to your project. There is 
no complete makefile included in the project.
ArticCore:
    Add the Xcp.mk file as an include directive to your projects makefile
    this should make use of ArticCore build system to build Xcp as a part
    of your project.

    


    



 CONFIGURATION
---------------

All configuration of the module is done through the Xcp_Cfg.h and Xcp_Cfg.c file.
These files could be automatically generated from autosar xml files or manually 
configured.


Defines:
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
        Number of bytes used for transmitting timestamps (1;2;4). If clock
        has higher number of bytes, Xcp will wrap timestamps as the max
        byte size is reached.

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
    
    XCP_MAX_DAQ:
        * Applicable only when XCP_FEATURE_DAQSTIM_DYNAMIC = STD_OFF *
        Number of allocated/configured daqlists in configuration.
        Currently this needs to be known at compile time and is
        not part of the config (Will be removed in future)

    XCP_MAX_EVENT_CHANNEL:
        Number of allocated/configured event channels in configuration.
        Currently this needs to be known at compile time and is
        not part of the config (Will be removed in future)

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

    XCP_FEATURE_STIM
        Enabled use of STIM lists. Requires setup of event channels
        and the calling of event channels from code:
            Xcp_MainFunction_Channel()
    
    XCP_FEATURE_DIO
        Enabled direct read/write support using Online Calibration
        to AUTOSAR DIO ports using memory exstensions:
                0x2: DIO port
                0x3: DIO channel
        All ports are considered to be of sizeof(Dio_PortLevelType)
        bytes long. So port 5 is at memory address 5 * sizeof(Dio_PortLevelType)
        Channels are of BYTE length.

            
    XCP_MAX_DTO:
    XCP_MAX_CTO:

 CANAPE
--------













