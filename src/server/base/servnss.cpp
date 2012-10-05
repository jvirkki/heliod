/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * nssinit.cpp: Initialize NSS (SSL) for a server
 * 
 * This was originally servssl.c.  This has been cleaned up for 4.x and 
 * moved to netsite/httpd/src
 * 
 * As needed stolen code originally written by Rob McCool
 * Written by Robin Maxwell
 */

#include <stdarg.h>

#include "nspr.h"
#include "pk11func.h"

#include "generated/ServerXMLSchema/Token.h"
#include "base/util.h"
#include "base/ereport.h"
#include "base/servnss.h"
#include "base/dbtbase.h"
#include "frame/conf.h"
#include "frame/conf_api.h"
#include "libaccess/nsauth.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/WatchdogClient.h"
#include "httpdaemon/updatecrl.h"

using ServerXMLSchema::Pkcs11;
using ServerXMLSchema::Token;
using ServerXMLSchema::SslSessionCache;

static const char INTERNAL_TOKEN_NAME[] = "internal                         ";

#ifdef XP_WIN32
//
// on NT, the server pops up a dialog box to get the server passwords
//
#include <windows.h>
#include "nt/resource.h"
#include "nt/regparms.h"
#include "nt/ntwdog.h"
#include "uniquename.h"
/* service_name was moved here from various nt*.c modules */
/* making this external here causes undefined external errors in cgi */
/* programs that include this lib. */
static char *service_name = NULL;  /* moved from ntmain.c -ahakim WatchDog */
static char ntpassword[512];
void static CenterDialog(HWND hwndParent, HWND hwndDialog);
BOOL CALLBACK PasswordDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

typedef struct passwordHolder pHolder;

struct passwordHolder {
   char prompt[512];
   char password[512];
};

pHolder pwdHolder[24];
int tokens[2];
#endif

struct TokenAuthContext {
    const Pkcs11 *pkcs11;
    const Token *token;
    const char *token_name;
    int retries;
};

static const char *servssl_prompt;
static PRFileDesc *servssl_console;
static PRBool servssl_init_early_called;
static PRBool servssl_init_late_called;

#ifdef XP_UNIX

#include <termios.h>
static void echoOff(int fd)
{
    if (isatty(fd)) {
	struct termios tio;
	tcgetattr(fd, &tio);
	tio.c_lflag &= ~ECHO;
	tcsetattr(fd, TCSAFLUSH, &tio);
    }
}

static void echoOn(int fd)
{
    if (isatty(fd)) {
	struct termios tio;
	tcgetattr(fd, &tio);
	tio.c_lflag |= ECHO;
	tcsetattr(fd, TCSAFLUSH, &tio);
    }
}

static char *SEC_GetPassword(PRBool retry)
{
    char phrase[200];
    int infd = fileno(stdin);
    int isTTY = isatty(infd);

    /* Got a watchdog process keeping passwords for us? */
    if (conf_get_true_globals()->started_by_watchdog) {
	PR_ASSERT(0);	/* should not get here */
    }

    for (;;) {
        /* Prompt for password */
        if (isTTY) {
            PR_fprintf(servssl_console, "%s", servssl_prompt);
            echoOff(infd);
        }
        fgets(phrase, sizeof(phrase), stdin);
        if (isTTY) {
            PR_fprintf(servssl_console, "\n");
            echoOn(infd);
        }

        /* stomp on newline */
        int len = strlen(phrase);
        if (len > 0)
            phrase[len - 1] = 0;

        return PORT_Strdup(phrase);
    }
}
#endif	/* XP_UNIX */

