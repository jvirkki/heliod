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
 * cmstat.cpp: Content manager status
 * 
 * Daryoush Paknad
 * 
 */

#include "base/util.h"            /* snprint */
#include "frame/cmstat.h"
#include "frame/conf.h"           /* default object */

int cmFlag;
CMVtbl cmVtbl;

#define CM_BUFF_LEN    2048

/* Sets "Link" header, if content manager is on */
NSAPI_PUBLIC void CM_set_link_header(Request *rq)
{
  char link[CM_BUFF_LEN];
  char *uri;

  if ( !CM_get_status() )  /* if cm is off. don't do anything */
	return;
  
  uri = pblock_findkeyval(pb_key_uri, rq->reqpb);

  /* e.g. Link: <http://www.glyphica.com/index.html?services>; rel="services" */
  if (pervs_vars()->Vsecurity_active) {
	if (pervs_vars()->Vport == 443)
	  util_snprintf(link, CM_BUFF_LEN, "<https://%s%s?%s>; rel=\"%s\"", 
					pervs_vars()->Vserver_hostname, uri, CM_SERVICES_NS,CM_SERVICES_NS  );
	else
	  util_snprintf(link, CM_BUFF_LEN, "<https://%s:%d%s?%s>; rel=\"%s\"", 
					pervs_vars()->Vserver_hostname, pervs_vars()->Vport, uri, CM_SERVICES_NS,CM_SERVICES_NS );
  } else {
	if (pervs_vars()->Vport == 80)
	  util_snprintf( link, CM_BUFF_LEN, "<http://%s%s?%s>; rel=\"%s\"", 
					 pervs_vars()->Vserver_hostname, uri, CM_SERVICES_NS,CM_SERVICES_NS );
	else
	  util_snprintf( link, CM_BUFF_LEN, "<http://%s:%d%s?%s>; rel=\"%s\"", 
					 pervs_vars()->Vserver_hostname,  pervs_vars()->Vport, uri, CM_SERVICES_NS,CM_SERVICES_NS );
  }
  pblock_nvinsert("Link", link, rq->srvhdrs);  
}


/* returns "Link" header, if content manager is on. Null otherwise */
NSAPI_PUBLIC int CM_get_link_header(char *uri, char *link, int size )
{
  if ( !CM_get_status() )  /* if cm is off. don't do anything */
	return 0;
  
  /* e.g. Link: <http://www.glyphica.com/index.html?services>; rel ="services" */
  if (pervs_vars()->Vsecurity_active) {
	if (pervs_vars()->Vport == 443)
	  util_snprintf(link, size, "Link: <https://%s%s?%s>; rel=\"%s\"", 
					pervs_vars()->Vserver_hostname, uri, CM_SERVICES_NS,CM_SERVICES_NS  );
	else
	  util_snprintf(link, size, "Link: <https://%s:%d%s?%s>; rel=\"%s\"", 
					pervs_vars()->Vserver_hostname, 
			  pervs_vars()->Vport, uri, CM_SERVICES_NS,CM_SERVICES_NS  );
  } else {
	if (pervs_vars()->Vport == 80)
	  util_snprintf( link, size, "Link: <http://%s%s?%s>; rel=\"%s\"", 
					 pervs_vars()->Vserver_hostname, uri, CM_SERVICES_NS,CM_SERVICES_NS  );
	else
	  util_snprintf( link, size, "Link: <http://%s:%d%s?%s>; rel=\"%s\"", 
					 pervs_vars()->Vserver_hostname, 
			   pervs_vars()->Vport, uri, CM_SERVICES_NS,CM_SERVICES_NS  );
  }
  return 1;
}


NSAPI_PUBLIC void CM_enable_cm(void *CMTrigger) 
{
  cmFlag = 1;
  cmVtbl.CMTrigger = (CMTriggerProc) CMTrigger;
}


NSAPI_PUBLIC void CM_disable_cm(void) 
{
  cmFlag = 0;
}


NSAPI_PUBLIC int CM_get_status(void) 
{
  return cmFlag;
}


NSAPI_PUBLIC int CM_is_cm_query( char *query ) 
{

  if ( cmFlag && (
       !strcmp( query, CM_SERVICES_NS  ) ||
	   !strcmp( query, CM_PROPERTY_STR ) ||
	   !strcmp( query, CM_TOC_NS )       || 
	   !strcmp( query, CM_SYS_PROP_NS )  || 
	   !strcmp( query, CM_CUSTOM_FEILD_NS) ||
	   !strcmp( query, CM_USR_PROP_NS )     ||
#ifdef HAS_RCS
	   !strcmp( query, CM_VER_INFO_NS )  ||
	   !strcmp( query, CM_VER_DIFF_NS )    || 
	   !strcmp( query, CM_START_VERSION_NS) ||
	   !strcmp( query, CM_STOP_VERSION_NS ) ||
	   !strcmp( query, CM_UNCHECKOUT_NS )   ||
#endif
#ifdef HAS_LM
	   !strcmp( query, CM_LINK_INFO_NS ) ||
	   !strcmp( query, CM_VER_LINKS_NS ) ||
#endif
#ifdef DEBUG
	   !strcmp( query, CM_ADMIN_DUMP )  ||
#endif
	   !strcmp( query, CM_HTML_REND_NS )
      ))
	return 1;

  return 0;
}
