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
 * wdconf.c - Watchdog parsing code for server config files.
 *
 *
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifdef AIX
#include <strings.h>
#endif
#include "wdconf.h"
#include "wdlog.h"

#define MAX_FILE_NAME_LENGTH 1024
#define MAX_CONF_LINE_LENGTH 1024

static int _default_port = 80;
static char *_default_addr = NULL;


/* Read config file line like util_getline() */
static int _watchdog_readconf_line(char *line, int maxlen, FILE *file)
{
    int len = 0;
    int nlseen = 0;
    int src;
    int dst;
    char *bufp = line;

    if (feof(file)) {
        return -1;
    }

    while (!nlseen && (len < maxlen - 1)) {

        if (!fgets(bufp, maxlen - len, file))
            break;

        /* Scan what was just read */
        for (dst = 0, src = 0; bufp[src]; ++src) {
            /* Remove CRs */
            if (bufp[src] != '\r') {

                /* Replace NL with NUL */
                if (bufp[src] == '\n') {
                    nlseen = 1;
                    break;
                }

                /* Copy if removing CRs */
                if (src != dst) {
                    bufp[dst] = bufp[src];
                }

                ++dst;
            }
        }

        if (dst > 0) {
            /* Check for continuation */
            if (nlseen && (bufp[dst-1] == '\\')) {
                dst -= 1;
                nlseen = 0;
            }

            len += dst;
            bufp += dst;
        }
    }
                
    if ((len <= 0) && !nlseen) {
        return -1;
    }

    line[len] = '\0';

    return len;
}

static int
_watchdog_parse_magnusconf(char *confdir, char *conffile, 
                           watchdog_conf_info_t *info)
{
    FILE *magnus;
    char filename[MAX_FILE_NAME_LENGTH];
    char line[MAX_CONF_LINE_LENGTH];
    char *name, *value;
    char *addr = NULL;
    int port;
    int len;
    int ssl2 = 0;
    int ssl2rsa = 1;
    int ssl3 = 0;
    int ssl3rsa = 1;

    if (confdir) 
        sprintf(filename, "%s/%s", confdir, conffile);
    else
        sprintf(filename, "%s", conffile);

    magnus = fopen(filename, "r");
    if (!magnus) {
        if(guse_stderr) {
           fprintf(stderr, "Unable to open %s\n", filename);
        }
	watchdog_log(LOG_ERR,
		     "Unable to open %s (%m)",
		     filename);
        return -1;
    }

    while ((len = _watchdog_readconf_line(line, MAX_CONF_LINE_LENGTH,
                                          magnus)) >= 0) {
        name = line;
        if ((*name) == '#')
            continue;
        while((*name) && (isspace(*name))) 
            ++name;  /* skip whitespace */
        if (!(*name))
            continue;                /* blank line */
        for(value=name;(*value) && !isspace(*value); ++value); /* skip name */
        *value++ = '\0';                    /* terminate the name string */
        while((*value) && (isspace(*value))) ++value;  /* skip whitespace */
        if (value[strlen(value)-1] == '\n')
            value[strlen(value)-1] = '\0';

        if (!strcasecmp(name, "PidLog")) {
            info->pidPath = strdup(value);
        }
        else if (!strcasecmp(name, "User")) {
            info->serverUser = strdup(value);
        }
#ifdef XP_UNIX
        else if (!strcasecmp(name, "Chroot")) {
            info->chroot = strdup(value);
        }
#endif /* XP_UNIX */
        if (!strcasecmp(name, "TempDir")) {
            info->tempDir = strdup(value);
        }
        else if (!strcasecmp(name, "Security")) {
            info->secOn = strcasecmp(value, "on") ? 0 : 1;
        }
        else if (!strcasecmp(name, "SSL2")) {
            ssl2 = strcasecmp(value, "on") ? 0 : 1;
        }
        else if (!strcasecmp(name, "SSL3")) {
            ssl3 = strcasecmp(value, "on") ? 0 : 1;
        }
        else if (!strcasecmp(name, "Ciphers")) {
            ssl2rsa = 0;

            /* Look for +any-cipher-name and set ssl2rsa if found */
            while (!ssl2rsa && *value) {
                if (isspace(*value)) {
                    ++value;
                    continue;
                }
                /* Set ssl2rsa if any SSL2 cipher is enabled */
                if (*value == '+') {
                    ssl2rsa = 1;
                }
                else {
                    /* Skip to next cipher, if any */
                    while (*value) {
                        if (*value == ',') {
                            ++value;
                            break;
                        }
                        ++value;
                    }
                }
            }
        }
        else if (!strcasecmp(name, "SSL3Ciphers")) {
            ssl3rsa = 0;

            while (!ssl3rsa && *value) {
                if (isspace(*value)) {
                    ++value;
                    continue;
                }
                if (*value == '+') {
                    char firstch = tolower(value[1]);
                    if (firstch == 'f') {
                    }
                    else {
                        ssl3rsa = 1;
                    }
                    ++value;
                }

                /* Skip to next cipher, if any */
                while (*value) {
                    if (*value == ',') {
                        ++value;
                        break;
                    }
                    ++value;
                }
            }
        }
        else if (!strcasecmp(name, "Address")) {
            addr = strdup(value);
        }
        else if (!strcasecmp(name, "Port")) {
            port = atoi(value);
        }
    }

    fclose(magnus);

    info->rsaOn = (ssl2 & ssl2rsa) | (ssl3 & ssl3rsa);

    if (!info->secOn) {
        info->rsaOn = 0;
    }

    _default_port = port;
    _default_addr = addr;

    return 0;
}