#ifdef XP_WIN32
char *_GetPasswordFromSharedMemory(const char *tokenName)
{
    BOOL bReturn = FALSE;
    HANDLE hMapObject = NULL;
    LPSTR szSharedPwd = NULL;
    char szObjName[MAX_PATH];
    char *tok = NULL;
    char retVal[512]="";
    int len = 0;
	PRBool found = PR_FALSE;

    len = strlen(tokenName);

    wsprintf(szObjName, "pwd-%s", get_uniquename());

    // open system-global named file mapping object (created by CGIs start.c, restart.c)

    if(hMapObject = OpenFileMapping(FILE_MAP_WRITE, FALSE, szObjName))
    {
        // get pointer to szSharedPwd
        if(szSharedPwd = (char *)MapViewOfFile(hMapObject, FILE_MAP_WRITE, 0, 0, 0))
        {
			char* hastoken= strstr(szSharedPwd, tokenName);
			if (hastoken)
			{
				hastoken = PERM_STRDUP(hastoken);
				strtok(hastoken, "\n");
				char* haspassword = strchr(hastoken, ':');
				if (haspassword)
				{
					strcpy(retVal, haspassword+1);
					found = PR_TRUE;
				}
				PERM_FREE(hastoken);
			}
			UnmapViewOfFile(szSharedPwd);
		}
        bReturn = TRUE;
        CloseHandle(hMapObject);
    }
	if (PR_TRUE == found)
		return PORT_Strdup(retVal);
	else
		return NULL;
}
#endif

static const Token *servssl_gettoken(const Pkcs11 *pkcs11, const char *name)
{
    for (int i = 0; i < pkcs11->getTokenCount(); i++) {
        const Token *token = pkcs11->getToken(i);
        if (!strcmp(token->name, name))
            return token;
    }

    return NULL;
}

//
// this function gets one password for one token
//
static char *servssl_getpwd(PK11SlotInfo* slot, PRBool retry, TokenAuthContext *context)
{
    char *pwdstr = NULL;
    
    // try to get the passwords from the password file
    // THIS IS UNSAFE! the password that gives access to the private keys
    // should NEVER EVER be stored somewhere on the disk
    if (!retry) {
        if (context->token && context->token->getPin())
            pwdstr = PORT_Strdup(*context->token->getPin());
    }
    // got a password? we're done then.
    if (pwdstr)
	return pwdstr;

#ifdef XP_UNIX
    // Got a watchdog process keeping passwords for us?
    if (conf_get_true_globals()->started_by_watchdog) {
	// yes. try to get it from the watchdog
        if (WatchdogClient::getPassword(servssl_prompt, context->retries, &pwdstr) != PR_SUCCESS) {
	    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSnoPassfromWD));
	    return NULL;
	}
        return pwdstr;
    } else {
	// get it from a terminal
	return (char *)SEC_GetPassword(retry);
    }
#endif /* XP_UNIX */

