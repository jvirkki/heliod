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

/* #define CRON_CONF_STAND_ALONE */

#include <stdlib.h>
#include <stdarg.h>
#include "definesEnterprise.h"

#ifdef CRON_CONF_STAND_ALONE
#include "cron_conf.h"
#else
#include "libadmin/libadmin.h"
#include "get_file_paths.h"
#ifndef XP_WIN32
#include "frame/conf.h"
#endif
#endif

#ifndef BUF_SIZE
#define BUF_SIZE 4096
#endif

#ifndef S_BUF_SIZE
#define S_BUF_SIZE 1024
#endif

// redefine MALLOC, FREE and STRDUP so that memory allocation is from the heap
// and from the session pool
#undef MALLOC
#define MALLOC(size)     malloc(size)
#undef FREE
#define FREE(x)          free(x)
#undef STRDUP
#define STRDUP(x)        strdup(x)

#ifdef XP_WIN32
#pragma warning (disable: 4005)  // macro redifinition //
#define strcasecmp(x, y) stricmp(x, y)
#pragma warning (default: 4005)  // macro redifinition //
#endif

static char *admroot = NULL;

#define DAILY "Sun Mon Tue Wed Thu Fri Sat"
#define BIG_LINE 1024

static https_cron_conf_list *cclist   = NULL;
static https_cron_conf_list *cctail   = NULL;
static char           *conffile = NULL;

#ifndef CRON_CONF_STAND_ALONE
static void set_roots(void)
{
  if (admroot == NULL) {
    char *root;
    char buf[BIG_LINE];

    if (root = getenv("ADMSERV_ROOT"))
      admroot = STRDUP(root);
    else {
      root = getenv("INSTANCE_ROOT");
#ifndef XP_WIN32
      if (root == NULL) {
        root = server_root; // In case of calling this library
                            // from a servlet via JNI (which is inprocess)
                            // then obtain SERVER_ROOT from conf_getglobals
      }
#endif
      if (root) {
        sprintf(buf, "%s/%s/config", root, PRODUCT_ADMSERV_NAME);
        admroot = STRDUP(buf);
      }
    }
  }
}
#endif

static char *nocr(char *buf)
{
  if (buf)
    {
      if(buf[strlen(buf) - 1] == '\n')
	buf[strlen(buf) - 1] = '\0';
    }
    
  return buf;
}

static int debug(char *fmt, ...)
{
  va_list args;
  char buf[BUF_SIZE];
 
  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  va_end(args);

  fprintf(stdout, "<<DEBUG>> %s <<DEBUG>>\n", buf);
  fflush(stdout);

  return 1;
}

static char *get_conf_file(void)
{
  static char conffile [S_BUF_SIZE];
  char        nsconfile[S_BUF_SIZE];
  char        buf      [BUF_SIZE];
  char       *r, *p;
  FILE       *fp;
  int         flag = 0;

  set_roots();


  if (admroot == NULL)
    return NULL;
     
  sprintf(nsconfile, "%s/schedulerd.conf", admroot);
  
  if (!(fp = fopen(nsconfile, "r")))
    return NULL;

  while(fgets(buf, sizeof(buf), fp))
    {
      r = strtok(buf, " \t\n");
      if (!r) /* bad line, ignore */
	continue;
 
      p = strtok(NULL, " \t\n");
      if (!p) /* bad line, ignore */
	continue;
 
      if (!strcasecmp(r, "ConfFile"))
	{
	 /* if filename without path is specified, default to admin svr dir */
	 if((strchr(p, '\\') == NULL) && 
	    (strchr(p, '/') == NULL))
	    sprintf(conffile, "%s/%s", admroot, p);
	 else
	    sprintf(conffile, "%s", p);
	 flag++;
	 break;
	}
    }

  fclose(fp);

  if (!flag)
    return NULL;

  return conffile;
}


