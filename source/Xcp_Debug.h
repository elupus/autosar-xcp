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