#ifdef XP_WIN32
    int rv;
    HWND hwndRemote;
    HANDLE hRemoteProcess;
    DWORD retlen;
    int gwladdr;
    int gwllen;
    int gwlcount;
    int itemlen;
    char *itemptr;
    int *tokenptr;
    int pos = -1 ;
    char *mPwdstr = NULL;

    ntpassword[0] = '\0';

    mPwdstr = _GetPasswordFromSharedMemory(context->token_name);
    if (mPwdstr && *mPwdstr)
	return mPwdstr;

    /* Check whether we're allowed to interact with the desktop */
    USEROBJECTFLAGS uof;
    if (GetUserObjectInformation(GetProcessWindowStation(), UOI_FLAGS, &uof, sizeof(uof), NULL)) {
        if (!(uof.dwFlags & WSF_VISIBLE)) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSPINDlgError));
            return NULL;
        }
    }

    /* Try to get the password or PIN from the NT watchdog */

    /* find WatchDog application window */
    hwndRemote = FindWindow(WC_WATCHDOG, get_uniquename());
    if (hwndRemote) {

	/* Get the process handle for the watchdog process */
	hRemoteProcess = (void *)GetWindowLong(hwndRemote,
					       GWL_PROCESS_HANDLE);
	if (hRemoteProcess) {

	    /* Set up to retrieve either password or PIN information */
	    gwllen = GWL_PASSWORD_LENGTH;
	    gwladdr = GWL_PASSWORD_ADDR;
	    gwlcount = GWL_TOKEN_COUNT;

	    itemlen = sizeof(ntpassword);

	    /* Get the watchdog process memory address of the item */
	    itemptr = (char *)GetWindowLong(hwndRemote, gwladdr);
	    tokenptr = (int *)GetWindowLong(hwndRemote,  gwlcount);

	    if (itemptr) {

		/* Read the item from the watchdog's memory */
		rv = ReadProcessMemory(hRemoteProcess, itemptr, pwdHolder,
				       sizeof(pwdHolder), &retlen);

		rv = ReadProcessMemory(hRemoteProcess, tokenptr, tokens,
				       sizeof(tokens), &retlen);
		if (rv) {

		    for (int i=0; i < tokens[0]; i++) {
			if (!strcmp(pwdHolder[i].prompt, context->token_name) ) {
			    if(retry)
			       memset(pwdHolder[i].password, 0, sizeof(pwdHolder[i].password));
			    strcpy(ntpassword, pwdHolder[i].password);
			    pos = i;
			    break;
			}
		    }
			    

		    if (!rv) {
			ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSPINWriteProcessMemoryError));
		    }
		}
	    }
	}
    }

    if (pos == -1) {
        if (!pwdHolder[tokens[0]].password[0]) {
            rv = DialogBox(GetModuleHandle(NULL),
                           MAKEINTRESOURCE(IDD_PASSWORD), HWND_DESKTOP,
                           (DLGPROC)PasswordDialogProc);

            if (rv != IDOK && rv != IDCANCEL) {
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSPINDlgError));
                return NULL;
            }
        }
    }
    else {
        if (retry) {
            rv = DialogBox(GetModuleHandle(NULL),
                           MAKEINTRESOURCE(IDD_PASSWORD), HWND_DESKTOP,
                           (DLGPROC)PasswordDialogProc);

            if (rv != IDOK && rv != IDCANCEL) {
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSPINDlgError));
                return NULL;
            }
        }
    }
    if (ntpassword[0]) {
        pwdstr = PORT_Strdup(ntpassword);

        strcpy(pwdHolder[tokens[0]].password, pwdstr);
        strcpy(pwdHolder[tokens[0]].prompt, context->token_name);

	rv = WriteProcessMemory(hRemoteProcess, itemptr,
			        pwdHolder, sizeof(pwdHolder), &retlen);
	if (pos == -1)
	  tokens[0] = tokens[0] + 1;
	rv = WriteProcessMemory(hRemoteProcess, tokenptr,
				tokens, sizeof(tokens), &retlen);

        if (!rv) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSPINWriteProcessMemoryError));
        }
    }
    return pwdstr;
#endif /* XP_WIN32 */
}

#ifdef XP_WIN32
BOOL _ClearOutSharedMemory()
{
    BOOL bReturn = FALSE;
    HANDLE hMapObject = NULL;
    LPSTR szSharedPwd = NULL;
    char szObjName[MAX_PATH];

    wsprintf(szObjName, "pwd-%s", get_uniquename());

    // open system-global named file mapping object (created by CGIs start.c, restart.c)

    if(hMapObject = OpenFileMapping(FILE_MAP_WRITE, FALSE, szObjName))
    {
        // get pointer to szSharedPwd
        if(szSharedPwd = (char *)MapViewOfFile(hMapObject, FILE_MAP_WRITE, 0, 0, 0))
        {
	  ZeroMemory(szSharedPwd, MAX_PATH);
          UnmapViewOfFile(szSharedPwd);
	  bReturn = TRUE;
        }
        CloseHandle(hMapObject);
    }
    return bReturn;
}
#endif

// NSSNoPassword - this is a password callback that always returns NULL
// it is used after the server is initialized, so that it doesn't prompt the user for
// passwords again
    
static char *NSSNoPassword(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    return NULL;
}

