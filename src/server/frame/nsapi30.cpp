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

/* nsapi30.cpp
 *
 * HOW TO ADD NEW FUNCTIONS TO THE NSAPI VECTOR TABLES
 *
 * Rule 1:  Never add to a vector table that has already been made public.
 *          Instead, bump the NSAPI version and create a new vector table for
 *          your new function(s). Otherwise, plugins that use your new
 *          functions will explode without any useful diagnostics on older
 *          servers.
 * Rule 2:  Don't allow the size of the table to vary between platforms
 *          If you add a function which is in a conditional #ifdef xxxx, be
 *          sure to include a #else clause which calls one of
 *                 nsapi_unimplemented_function_returns_int
 *                 nsapi_unimplemented_function_returns_ptr
 *                 nsapi_unimplemented_function_returns_void
 *          Otherwise, if someone accidentally compiles with a slightly 
 *          different set of macros than the set used to compile the web 
 *          server, the table alignment will be totally hosed and it will
 *          be very difficult to discover what went wrong.
 */



#include "netsite.h"
#include "base/buffer.h"
#include "base/cinfo.h"
#include "base/crit.h"
#include "base/daemon.h"
#include "base/ereport.h"
#include "base/file.h"
#include "base/net.h"
#include "base/pblock.h"
#include "base/pool.h"
#include "base/regexp.h"
#include "base/sem.h"
#include "base/session.h"
#include "base/shexp.h"
#include "base/shmem.h"
#include "base/systems.h"
#include "base/systhr.h"
#include "base/util.h"
#include "base/vs.h"
#include "libaccess/acl.h"
#include "libaccess/aclproto.h"
#include "frame/conf.h"
#include "frame/func.h"
#include "frame/http.h"
#include "frame/log.h"
#include "frame/object.h"
#include "frame/objset.h"
#include "frame/protocol.h"
#include "frame/req.h"
#include "frame/acl.h"
#include "frame/httpact.h"
#include "frame/nsapi_accessors.h"
#include "frame/filter.h"
#include "frame/dbtframe.h"

PR_BEGIN_EXTERN_C
#include "libproxy/host_dns_cache.h"

void
nsapi_unimplemented_function_msg(void)
{
    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_nsapi30UnImplemented));
    return;
}

void
nsapi_unimplemented_function_returns_void(void)
{
    nsapi_unimplemented_function_msg();
    return;
}

int
nsapi_unimplemented_function_returns_int(void)
{
    nsapi_unimplemented_function_msg();
    return -1;
}

void *
nsapi_unimplemented_function_returns_ptr(void)
{
    nsapi_unimplemented_function_msg();
    return NULL;
}

