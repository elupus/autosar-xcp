//*****************************************************************************
/// @file        seedNKeyXcp.h
/// @author      Alex Lunkenheimer (ETAS/PAC-Lr)
/// @responsible author
/// @brief       DLL-Main for Seed'n Key for XCP
/// @history     2002-09-16,Lr: - draft
///              2003-02-07,Lr: - DLL-API using extern "C"
///                             - adapted privileges to XCP-Spec V1.0,Rev22
//*****************************************************************************
#ifndef SEED_N_KEY_XCP_H
#define SEED_N_KEY_XCP_H

//*****************************************************************************
//                               includes
//*****************************************************************************
#include <windows.h>

//*****************************************************************************
//                               typedefs
//*****************************************************************************
typedef DWORD TXcpSkExtFncRet;        // return code

//*****************************************************************************
//                               constants
//*****************************************************************************
// return codes
const TXcpSkExtFncRet XcpSkExtFncAck                      = 0; // o.k.
const TXcpSkExtFncRet XcpSkExtFncErrPrivilegeNotAvailable = 1; // the requested privilege can not be unlocked with this DLL
const TXcpSkExtFncRet XcpSkExtFncErrInvalidSeedLength     = 2; // the seed length is wrong, key could not be computed
const TXcpSkExtFncRet XcpSkExtFncErrUnsufficientKeyLength = 3; // the space for the key is too small
// privileges
const BYTE            XcpSkPrivCalPag                     = 0x01; // Calibration & Page Mngmnt privilege
const BYTE            XcpSkPrivDaq                        = 0x04; // Acquisition privilege
const BYTE            XcpSkPrivStim                       = 0x08; // Stimulation privilege
const BYTE            XcpSkPrivPgm                        = 0x10; // Programming privilege


//*****************************************************************************
//                               prototypes
//*****************************************************************************
EXTERN_C __declspec(dllexport) TXcpSkExtFncRet __cdecl XCP_GetAvailablePrivileges(BYTE *privilege);
EXTERN_C __declspec(dllexport) TXcpSkExtFncRet __cdecl XCP_ComputeKeyFromSeed(BYTE privilege, 
                                                       BYTE byteLenSeed, BYTE *seed,
                                                       BYTE *byteLenKey, BYTE *key);

#endif