// NSSGetPasswd - this function is set up so that NSS calls it whenever it feels it needs
// a password. It handles retrying correctly.
static char *NSSGetPasswd(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    TokenAuthContext *context = (TokenAuthContext *) arg;

    if (retry) {
        context->retries++;
        if (context->retries > 2)
            return NULL; // abort after 2 retries (3 failed attempts)
    }

    NSString buf;
#ifndef XP_WIN32
    if (retry) {
        buf.printf(XP_GetAdminStr(DBT_tokenXPINIncorrect), context->token_name);
        buf.append('\n');
    }
#endif
    buf.printf(XP_GetAdminStr(DBT_tokenXPINPrompt), context->token_name);

    servssl_prompt = buf;

    char *passwd = servssl_getpwd(slot, retry, context);

    servssl_prompt = NULL;

    return passwd; // NSS will call PORT_Free() to free this
}

static SECStatus _nss_InitTokens(const Pkcs11& pkcs11)
{
    SECStatus rv = SECSuccess;

    // set NSSGetPasswd to be the function to call when NSS needs a password
    PK11_SetPasswordFunc(NSSGetPasswd);

    PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_TRUE, NULL);
    PK11SlotListElement *element;

    for (element = PK11_GetFirstSafe(list); element; element = element->next) {
        PK11SlotInfo *slot = element->slot;

        char *token_name = PK11_GetTokenName(slot);

        const Token *token = servssl_gettoken(&pkcs11, token_name);
        if (token && !token->enabled) {
            ereport(LOG_VERBOSE, "Skipping disabled \"%s\" PKCS #11 token", token_name);
            continue;
        }

        if (PK11_NeedLogin(slot) && PK11_NeedUserInit(slot)) {
            if (slot == PK11_GetInternalKeySlot()) {
                ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_servNSSdbnopassword));
            } else {
                ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_servNSStokenuninitialized), token_name);
            }
            continue;
        }

        ereport(LOG_VERBOSE, "Initializing \"%s\" PKCS #11 token", token_name);

        struct TokenAuthContext context;
        context.pkcs11 = &pkcs11;
        context.token = token;
        context.token_name = token_name;
        context.retries = 0;

        if (PK11_Authenticate(slot, PR_TRUE, &context) != SECSuccess) {
            rv = SECFailure;
            break;
        }
    }

    // reset NSS password callback to blank, so that the server won't prompt again after init
    PK11_SetPasswordFunc(NSSNoPassword);

    return rv;
}

void PR_CALLBACK servssl_handshake_callback(PRFileDesc *socket, void *arg)
{
    // do nothing
}


//
// NSS & SSL-related initialization which is done prior to forking.
//
PRStatus servssl_init_early(const Pkcs11& pkcs11, 
                            const SslSessionCache& sslSessionCache)
{
    PR_ASSERT(!servssl_init_early_called);
    servssl_init_early_called = PR_TRUE;

    conf_global_vars_s *globals = conf_get_true_globals();

    // security_active indicates whether we should initialize tokens (this
    // includes unlocking the internal trust database)
    PR_ASSERT(globals->Vsecurity_active == pkcs11.enabled);

    // Get SSL session cache configuration
    if (sslSessionCache.enabled) {
        globals->Vssl_cache_entries = sslSessionCache.maxEntries;
        globals->Vsecurity_session_timeout = sslSessionCache.maxSsl2SessionAge;
        globals->Vssl3_session_timeout = sslSessionCache.maxSsl3TlsSessionAge;
    } else {
        PR_ASSERT(globals->Vssl_cache_entries == 0);
    }

    // Configure the multiprocess session cache before the fork
    if (globals->Vpool_max != 1) {
        if (globals->Vssl_cache_entries > 0) {
            SSL_ConfigMPServerSIDCache(globals->Vssl_cache_entries,
                                       globals->Vsecurity_session_timeout,
                                       globals->Vssl3_session_timeout,
                                       system_get_temp_dir());
            ereport(LOG_VERBOSE, "SSL/TLS session cache is enabled "
                    "(entries=%d, "
                    "ssl2 session timeout=%d, ssl3tls session timeout=%d)",
                    globals->Vssl_cache_entries,
                    globals->Vsecurity_session_timeout,
                    globals->Vssl3_session_timeout);
        }
    }

    return PR_SUCCESS;
}


