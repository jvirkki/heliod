/*
** libdaemon.h
**
** Basic definitions for daemon module.
**
*/

#ifndef _LIBDAEMON_H_
#define _LIBDAEMON_H_

#ifdef XP_WIN32
// Need to force service-pack 3 extensions to be defined by 
// setting _WIN32_WINNT to NT 4.0 for winsock.h, winbase.h, winnt.h.  
// Ug.
#ifndef  _WIN32_WINNT
 #define _WIN32_WINNT 0x0400
#elif   (_WIN32_WINNT < 0x0400)
 #undef  _WIN32_WINNT
 #define _WIN32_WINNT 0x0400
#endif
#include <windows.h>
#include <winsock.h>
#endif

#include "prtypes.h"
#include "netsite.h"
#include "base/systems.h"
#include "base/ereport.h"
#include "frame/conf_api.h"

#ifdef XP_WIN32
#pragma warning (disable:4355)
#endif

#define streq(s1,s2) (strcmp((s1),(s2))==0)
#define strieq(s1,s2) (strcasecmp((s1),(s2))==0)
#define strneq(s1,s2,n) (strncmp((s1),(s2),(n))==0)

#ifdef XP_UNIX
#define max(n1,n2) ((n1)>(n2)?(n1):(n2))
#endif

#ifdef XP_WIN32
#ifdef BUILD_DLL
#define HTTPDAEMON_DLL _declspec(dllexport)
#else
#define HTTPDAEMON_DLL _declspec(dllimport)
#endif
#else
#define HTTPDAEMON_DLL
#endif

#endif /* _LIBDAEMON_H_ */
