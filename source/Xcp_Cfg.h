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

#ifndef XCP_CFG_H_
#define XCP_CFG_H_

#include "Std_Types.h"

/* Compile switches */
#define XCP_DEV_ERROR_DETECT STD_OFF
#define Xcp_VERION_INFO_API  STD_OFF


/* Error codes */
#define XCP_E_INV_POINTER     0x01
#define XCP_E_NOT_INITIALIZED 0x02
#define XCP_E_INVALID_PDUID   0x03
#define XCP_E_INIT_FAILED     0x04 /* TODO assigned by DEM */
#define XCP_E_NULL_POINTER    0x12

/* Init Structure */
typedef struct {
} Xcp_ConfigType;



#endif /* XCP_CFG_H_ */