//
// NSS & SSL-related done in child process(es) after forking.
//
PRStatus servssl_init_late(PRFileDesc *console, const Pkcs11& pkcs11)
{
    PR_ASSERT(servssl_init_early_called);
    PR_ASSERT(!servssl_init_late_called);
    servssl_init_late_called = PR_TRUE;

    conf_global_vars_s *globals = conf_get_true_globals();

    PR_ASSERT(globals->Vsecurity_active == pkcs11.enabled);

    // Set the name of the internal token
    PK11_ConfigurePKCS11(NULL, NULL, NULL, INTERNAL_TOKEN_NAME, 
                         NULL, NULL, NULL, NULL, 8, 1);

    char *certdir = file_canonicalize_path(conf_getstring("NSSDir", "."));

    // Initialize NSS read only
    SECStatus rv;
    rv = NSS_Initialize(certdir, "", "", SECMOD_DB, NSS_INIT_READONLY);
    if (rv != SECSuccess && !globals->Vsecurity_active) {
        // Initializing NSS failed, so try to initialize without the trust
        // database as we don't need it anyway
        rv = NSS_NoDB_Init(certdir);
    }

    FREE(certdir);

    if (rv != SECSuccess) {
        ereport(LOG_FAILURE, 
                XP_GetAdminStr(DBT_servNSSInitFailed), system_errmsg());
        return PR_FAILURE;
    }

    servssl_console = console;
    if (!servssl_console)
        servssl_console = PR_STDOUT;

    // Configure the single process session cache after the fork
    if (globals->Vpool_max == 1) {
        if (globals->Vssl_cache_entries > 0) {
            SSL_ConfigServerSessionIDCache(globals->Vssl_cache_entries,
                                           globals->Vsecurity_session_timeout,
                                           globals->Vssl3_session_timeout,
                                           system_get_temp_dir());
            ereport(LOG_VERBOSE, "SSL/TLS session cache is enabled "
                    "(entries=%d, "
                    "ssl2 session timeout=%d, ssl3tls session timeout=%d)",
                    globals->Vssl_cache_entries,
                    globals->Vsecurity_session_timeout,
                    globals->Vssl3_session_timeout);
        }
    }

    if (globals->Vsecurity_active) {
        if (_nss_InitTokens(pkcs11) != SECSuccess) { 
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSPKCS11InitFailed), 
                    system_errmsg());
            return PR_FAILURE;
        }
    }

#if defined(NS_DOMESTIC)
    if (NSS_SetDomesticPolicy() != SECSuccess ){ 
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSSetDomesticFailed), 
                PR_GetError());
        return PR_FAILURE;
    }
#else
#if defined(NS_IMPORTFRANCE)
    if (NSS_SetFrancePolicy() != SECSuccess) { 
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSSetFrenchFailed), 
                PR_GetError());
        return PR_FAILURE;
    }
#else
    if (NSS_SetExportPolicy != SECSuccess) { 
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSSetExportFailed), 
                PR_GetError());
        return PR_FAILURE;
    }
#endif /* NS_IMPORTFRANCE */
#endif /* NS_DOMESTIC */

    // Attach to the multiprocess session cache
    if (globals->Vpool_max != 1) {
        if (globals->Vssl_cache_entries > 0) {
            SSL_InheritMPServerSIDCache(NULL);
        }
    }

    // Do not enable SSL, client auth, or
    // any cipher suites, this is now done per listen socket


    // Do initial load of any CRLs which may be in crl-path dir right now
    crl_check_updates_p(pkcs11.crlPath, PR_FALSE);

    // That's it.
    return PR_SUCCESS;
}


PRBool servssl_pkcs11_enabled()
{
    return conf_get_true_globals()->Vsecurity_active;
}