#define OBJ_CONF_NAME "obj.conf"
static int
_watchdog_parse_objconf(char *confdir,
                        watchdog_conf_info_t *info)
{
    FILE *obj;
    char filename[MAX_FILE_NAME_LENGTH];
    char line[MAX_CONF_LINE_LENGTH];
    char *ptr;
    int len;
    int len_address = strlen("address");
    int len_port = strlen("port");

    if (confdir) 
        sprintf(filename, "%s/%s", confdir, OBJ_CONF_NAME);
    else
        sprintf(filename, "%s", OBJ_CONF_NAME);

    obj = fopen(filename, "r");
    if (!obj) {
        if (guse_stderr) {
            fprintf(stderr, "Unable to open %s\n", filename);
        }
	watchdog_log(LOG_ERR,
		     "Unable to open %s (%m)",
		     filename);
        return -1;
    }

    while ((len = _watchdog_readconf_line(line, MAX_CONF_LINE_LENGTH,
                                          obj)) >= 0) {

        ptr = line;

        if ((*ptr) == '#')
            continue;
        while((*ptr) && (isspace(*ptr))) 
            ++ptr;  /* skip whitespace */

        /* Search for "NameTrans" functions */
        if (*ptr && !strncasecmp(ptr, "NameTrans", strlen("NameTrans")) &&
	    strstr(ptr, "document-root"))
	{
            char *addr = 0;
            int port = 0;
            char endquote = '\0';

            ptr = strstr(ptr, "address");
            if (ptr && isspace(ptr[-1])) {
		ptr += len_address;
                while(*ptr && isspace(*ptr)) ptr++;
		    
                if (*ptr++ == '=') {
		    while(*ptr && isspace(*ptr)) ptr++;

		    if (*ptr == '"') {
			++ptr;
			endquote = '"';
		    }

		    addr = ptr;
		    if (endquote) {
			while(*ptr && (*ptr != endquote))
			    ptr++;
		    }
		    else {
			while(*ptr && !isspace(*ptr))
			    ptr++;
		    }

		    endquote = *ptr;
		    *ptr = '\0';
		    addr = strdup(addr);
		    *ptr = endquote;
		    port = _default_port;
		}
            }

            ptr = strstr(line, "port");
            if (ptr && isspace(ptr[-1])) {
		ptr += len_port;
                while(*ptr && isspace(*ptr)) ptr++;

                /* Skip the '=' */
                if (*ptr++ == '=') {
		    while(*ptr && isspace(*ptr)) ptr++;
                
		    /* Skip opening quote if present */
		    if (*ptr == '"')
			++ptr;

		    port = atoi(ptr);
		    if (port < 0 || port > (64*1024))
			port = -1;

		    if (!addr)
			addr = strdup(_default_addr);
		}
            }
				
            if (addr) {
                info->sockets[info->numSockets].port = port;
                info->sockets[info->numSockets].ip = addr;
                info->numSockets++;
            }
        }
    }

    fclose(obj);

    info->sockets[info->numSockets].port = _default_port;
    info->sockets[info->numSockets].ip = _default_addr;
    info->numSockets++;


    return 0;
}


watchdog_conf_info_t *watchdog_parse(char *confdir, char *conffile)
{
    watchdog_conf_info_t *info;

    info = (watchdog_conf_info_t *)malloc(sizeof(watchdog_conf_info_t));
    if (!info) {
        if (guse_stderr) {
            fprintf(stderr, "Out of memory allocating watchdog info\n");
        }
	watchdog_log(LOG_ERR,
		     "Out of memory allocating watchdog info\n");
        return NULL;
    }
    memset(info, 0, sizeof(watchdog_conf_info_t));

    if (_watchdog_parse_magnusconf(confdir, conffile, info) < 0) {
        watchdog_confinfo_free(info);
        return NULL;
    }

#if 0
    if (_watchdog_parse_objconf(confdir, info) < 0) {
        watchdog_confinfo_free(info);
        return NULL;
    }
#endif

    return info;
}

void watchdog_confinfo_free(watchdog_conf_info_t *info)
{
    int index;

    if (info->pidPath) {
        free(info->pidPath);
    }

    if (info->serverUser) {
        free(info->serverUser);
    }

#ifdef XP_UNIX
    if (info->chroot) {
        free(info->chroot);
    }
#endif /* XP_UNIX */

    for (index=0; index<info->numSockets; index++)
	if (info->sockets[index].ip) free(info->sockets[index].ip);

    free(info);
}

void watchdog_print_confinfo(watchdog_conf_info_t *info)
{
    int index;

    printf("serverUser: %s\n", info->serverUser);
#ifdef XP_UNIX
    if (info->chroot) {
        printf("chroot    : %s\n", info->chroot);
    }
#endif /* XP_UNIX */
    printf("pidPath   : %s\n", info->pidPath);
    printf("numSockets: %d\n", info->numSockets);

    for (index=0; index<info->numSockets; index++)
        printf("socket[%d]: %s:%d\n", index, 
               info->sockets[index].ip, info->sockets[index].port);

    return;
}
