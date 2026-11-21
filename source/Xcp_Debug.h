/* Copyright (C) 2026 Joakim Plate
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

/* Debug output from the XCP module
 *
 * XCP_DEBUG(format, ...) takes printf style arguments and prints nothing
 * unless XCP_DEBUG_FN is defined to a printf style function or macro,
 * for example in Xcp_Cfg.h:
 *
 *   #define XCP_DEBUG_FN printf
 *
 * When building with XCP_STANDALONE it defaults to printf.
 */

#ifndef XCP_DEBUG_H_
#define XCP_DEBUG_H_

#if !defined(XCP_DEBUG_FN) && defined(XCP_STANDALONE)
#   include <stdio.h>
#   define XCP_DEBUG_FN printf
#endif

#ifdef XCP_DEBUG_FN
#   define XCP_DEBUG(...) XCP_DEBUG_FN(__VA_ARGS__)
#else
#   define XCP_DEBUG(...) do { } while(0)
#endif

#endif /* XCP_DEBUG_H_ */