#ifndef CRON_CONF_STAND_ALONE
NSAPI_PUBLIC
#endif
https_cron_conf_obj *https_cron_conf_create_obj(char *name, char *command, char *dir,
				    char *user, char *start_time, char *days)
{
  https_cron_conf_obj *object;
  char *d = NULL;

  object = (https_cron_conf_obj*)MALLOC(sizeof(https_cron_conf_obj));
 
  object->name       = (name)       ? STRDUP(name)       : NULL;
  object->command    = (command)    ? STRDUP(command)    : NULL;
  object->dir        = (dir)        ? STRDUP(dir)        : NULL;
  object->user       = (user)       ? STRDUP(user)       : NULL;
  object->start_time = (start_time) ? STRDUP(start_time) : NULL;

#if 1
  if (days)
    {
      if (!(strcasecmp(days, "Daily")))
	d = STRDUP(DAILY);
      else
	d = STRDUP(days);
    }
#else
  d = STRDUP("Wed Thu");
#endif

  object->days = d;

  return object;
}


static void https_cron_conf_free_listobj(https_cron_conf_list *lobj)
{
  https_cron_conf_obj *obj = lobj->obj;


  if (obj)
    {
      if(obj->name)       FREE(obj->name);
      if(obj->command)    FREE(obj->command);
      if(obj->dir)        FREE(obj->dir);
      if(obj->user)       FREE(obj->user);
      if(obj->start_time) FREE(obj->start_time);
      if(obj->days)       FREE(obj->days);
      
      FREE(obj);
    }
 
  FREE(lobj);
}


static https_cron_conf_obj *get_object(FILE *fp)
{
  https_cron_conf_obj *object;
  char name      [S_BUF_SIZE];
  char command   [S_BUF_SIZE];
  char dir       [S_BUF_SIZE];
  char user      [S_BUF_SIZE]; 
  char start_time[S_BUF_SIZE];
  char days      [S_BUF_SIZE];
  char buf       [BUF_SIZE];
  char *p, *q;
  int flag = 0;
  int hascom, hasdir, hasuser, hastime, hasdays;

  p = fgets(buf, sizeof(buf), fp);

  if (!p)
    return NULL;
  /* else debug("Read line '%s'", nocr(buf)); */

  if (strncmp(buf, "<Object", 7))
    return NULL;

  hascom = hasdir = hasuser = hastime = hasdays = 0;

  p = strtok(buf,  "<=>\n\t ");
  if (!p)
    return NULL;

  p = strtok(NULL, "<=>\n\t ");
  if (!p)
    return NULL;

  p = strtok(NULL, "<=>\n\t ");
  if (!p)
    return NULL;

  sprintf(name, "%s", p);
  /* debug("Setting name to '%s'", name); */

  while(fgets(buf, sizeof(buf), fp))
    {
      /* debug("Read line '%s'", nocr(buf)); */

      p = strtok(buf, " \t\n");

      if (!p)
	continue;

      if (!strcasecmp(p, "</Object>"))
	{
	  flag++;
	  break;
	}

      if(!strcasecmp(p, "Command"))
	{
	  q = strtok(NULL, "\n");

	  if (q)
	    q = strchr(q, '\"');

	  if (q)
	    q++;
      
	  if (q)
	    {
	      if (!hascom)
		{
		  /* get rid of quotes */
		  p = strrchr(q, '\"');

		  if (p)
		    *p = '\0';

		  if (q)
		    {
		      sprintf(command, "%s", q);
		      /* debug("Setting command to '%s'", command); */
		      hascom++;
		    }
		}
	      else /* already has a command */
		;  /* ignore */
	    }
	  continue;
	}

      if(!strcasecmp(p, "Dir"))
	{
	  q = strtok(NULL, "\n");

	  if (q)
	    q = strchr(q, '\"');

	  if (q)
	    q++;
      
	  if (q)
	    {
	      if (!hasdir)
		{
		  /* get rid of quotes */
		  p = strrchr(q, '\"');

		  if (p)
		    *p = '\0';

		  if (q)
		    {
		      sprintf(dir, "%s", q);
		      /* debug("Setting dir to '%s'", dir); */
		      hasdir++;
		    }
		}
	      else /* already has a dir */
		;  /* ignore */
	    }
	  continue;
	}

      else if(!strcasecmp(p, "User"))
	{
	  q = strtok(NULL, " \t\n");
      
	  if (q)
	    {
	      if (!hasuser)
		{
		  sprintf(user, "%s", q);
		  /* debug("Setting user to '%s'", user); */
		  hasuser++;
		}
	      else /* already has a user */
		;  /* ignore */
	    }
	  continue;
	}

      else if(!strcasecmp(p, "Time"))
	{
	  q = strtok(NULL, "\n");
	  
	  if (q)
	    {
	      if (!hastime)
		{
		  sprintf(start_time, "%s", q);
		  /* debug("Setting time to '%s'", start_time); */
		  hastime++;
		}
	      else /* already has a time */
		;  /* ignore */
	    }
	  continue;
	}

      else if(!strcasecmp(p, "Days"))
	{
	  q = strtok(NULL, "\n");

	  if (q)
	    {
	      if (!hasdays)
		{
		  sprintf(days, "%s", q);
		  /* debug("Setting days to '%s'", days); */
		  hasdays++;
		}
	      else /* already has days */
		;  /* ignore */
	    }
	  continue;
	}

      else
	{
	  /* gibberish...  ignore... will be fixed when
	     file is rewritten */
	  continue;	  
	}
    }

  object = https_cron_conf_create_obj(name,
				(hascom)  ? command    : NULL, 
				(hasdir)  ? dir        : NULL,
				(hasuser) ? user       : NULL, 
				(hastime) ? start_time : NULL, 
				(hasdays) ? days       : NULL);

  return object;
}