#ifdef XP_WIN32

//--------------------------------------------------------------------------//
// borrowed from \msvc20\mfc\src\dlgcore.cpp                                //
//--------------------------------------------------------------------------//
void static CenterDialog(HWND hwndParent, HWND hwndDialog)
{
	RECT DialogRect;
	RECT ParentRect;
	POINT Point;
	int nWidth;
	int nHeight;
 
	// Determine if the main window exists. This can be useful when
   // the application creates the dialog box before it creates the
   // main window. If it does exist, retrieve its size to center
   // the dialog box with respect to the main window.
   if(hwndParent != NULL)
	{
      GetClientRect(hwndParent, &ParentRect);
	}
   else
   {
	   // if main window does not exist, center with respect to desktop
   	hwndParent = GetDesktopWindow();
      GetWindowRect(hwndParent, &ParentRect);
   }
 
   // get the size of the dialog box
   GetWindowRect(hwndDialog, &DialogRect);
 
   // calculate height and width for MoveWindow()
   nWidth = DialogRect.right - DialogRect.left;
   nHeight = DialogRect.bottom - DialogRect.top;
 
   // find center point and convert to screen coordinates
   Point.x = (ParentRect.right - ParentRect.left) / 2;
   Point.y = (ParentRect.bottom - ParentRect.top) / 2;

	ClientToScreen(hwndParent, &Point);
 
   // calculate new X, Y starting point
   Point.x -= nWidth / 2;
   Point.y -= nHeight / 2;
 
   MoveWindow(hwndDialog, Point.x, Point.y, nWidth, nHeight, FALSE);
}

//--------------------------------------------------------------------------//
// length and wierdness checks are removed because they                     //
// do not belong in a password entry dialog.  This behavior OK-d            //
// by jsw@netscape.com on 01/26/96                                          //
//--------------------------------------------------------------------------//
BOOL CALLBACK PasswordDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
   {
		case WM_INITDIALOG:
			CenterDialog(NULL, hDlg);
			SendDlgItemMessage(hDlg, IDEDIT, EM_SETLIMITTEXT, sizeof(ntpassword), 0);
			EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
  			SetDlgItemText(hDlg, IDC_PASSWORD, servssl_prompt);
			return(FALSE);

		case WM_COMMAND:

			if(LOWORD(wParam) == IDEDIT)
			{
				if(HIWORD(wParam) == EN_CHANGE)
				{
					if(GetDlgItemText(hDlg, IDEDIT, ntpassword, sizeof(ntpassword)) > 0)
						EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
					else
						EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
				}
			}
			else
			if(LOWORD(wParam) == IDOK)
			{
				GetDlgItemText(hDlg, IDEDIT, ntpassword, sizeof(ntpassword));
				EndDialog(hDlg, IDOK);
				return (TRUE);
			}
			else
			if(LOWORD(wParam) == IDCANCEL)
			{
				memset(ntpassword, 0, sizeof(ntpassword));
				EndDialog(hDlg, IDCANCEL);
				return(FALSE);
			}
    }
    return (FALSE);
}

#endif   /* XP_WIN32 */

PRStatus servssl_check_session(Session *sn)
{
    ClAuth_t *cla;
    CERTCertificate *cert;

    // check if client auth was enabled
    if (0 == sn->clientauth)
	return PR_SUCCESS;	// no cert required - ok

    PR_ASSERT(sn->ssl);

    cla = (ClAuth_t*)sn->clauth;
    cert = SSL_PeerCertificate(sn->csd);

    if (cla->cla_cert != NULL)
	CERT_DestroyCertificate(cla->cla_cert);
    cla->cla_cert = cert;

    if (cert)
	return PR_SUCCESS;	// we have a cert - ok

    if (-1 == sn->clientauth)
	return PR_SUCCESS;	// cert was optional - ok
    
    SSL_InvalidateSession(sn->csd);

    return PR_FAILURE;	// still no cert - fail
}

