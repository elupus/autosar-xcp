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

#include "Xcp_Cfg.h"

#include "Os.h"
#if(XCP_DEV_ERROR_DETECT)
#include "Dem.h"
#endif
#include "ComStack_Types.h"
#include "Std_Types.h"

#if(Xcp_VERION_INFO_API)
void Xcp_GetVersionInfo_Impl(Std_VersionInfoType* versioninfo)
{
#if(XCP_DEV_ERROR_DETECT)
	if(!versioninfo) {
		#error TODO error XCP_E_INV_POINTER
		return
	}
#endif
	#error TODO fill version info
}
#endif

void Xcp_Init(const Xcp_ConfigType* Xcp_ConfigPtr)
{
#if(XCP_DEV_ERROR_DETECT)
	if(!Xcp_ConfigPtr) {
		#error TODO error XCP_E_INV_POINTER
		return;
	}
#endif
}



/* Scheduled function of the XCP module */
void Xcp_MainFunction(void)
{
}