static void https_cron_conf_write_stream(FILE *fp)
{
  https_cron_conf_obj *obj;
  https_cron_conf_list *lobj;

  for(lobj = cclist; lobj; lobj = lobj->next)
    {
      obj = lobj->obj;

      fprintf(fp, "<Object name=%s>\n", (obj->name) ? obj->name : "?");
      fprintf(fp, "    Command \"%s\"\n", (obj->command) ? obj->command : "?");
      if (obj->dir) 
	fprintf(fp, "    Dir \"%s\"\n", obj->dir);
      if (obj->user) 
	fprintf(fp, "    User %s\n", obj->user);
      fprintf(fp, "    Time %s\n", (obj->start_time) ? obj->start_time : "?");
      fprintf(fp, "    Days %s\n", (obj->days) ? obj->days : "?");
      fprintf(fp, "</Object>\n");
    }
}

#ifndef CRON_CONF_STAND_ALONE
NSAPI_PUBLIC
#endif
void https_cron_conf_delete(char *name, https_cron_conf_obj *cco)
{
  https_cron_conf_list *lobj = NULL;
  https_cron_conf_list *pobj = NULL;

  if (!cclist)
    return;

  if (!strcmp(cclist->name, name))
    {
      lobj = cclist;
      cclist = cclist->next;
      if (cctail == lobj)
	cctail = cclist;

      https_cron_conf_free_listobj(lobj);
    }
  else
    {
      pobj = cclist;

      for(lobj = cclist->next; lobj; lobj = lobj->next)
	{
	  if(!strcmp(lobj->name, name))
	    {
	      if (lobj == cctail)
		cctail = pobj;

	      pobj->next = lobj->next;
	      https_cron_conf_free_listobj(lobj);

	      break;
	    }

	  pobj = lobj;
	}
    }

  return;
}

