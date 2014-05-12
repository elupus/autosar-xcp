//*****************************************************************************
/// @file        seedNKeyXcpWin.c
/// @author      Alex Lunkenheimer (ETAS/PAC-Lr)
/// @responsible author
/// @brief       DLL-Main for Seed'n Key for XCP
/// @history     2002-09-16,Lr: - draft
///              2003-02-07,Lr: - DLL-API using extern "C"
///                             - adapted privileges to XCP-Spec V1.0,Rev22
//*****************************************************************************

//*****************************************************************************
//                               includes
//*****************************************************************************
#include <assert.h>
#include "seedNKeyXcp.h"

//*****************************************************************************
//                               variables
//*****************************************************************************
static BYTE MyPrivilege = 0;

//*****************************************************************************
//                               prototypes
//*****************************************************************************
static TXcpSkExtFncRet computeKeyFromSeedCalPag(BYTE byteLenSeed, BYTE *seed, BYTE *key);
static TXcpSkExtFncRet computeKeyFromSeedDaq(BYTE byteLenSeed, BYTE *seed, BYTE *key);
static TXcpSkExtFncRet computeKeyFromSeedStim(BYTE byteLenSeed, BYTE *seed, BYTE *key);
static TXcpSkExtFncRet computeKeyFromSeedPgm(BYTE byteLenSeed, BYTE *seed, BYTE *key);
static void         setMyPrivilege();

//*****************************************************************************
/// @fn   DllMain ... Windows DLL entrance point
//*****************************************************************************
BOOL WINAPI DllMain (HANDLE hModule,  DWORD  fdwReason, LPVOID lpReserved)
{
   switch (fdwReason) {
      case DLL_PROCESS_ATTACH: setMyPrivilege(); break;
      case DLL_PROCESS_DETACH: break;
      default:                 break;
   } // switch

   return TRUE;
}

//*****************************************************************************
/// @fn   XCP_GetAvailablePrivileges ... set available privileges
//*****************************************************************************
EXTERN_C __declspec(dllexport) TXcpSkExtFncRet __cdecl XCP_GetAvailablePrivileges(BYTE *privilege)
{
   // check input parameter
   assert(privilege);

   // set available privileges
   *privilege  = MyPrivilege;

   return XcpSkExtFncAck;
}

//*****************************************************************************
/// @fn   XCP_ComputeKeyFromSeed ... compute key for privilege
//*****************************************************************************
EXTERN_C __declspec(dllexport) TXcpSkExtFncRet __cdecl XCP_ComputeKeyFromSeed(BYTE privilege, 
                                                       BYTE byteLenSeed, BYTE *seed,
                                                       BYTE *byteLenKey, BYTE *key)
{
   // check input parameter
   assert(seed);
   assert(byteLenKey);
   assert(key);
   if ( !(MyPrivilege & privilege) )
      return XcpSkExtFncErrPrivilegeNotAvailable;  // unsupported privilege request
   if ( *byteLenKey < byteLenSeed )
      return XcpSkExtFncErrUnsufficientKeyLength;  // check key length

   // compute key for respective privilege
   *byteLenKey = byteLenSeed; // assumption: same length for seed and key
   switch ( privilege ) {
   case  XcpSkPrivCalPag: return computeKeyFromSeedCalPag(byteLenSeed, seed, key);
   case  XcpSkPrivDaq:    return computeKeyFromSeedDaq(byteLenSeed, seed, key);
   case  XcpSkPrivStim:   return computeKeyFromSeedStim(byteLenSeed, seed, key);
   case  XcpSkPrivPgm:    return computeKeyFromSeedPgm(byteLenSeed, seed, key);
   default: return XcpSkExtFncErrPrivilegeNotAvailable;
   }

   return XcpSkExtFncErrPrivilegeNotAvailable;
}
//*****************************************************************************
/// @fn   computeKeyFromSeedCalPag ... algorithm for Calibration & Page Mngmnt
///                                    access
//*****************************************************************************
static TXcpSkExtFncRet computeKeyFromSeedCalPag(BYTE byteLenSeed, BYTE *seed, BYTE *key)
{
   BYTE i;

   // to do:   implement actual algorithm
   // example: key == seed 
   for ( i = 0; i < byteLenSeed; i++ )
      *key++ = *seed++;

   return XcpSkExtFncAck;
}
//*****************************************************************************
/// @fn   computeKeyFromSeedDaq ... algorithm for Acquisition access
//*****************************************************************************
static TXcpSkExtFncRet computeKeyFromSeedDaq(BYTE byteLenSeed, BYTE *seed, BYTE *key)
{
   BYTE i;

   // to do:   implement actual algorithm
   // example: key == seed 
   for ( i = 0; i < byteLenSeed; i++ )
      *key++ = *seed++;

   return XcpSkExtFncAck;
}
//*****************************************************************************
/// @fn   computeKeyFromSeedStim ... algorithm for Stimulation access
//*****************************************************************************
static TXcpSkExtFncRet computeKeyFromSeedStim(BYTE byteLenSeed, BYTE *seed, BYTE *key)
{
   BYTE i;

   // key[i] == seed[i] + 1
   for ( i = 0; i < byteLenSeed; i++ )
      *key++ = *seed++;

   return XcpSkExtFncAck;
}
//*****************************************************************************
/// @fn   computeKeyFromSeedPgm ... algorithm for Programming access
//*****************************************************************************
static TXcpSkExtFncRet computeKeyFromSeedPgm(BYTE byteLenSeed, BYTE *seed, BYTE *key)
{
   BYTE i;

   // to do:   implement actual algorithm
   // example: key == seed 
   for ( i = 0; i < byteLenSeed; i++ )
      *key++ = *seed++;

   return XcpSkExtFncAck;
}
//*****************************************************************************
/// @fn   setMyPrivilege ... set available privileges
//*****************************************************************************
static void setMyPrivilege()
{
   MyPrivilege = 0;
   // to do:   comment out unsupported privileges
   // example: MyPrivilege |= XcpSkPrivCalPag; -> //MyPrivilege |= XcpSkPrivCalPag;
   MyPrivilege |= XcpSkPrivCalPag;
   MyPrivilege |= XcpSkPrivDaq;
   MyPrivilege |= XcpSkPrivStim;
   MyPrivilege |= XcpSkPrivPgm;
}
