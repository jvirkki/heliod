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

#ifndef _LDAPU_DBCONF_H
#define _LDAPU_DBCONF_H

#include <stdio.h>

#ifndef NSAPI_PUBLIC
#ifdef XP_WIN32
#define NSAPI_PUBLIC __declspec(dllexport)
#else
#define NSAPI_PUBLIC 
#endif
#endif

typedef struct dbconf_propval {
    char *prop;			    /* Property name */
    char *val;			    /* Property value */
    struct dbconf_propval *next;    /* Pointer to the next prop-val pair */
} DBPropVal_t;

typedef struct dbconf_dbinfo {
    char *dbname;		/* Database name */
    char *url;			/* Database URL */
    DBPropVal_t *firstprop;	/* pointer to first property-value pair */
    DBPropVal_t *lastprop;	/* pointer to last property-value pair */
    struct dbconf_dbinfo *next;	/* pointer to next db info */
} DBConfDBInfo_t;

typedef struct {
    DBConfDBInfo_t *firstdb;	/* pointer to first db info */
    DBConfDBInfo_t *lastdb;	/* pointer to last db info */
} DBConfInfo_t;

#define DBCONF_DEFAULT_DBNAME "default"

#ifdef __cplusplus
extern "C" {
#endif

NSAPI_PUBLIC extern int dbconf_read_default_dbinfo (const char *file,
						    DBConfDBInfo_t **db_info);
NSAPI_PUBLIC extern int dbconf_read_config_file (const char *file,
						 DBConfInfo_t **conf_info);

NSAPI_PUBLIC extern int ldapu_dbinfo_attrval (DBConfDBInfo_t *db_info,
					      const char *attr, char **val);

NSAPI_PUBLIC extern void dbconf_free_confinfo (DBConfInfo_t *conf_info);
NSAPI_PUBLIC extern void dbconf_free_dbinfo (DBConfDBInfo_t *db_info);

extern void dbconf_free_propval (DBPropVal_t *propval);

extern void dbconf_print_confinfo (DBConfInfo_t *conf_info);
extern void dbconf_print_dbinfo (DBConfDBInfo_t *db_info);
extern void dbconf_print_propval (DBPropVal_t *propval);


NSAPI_PUBLIC int dbconf_output_db_directive (FILE *fp, const char *dbname,
				       const char *url);

NSAPI_PUBLIC int dbconf_output_propval (FILE *fp, const char *dbname,
				  const char *prop, const char *val,
				  const int encoded);

/* Following functions are required by certmap.c file */
extern int dbconf_read_config_file_sub (const char *file,
					const char *directive,
					const int directive_len,
					DBConfInfo_t **conf_info_out);

extern int dbconf_read_default_dbinfo_sub (const char *file,
					   const char *directive,
					   const int directive_len,
					   DBConfDBInfo_t **db_info_out);

NSAPI_PUBLIC int dbconf_get_dbnames (const char *dbmap, char ***dbnames, int *cnt);

NSAPI_PUBLIC int dbconf_free_dbnames (char **dbnames);


extern int ldapu_strcasecmp (const char *s1, const char *s2); 

#ifdef __cplusplus
}
#endif

#endif /* _LDAPU_DBCONF_H */