#ifndef CRON_CONF_STAND_ALONE
NSAPI_PUBLIC
#endif
int https_cron_conf_read()
{
  FILE *fp;
  https_cron_conf_obj *obj;
  https_cron_conf_list *lobj;

#ifndef CRON_CONF_STAND_ALONE
  set_roots();
#endif

  if (!(conffile = get_conf_file()))
    {
      /* debug("Conffile is null"); */
      return 0;
    }
  /* else debug("Conffile: '%s'", conffile); */

  if (!(fp = fopen(conffile, "r")))
    {
      /* debug("Couldn't open conffile"); */
      return 0;
    }

  while((obj = get_object(fp)))
    {
      lobj       = (https_cron_conf_list*)MALLOC(sizeof(struct https_cron_conf_list));
      lobj->name = obj->name;
      lobj->obj  = obj;
      lobj->next = NULL;

      /* debug("Created a list object named '%s'", lobj->name); */

      if(cclist == NULL) /* first object */
	{
	  cclist = cctail = lobj;
	}
      else
	{
	  cctail->next = lobj;
	  cctail       = lobj;
	}

      /* debug("List now, head: '%s', tail: '%s'", 
	 cclist->name, cctail->name); */
    }

  fclose(fp);

  return 1;
}

#ifndef CRON_CONF_STAND_ALONE
NSAPI_PUBLIC
#endif
https_cron_conf_obj *https_cron_conf_get(char *name)
{
  https_cron_conf_obj  *obj  = NULL;
  https_cron_conf_list *lobj = NULL;

 /* find object */
  for(lobj = cclist; lobj; lobj = lobj->next)
    {
      if(!strcmp(lobj->name, name))
	{
	  obj = lobj->obj;
	  break;
	}
    }

#if 0
  if (obj)
    {
      debug("Found object %s", obj->name);
      debug("obj->command = %s", (obj->command) ? obj->command : "NULL");
      debug("obj->dir = %s", (obj->dir) ? obj->dir : "NULL");
      debug("obj->user = %s", (obj->user) ? obj->user : "NULL");
      debug("obj->start_time = %s", (obj->start_time) ? obj->start_time : "NULL");
      debug("obj->days = %s", (obj->days) ? obj->days : "NULL");
    }
#endif

  return obj;
}


#ifndef CRON_CONF_STAND_ALONE
NSAPI_PUBLIC
#endif
https_cron_conf_list *https_cron_conf_get_list()
{
  return cclist;
}

#ifndef CRON_CONF_STAND_ALONE
NSAPI_PUBLIC
#endif
https_cron_conf_obj *https_cron_conf_set(char *name, https_cron_conf_obj *cco)
{
  https_cron_conf_obj  *obj  = NULL;
  https_cron_conf_list *lobj = NULL;

  if (!name)
    return NULL;

  if (!cco)
    {
      https_cron_conf_delete(name, cco);
      return NULL;
    }
  else /* cco exists */
    {
      /* find object */
      obj = https_cron_conf_get(name);


      if (obj)   /* found it */
	{
	  if (cco->command)
	    {
	      FREE(obj->command);
	      obj->command = cco->command;
	    }

	  if (cco->dir)
	    {
	      FREE(obj->dir);
	      obj->dir = cco->dir;
	    }

	  if (cco->user)
	    {
	      FREE(obj->user);
	      obj->user = cco->user;
	    }

	  if (cco->start_time)
	    {
	      FREE(obj->start_time);
	      obj->start_time = cco->start_time;
	    }

	  if (cco->days)
	    {
	      FREE(obj->days);
	      obj->days = cco->days;
	    }

	  FREE(cco);
	}
      else /* couldn't find it */
	{
	  obj = cco;
	  
	  lobj       = (https_cron_conf_list*)MALLOC(sizeof(https_cron_conf_list));
	  lobj->name = obj->name;
	  lobj->obj  = obj;
	  lobj->next = NULL;
	  
	  if(cclist == NULL) /* first object */
	    {
	      cclist = cctail = lobj;
	    }
	  else
	    {
	      cctail->next = lobj;
	      cctail = lobj;
	    }
	}
    }

  return obj;
}