PRBool servssl_maybe_client_hello(void *buf, int sz)
{
    union SSLClientHello {
        struct {
            PRUint8 msg_length_hi;
            PRUint8 msg_length_lo;
            PRUint8 msg_type;
            PRUint8 version_major;
            PRUint8 version_minor;
            PRUint8 cipher_spec_length_hi;
            PRUint8 cipher_spec_length_lo;
            PRUint8 session_id_length_hi;
            PRUint8 session_id_length_lo;
        } v2;
        struct {
            PRUint8 type;
            PRUint8 version_major;
            PRUint8 version_minor;
            PRUint8 msg_length_hi;
            PRUint8 msg_length_lo;
            PRUint8 msg_type;
        } v3;
    };

    SSLClientHello *hello = (SSLClientHello *) buf;

    if (sz >= sizeof(hello->v2)) {
        if ((hello->v2.msg_length_hi & 0x80) &&
            hello->v2.msg_type == 1 &&
            hello->v2.version_major <= 4 &&
            hello->v2.session_id_length_hi == 0 &&
            hello->v2.session_id_length_lo == 0)
            return PR_TRUE;
    }

    if (sz >= sizeof(hello->v3)) {
        if (hello->v3.type == 16 &&
            hello->v3.version_major >= 2 &&
            hello->v3.version_major <= 4 &&
            hello->v3.msg_type == 1)
            return PR_TRUE;
    }

    return PR_FALSE;
}

/*
 * Return PR_TRUE if certa is better than certb
 */
static PRBool is_better(CERTCertificate *certa, CERTCertificate *certb)
{
    PRTime notBeforeA, notAfterA, notBeforeB, notAfterB;

    SECStatus rv = CERT_GetCertTimes(certa, &notBeforeA, &notAfterA);
    if (rv != SECSuccess)
        return PR_FALSE;

    rv = CERT_GetCertTimes(certb, &notBeforeB, &notAfterB);
    if (rv != SECSuccess)
        return PR_TRUE;

    PRBool isfutureA = (notBeforeA > PR_Now()) ? PR_TRUE : PR_FALSE;
    PRBool isfutureB = (notBeforeB > PR_Now()) ? PR_TRUE : PR_FALSE;

    // If one cert is newer than the other, choose that unless
    // the issue-date of newer cert is in the future and the
    // issue-date of the older cert is in the past
    if (LL_CMP(notBeforeA, >, notBeforeB)) {
        if (isfutureA && !isfutureB)
            return PR_FALSE;
        return PR_TRUE;
    } else {
        if (isfutureB && !isfutureA)
            return PR_TRUE;
        return PR_FALSE;
    }
}

NSAPI_PUBLIC CERTCertificate *servnss_get_cert_from_nickname(const char *nickname, 
                             PK11CertListType listType, CERTCertList* certList)
{
    if (!nickname) { return NULL; }

    CERTCertificate *currentCert = NULL, // Cursor in list
                    *savedCert = NULL;   // Last matching cert

    CERTCertList* clist;
    if (!certList)
        clist = PK11_ListCerts(listType, NULL);
    else
        clist = certList;
    CERTCertListNode *cln;
    for (cln = CERT_LIST_HEAD(clist); !CERT_LIST_END(cln,clist);
            cln = CERT_LIST_NEXT(cln)) {
        currentCert = cln->cert;
        const char* cnick = (const char*) cln->appData;
        if (!cnick) {
            cnick = currentCert->nickname;
        }
        /*If nickname matches
         *    Case 1: Previous match saved in savedCert?
         *            if no, save currentCert into savedCert
         *    Case 2: store newer cert in savedCert
         */
        if (0==strcmp(cnick, nickname))
        {
            if (savedCert == NULL || is_better(currentCert, savedCert))
                savedCert = currentCert;
        }
    }
    //We found our cert
    CERTCertificate *cert = NULL;
    if (savedCert)
        cert = CERT_DupCertificate(savedCert);

    if(!certList)
        CERT_DestroyCertList(clist);

    return cert;
}