/* Initialize NSAPI 3.0 dispatch vector as defined in public/nsapi.h */
nsapi_dispatch_t __nsapi30_init = {
    &INTsystem_version,
    &INTsystem_malloc,
    &INTsystem_calloc,
    &INTsystem_realloc,
    &INTsystem_free,
    &INTsystem_strdup,
    &INTsystem_malloc_perm,
    &INTsystem_calloc_perm,
    &INTsystem_realloc_perm,
    &INTsystem_free_perm,
    &INTsystem_strdup_perm,
    &INTgetThreadMallocKey,
    &INTdaemon_atrestart,       /* remap magnus_atrestart */
    &INTfilebuf_open,
    &INTnetbuf_open,
    &INTfilebuf_create,
    &INTfilebuf_close_buffer,
    &INTfilebuf_open_nostat,
#ifdef XP_WIN32
    &INTpipebuf_open,
#else
    &nsapi_unimplemented_function_returns_ptr,
#endif /* XP_WIN32 */
    &nsapi_unimplemented_function_returns_int,
    &INTnetbuf_next,
#ifdef XP_WIN32 
    &INTpipebuf_next,
#else
    &nsapi_unimplemented_function_returns_int,
#endif /* XP_WIN32 */
    &INTfilebuf_close,
    &INTnetbuf_close,
#ifdef XP_WIN32
    &INTpipebuf_close,
#else
    &nsapi_unimplemented_function_returns_void,
#endif /* XP_WIN32 */
    &INTfilebuf_grab,
    &INTnetbuf_grab,
#ifdef XP_WIN32
    &INTpipebuf_grab,
#else
    &nsapi_unimplemented_function_returns_int,
#endif /* XP_WIN32 */
    &INTnetbuf_buf2sd,
    &INTfilebuf_buf2sd,
#ifdef XP_WIN32
    &INTpipebuf_buf2sd,
    &INTpipebuf_netbuf2sd,
    &INTpipebuf_netbuf2pipe,
#else
    &nsapi_unimplemented_function_returns_int,
    &nsapi_unimplemented_function_returns_int,
    &nsapi_unimplemented_function_returns_int,
#endif /* XP_WIN32 */
    &INTcinfo_init,
    &INTcinfo_terminate,
    &INTcinfo_merge,
    &INTcinfo_find,
    &INTcinfo_lookup,
    &INTcinfo_dump_database,
    &INTcrit_init,
    &INTcrit_enter,
    &INTcrit_exit,
    &INTcrit_terminate,
    &INTcondvar_init,
    &INTcondvar_wait,
    &INTcondvar_notify,
    &INTcondvar_notifyAll,
    &INTcondvar_terminate,
    &INTcs_init,
    &INTcs_terminate,
    &INTcs_wait,
    &INTcs_trywait,
    &INTcs_release,
    &INTdaemon_atrestart,
    &nsapi_unimplemented_function_returns_void,  /* OBSOLETE: servssl_init */
    &INTereport,
    &INTereport_v,
    &INTereport_init,
    &INTereport_terminate,
    &INTereport_getfd,
    &INTsystem_fopenRO,
    &INTsystem_fopenWA,
    &INTsystem_fopenRW,
    &INTsystem_fopenWT,
    &INTsystem_fread,
    &INTsystem_fwrite,
    &INTsystem_fwrite_atomic,
    &INTsystem_lseek,
    &INTsystem_fclose,
    &INTsystem_stat,
    &INTsystem_rename,
    &INTsystem_unlink,
    &INTsystem_tlock,
    &INTsystem_flock,
    &INTsystem_ulock,
#ifdef XP_WIN32
    &INTdir_open,
    &INTdir_read,
    &INTdir_close,
#else
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_void,
#endif /* XP_WIN32 */
    &INTdir_create_all,
#ifdef XP_WIN32
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_ptr,
    &INTsystem_pread,
    &INTsystem_pwrite,
    &INTfile_unix2local,
#else
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_int,
    &nsapi_unimplemented_function_returns_int,
    &nsapi_unimplemented_function_returns_void,
#endif /* XP_WIN32 */
    &INTsystem_nocoredumps,
    &INTfile_setinherit,
    &INTfile_notfound,
    &INTsystem_errmsg,
    &INTsystem_errmsg_fn,
    &INTnet_socket_alt,
    &INTnet_listen,
    &INTnet_create_listener,
    &INTnet_connect,
    &INTnet_getpeername,
    &INTnet_close,
    &INTnet_bind,
    &INTnet_accept,
    &INTnet_read,
    &INTnet_write,
    &INTnet_writev,
    &INTnet_isalive,
    &INTnet_ip2host,
    &INTnet_getsockopt,
    &INTnet_setsockopt,
    &INTnet_select,
    &INTnet_ioctl,
    &INTparam_create,
    &INTparam_free,
    &INTpblock_create,
    &INTpblock_free,
    &INTpblock_findval,
    &INTpblock_nvinsert,
    &INTpblock_nninsert,
    &INTpblock_pinsert,
    &INTpblock_str2pblock,
    &INTpblock_pblock2str,
    &INTpblock_copy,
    &INTpblock_dup,
    &INTpblock_pb2env,
    &INTpblock_fr,
    &INTpblock_replace,
    &INTpool_create,
    &INTpool_destroy,
    &INTpool_enabled,
    &INTpool_malloc,
    &INTpool_free,
    &INTpool_calloc,
    &INTpool_realloc,
    &INTpool_strdup,
    &INTregexp_valid,
    &INTregexp_match,
    &INTregexp_cmp,
    &INTregexp_casecmp,
    &INTsem_init,
    &INTsem_terminate,
    &INTsem_grab,
    &INTsem_tgrab,
    &INTsem_release,
    &INTsession_alloc,
    &INTsession_fill,
    &INTsession_create,
    &INTsession_free,
    &INTsession_dns_lookup,
    &INTshexp_valid,
    &INTshexp_match,
    &INTshexp_cmp,
    &INTshexp_casecmp,
    &INTshmem_alloc,
    &INTshmem_free,
    &INTsysthread_start,
    &INTsysthread_current,
    &INTsysthread_yield,
    &INTsysthread_attach,
    &INTsysthread_detach,
    &INTsysthread_terminate,
    &INTsysthread_sleep,
    &INTsysthread_init,
    &INTsysthread_timerset,
    &INTsysthread_newkey,
    &INTsysthread_getdata,
    &INTsysthread_setdata,
    &INTsysthread_set_default_stacksize,
    &INTutil_getline,
    &INTutil_env_create,
    &INTutil_env_str,
    &INTutil_env_replace,
    &INTutil_env_free,
    &INTutil_env_copy,
    &INTutil_env_find,
    &INTutil_hostname,
    &INTutil_chdir2path,
    &INTutil_is_mozilla,
    &INTutil_is_url,
    &INTutil_later_than,
    &INTutil_time_equal,
    &INTutil_str_time_equal,
    &INTutil_uri_is_evil,
    &INTutil_uri_parse,
    &INTutil_uri_unescape,
    &INTutil_uri_escape,
    &INTutil_url_escape,
    &INTutil_sh_escape,
    &INTutil_mime_separator,
    &INTutil_itoa,
    &INTutil_vsprintf,
    &INTutil_sprintf,
    &INTutil_vsnprintf,
    &INTutil_snprintf,
    &INTutil_strftime,
    &INTutil_strtok,
    &INTutil_localtime,
    &INTutil_ctime,
    &INTutil_strerror,
    &INTutil_gmtime,
    &INTutil_asctime,
#ifdef NEED_STRCASECMP
    &INTutil_strcasecmp,
#else
    &nsapi_unimplemented_function_returns_int,
#endif /* NEED_STRCASECMP */
#ifdef NEED_STRNCASECMP
    &INTutil_strncasecmp,
#else
    &nsapi_unimplemented_function_returns_int,
#endif /* NEED_STRNCASECMP */
#ifdef XP_UNIX
    &INTutil_can_exec,
    &INTutil_getpwnam,
    &INTutil_waitpid,
#else
    &nsapi_unimplemented_function_returns_int,
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_int,
#endif /* XP_UNIX */
#ifdef XP_WIN32
    &INTutil_delete_directory,
#else
    &nsapi_unimplemented_function_returns_void,
#endif /* XP_WIN32 */
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_void,
    &INTconf_getglobals,
    &INTfunc_init,
    &INTfunc_find,
    &INTfunc_exec,
    &INTfunc_insert,
    &INTobject_execute,
    &nsapi_unimplemented_function_returns_ptr,
    &INThttp_parse_request,
    &nsapi_unimplemented_function_returns_int,
    &INThttp_start_response,
    &INThttp_hdrs2env,
    &INThttp_status,
    &INThttp_set_finfo,
    &INThttp_dump822,
    &INThttp_finish_request,
    &nsapi_unimplemented_function_returns_void,
    &INThttp_uri2url,
    &INThttp_uri2url_dynamic,
    &INThttp_set_keepalive_timeout,
    &INTlog_error_v,
    &INTlog_error,
    &INTlog_ereport_v,
    &INTlog_ereport,
    &INTobject_create,
    &INTobject_free,
    &INTobject_add_directive,
    &INTobjset_scan_buffer,
    &INTobjset_create,
    &INTobjset_free,
    &INTobjset_free_setonly,
    &INTobjset_new_object,
    &INTobjset_add_object,
    &INTobjset_add_init,
    &INTobjset_findbyname,
    &INTobjset_findbyppath,
    &INTrequest_create,
    &INTrequest_free,
    &INTrequest_restart_internal,
    &INTrequest_header,
    &INTrequest_stat_path,
    &INTconf_getServerString,
    &INTfunc_replace,
    &INTnet_socketpair,
#ifdef XP_UNIX
    &INTnet_dup2,
    &INTnet_is_STDOUT,
    &INTnet_is_STDIN,
#else
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_int,
    &nsapi_unimplemented_function_returns_int,
#endif /* XP_UNIX */
    &INTfunc_set_native_thread_flag,
#ifdef NET_SSL
    &nsapi_random_create,
    &nsapi_random_update,
    &nsapi_random_generate,
    &nsapi_random_destroy,
    &nsapi_md5hash_create,
    &nsapi_md5hash_copy,
    &nsapi_md5hash_begin,
    &nsapi_md5hash_update,
    &nsapi_md5hash_end,
    &nsapi_md5hash_destroy,
    &nsapi_md5hash_data,
#else
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_void,
    &nsapi_unimplemented_function_returns_void,
    &nsapi_unimplemented_function_returns_void,
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_ptr,
    &nsapi_unimplemented_function_returns_void,
    &nsapi_unimplemented_function_returns_void,
    &nsapi_unimplemented_function_returns_void,
    &nsapi_unimplemented_function_returns_void,
    &nsapi_unimplemented_function_returns_void,
#endif
    &ACL_SetupEval,
    &INTnetbuf_getbytes,
    &INTservact_translate_uri,
#ifdef NET_SSL
    &nsapi_rsa_set_priv_fn,
#else
    &nsapi_unimplemented_function_returns_int,
#endif
    &INTnet_native_handle,
    &INTrequest_is_internal,
    &INTutil_cookie_find,
    &INTutil_cookie_next,
    &INTutil_cookie_next_av_pair,
    &INTobjset_get_number_objects,
    &INTobjset_get_object,
    &INTobjset_get_initfns,
    &INTobject_get_name,
    &INTobject_get_num_directives,
    &INTobject_get_directive_table,
    &INTdirective_table_get_num_directives,
    &INTdirective_table_get_directive,
    &INTdirective_get_pblock,
    &INTdirective_get_funcstruct,
    &INTdirective_get_client_pblock,
    &INTvs_register_cb,
    &INTvs_get_id,
    &INTvs_lookup_config_var,
    &INTvs_alloc_slot,
    &INTvs_set_data,
    &INTvs_get_data,
    &INTrequest_get_vs,
    &INTvs_get_httpd_objset,
    &INTvs_get_default_httpd_object,
    &INTvs_get_doc_root,
    &INTvs_translate_uri,
    &INTvs_get_mime_type,
    &INTvs_is_default_vs,
    &INTvs_get_acllist,
    &INTvs_set_acllist,
    &INTfile_is_path_abs,
    &INTfile_canonicalize_path,
    &INTfile_are_files_distinct,
    &INTvs_directive_register_cb,
    &INTvs_substitute_vars,
    &INTconf_getfilename,
    &INTconf_getstring,
    &INTconf_getboolean,
    &INTconf_getinteger,
    &INTconf_getboundedinteger,
    &INTprepare_nsapi_thread
    // Do not add new stuff to __nsapi30_init!  NSAPI 3.0 is frozen.
};

/* Initialize NSAPI 3.2 dispatch vector as defined in public/nsapi.h */
nsapi302_dispatch_t __nsapi302_init = {
    &net_flush,
    &net_sendfile,
    &filter_create,
    &filter_name,
    &filter_find,
    &filter_layer,
    &filter_insert,
    &filter_remove,
    &filter_create_stack
    // Do not add new stuff to __nsapi302_init!  NSAPI 3.2 is frozen.
};

/* Initialize NSAPI 3.3 dispatch vector as defined in public/nsapi.h */
nsapi303_dispatch_t __nsapi303_init = {
    &dns_set_hostent
};

PR_END_EXTERN_C