void https_cron_conf_write()
{
  FILE *fp;

  if (!conffile)
    conffile = get_conf_file();

  if(!(fp = fopen(conffile, "w")))
    return;

  https_cron_conf_write_stream(fp);

  fclose(fp);
}


#ifndef CRON_CONF_STAND_ALONE
NSAPI_PUBLIC
#endif
void https_cron_conf_free()
{
  https_cron_conf_list  *lobj  = NULL;
 
  /* find object */
  while(cclist)
    {
      lobj   = cclist;
      cclist = cclist->next;

      https_cron_conf_free_listobj(lobj);
    }

  cclist = cctail = NULL;
}


#ifdef CRON_CONF_STAND_ALONE
main() /* cron.conf management utility */
{
  https_cron_conf_obj *obj;

  char buf        [BUF_SIZE];
  char name       [S_BUF_SIZE];
  char command    [S_BUF_SIZE];
  char dir        [S_BUF_SIZE];
  char user       [S_BUF_SIZE];
  char start_time [S_BUF_SIZE];
  char days       [S_BUF_SIZE];
  
  int  breakloop = 0;
  
  https_cron_conf_read();

  printf("cron.conf mgmt> ");
  while(fgets(buf, BUF_SIZE, stdin) != NULL)
    {
      nocr(buf);

      switch(buf[0])
	{
	case 'p':
	  https_cron_conf_write_stream(stdout);
	  fflush(stdout);
	  break;

	case 'w':
	  https_cron_conf_write();
	  break;

	case 'd':
	  printf("Object to delete? ");
	  fgets(buf, BUF_SIZE, stdin);
	  nocr(buf);

	  https_cron_conf_set(buf, NULL);
	  break;

	case 'f':
	  {
	    printf("Find Object: Name? ");
	    fgets(name, S_BUF_SIZE, stdin);
	    nocr(name);
	    
	    if (https_cron_conf_get(name))
	      printf("Found.\n");
	    else
	      printf("Not found.\n");
	  }
	  break;
	case 'm':
	case 'a':
	  {
	    name       [0] = '\0';
	    command    [0] = '\0';
	    dir        [0] = '\0';
	    user       [0] = '\0';
	    start_time [0] = '\0';
	    days       [0] = '\0';

	    printf("Add/Modify Object: Name? ");
	    fgets(name, S_BUF_SIZE, stdin);
	    nocr(name);
	    
	    https_cron_conf_get(name);

	    if (!(name[0]))
	      break;

	    printf("Add/Modify Object: Command? ");
	    fgets(command, S_BUF_SIZE, stdin);
	    nocr(command);

	    printf("Add/Modify Object: Dir? ");
	    fgets(dir, S_BUF_SIZE, stdin);
	    nocr(dir);

	    printf("Add/Modify Object: User? ");
	    fgets(user, S_BUF_SIZE, stdin);
	    nocr(user);

	    printf("Add/Modify Object: Start Time? ");
	    fgets(start_time, S_BUF_SIZE, stdin);
	    nocr(start_time);

	    printf("Add/Modify Object: Days? ");
	    fgets(days, S_BUF_SIZE, stdin);
	    nocr(days);

	    obj = 
	      https_cron_conf_create_obj( (name      [0]) ? name       : NULL,
                                    (command   [0]) ? command    : NULL,
                                    (dir       [0]) ? dir        : NULL,
                                    (user      [0]) ? user       : NULL,
                                    (start_time[0]) ? start_time : NULL,
                                    (days      [0]) ? days       : NULL);

	    https_cron_conf_set(name, obj);
	    break;
	  }
	  break;

	case 'q':
	  breakloop = 1;
	  break;
	  
	default:
	  printf("Unknown command '%s'\n", buf);
	  break;
	}
  
      if (breakloop)
	break;

      printf("cron.conf mgmt> ");
    }

  https_cron_conf_free();
}
#endif
