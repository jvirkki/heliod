#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2008 Sun Microsystems, Inc. All rights reserved.
#
# THE BSD LICENSE
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer. 
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
#
# Neither the name of the  nor the names of its contributors may be
# used to endorse or promote products derived from this software without 
# specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# NSAPI
#

data            __nsapi30_table
declaration     nsapi_dispatch_t *__nsapi30_table
include         "nsapi.h"
arch            all
version         SUNW_1.1
end

data            __nsapi302_table
declaration     nsapi302_dispatch_t *__nsapi302_table
include         "nsapi.h"
arch            all
version         SUNW_1.2
end

data            __nsapi303_table
declaration     nsapi303_dispatch_t *__nsapi303_table
include         "nsapi.h"
arch            all
version         SUNW_1.2
end

#
# DRNSAPI
#

function        dr_cache_destroy
declaration     void dr_cache_destroy(DrHdl *hdl)
include         "drnsapi.h"
arch            all
version         SUNW_1.1
end

function        dr_cache_init
declaration     PRInt32 dr_cache_init(DrHdl *hdl, RefreshFunc_t ref, FreeFunc_t fre, CompareFunc_t cmp, PRUint32 maxEntries, PRIntervalTime maxAge)
include         "drnsapi.h"
arch            all
version         SUNW_1.1
end

function        dr_cache_refresh
declaration     PRInt32 dr_cache_refresh(DrHdl hdl, const char *key, PRUint32 klen, PRIntervalTime timeout, Entry *entry, Request *rq, Session *sn)
include         "drnsapi.h"
arch            all
version         SUNW_1.1
end

function        dr_net_write
declaration     PRInt32 dr_net_write(DrHdl hdl, const char *key, PRUint32 klen, const char *hdr, const char *ftr, PRUint32 hlen, PRUint32 flen, PRIntervalTime timeout, PRUint32 flags, Request *rq, Session *sn)
include         "drnsapi.h"
arch            all
version         SUNW_1.1
end

function        fc_close
declaration     void fc_close(PRFileDesc *fd, FcHdl *hDl)
include         "drnsapi.h"
arch            all
version         SUNW_1.1
end

function        fc_net_write
declaration     PRInt32 fc_net_write(const char *fileName, const char *hdr, const char *ftr, PRUint32 hlen, PRUint32 flen, PRUint32 flags, PRIntervalTime timeout, Session *sn, Request *rq)
include         "drnsapi.h"
arch            all
version         SUNW_1.1
end

function        fc_open
declaration     PRFileDesc *fc_open(const char *fileName, FcHdl *hDl, PRUint32 flags, Session *sn, Request *rq)
include         "drnsapi.h"
arch            all
version         SUNW_1.1
end

#
# Following symbols are consolidation private
#

data            __nsacl_table
arch            all
version         SUNWprivate
end

data            __1cMStatsManagerDhdr_
arch            all
version         SUNWprivate
end

data            __nsapi30_init
arch            all
version         SUNWprivate
end

data            NET_BUFFERSIZE
arch            all
version         SUNWprivate
end

data            NET_READ_TIMEOUT
arch            all
version         SUNWprivate
end

data            NET_WRITE_TIMEOUT
arch            all
version         SUNWprivate
end

data            net_enableAsyncDNS
arch            all
version         SUNWprivate
end

data            net_enabledns
arch            all
version         SUNWprivate
end

data            net_listenqsize
arch            all
version         SUNWprivate
end

data            pb_key_accept
arch            all
version         SUNWprivate
end

data            pb_key_accept_charset
arch            all
version         SUNWprivate
end

data            pb_key_accept_encoding
arch            all
version         SUNWprivate
end

data            pb_key_accept_language
arch            all
version         SUNWprivate
end

data            pb_key_accept_ranges
arch            all
version         SUNWprivate
end

data            pb_key_auth_cert
arch            all
version         SUNWprivate
end

data            pb_key_auth_group
arch            all
version         SUNWprivate
end

data            pb_key_auth_type
arch            all
version         SUNWprivate
end

data            pb_key_auth_user
arch            all
version         SUNWprivate
end

data            pb_key_authorization
arch            all
version         SUNWprivate
end

data            pb_key_ChunkedRequestBufferSize
arch            all
version         SUNWprivate
end

data            pb_key_charset
arch            all
version         SUNWprivate
end

data            pb_key_cipher
arch            all
version         SUNWprivate
end

data            pb_key_clf_request
arch            all
version         SUNWprivate
end

data            pb_key_connection
arch            all
version         SUNWprivate
end

data            pb_key_content_length
arch            all
version         SUNWprivate
end

data            pb_key_content_type
arch            all
version         SUNWprivate
end

data            pb_key_cookie
arch            all
version         SUNWprivate
end

data            pb_key_dir
arch            all
version         SUNWprivate
end

data            pb_key_dns
arch            all
version         SUNWprivate
end

data            pb_key_enc
arch            all
version         SUNWprivate
end

data            pb_key_etag
arch            all
version         SUNWprivate
end

data            pb_key_find_pathinfo_forward
arch            all
version         SUNWprivate
end

data            pb_key_fn
arch            all
version         SUNWprivate
end

data            pb_key_from
arch            all
version         SUNWprivate
end

data            pb_key_host
arch            all
version         SUNWprivate
end

data            pb_key_if_match
arch            all
version         SUNWprivate
end

data            pb_key_if_modified_since
arch            all
version         SUNWprivate
end

data            pb_key_if_none_match
arch            all
version         SUNWprivate
end

data            pb_key_if_unmodified_since
arch            all
version         SUNWprivate
end

data            pb_key_ip
arch            all
version         SUNWprivate
end

data            pb_key_iponly
arch            all
version         SUNWprivate
end

data            pb_key_lang
arch            all
version         SUNWprivate
end

data            pb_key_last_modified
arch            all
version         SUNWprivate
end

data            pb_key_lock_owner
arch            all
version         SUNWprivate
end

data            pb_key_magnus_charset
arch            all
version         SUNWprivate
end

data            pb_key_magnus_internal_dav_src
arch            all
version         SUNWprivate
end

data            pb_key_magnus_internal_j2ee_nsapi
arch            all
version         SUNWprivate
end

data            pb_key_magnus_internal_error_j2ee
arch            all
version         SUNWprivate
end

data            pb_key_magnus_internal_preserve_srvhdrs
arch            all
version         SUNWprivate
end

data            pb_key_magnus_internal_webapp_errordesc
arch            all
version         SUNWprivate
end

data            pb_key_matched_browser
arch            all
version         SUNWprivate
end

data            pb_key_method
arch            all
version         SUNWprivate
end

data            pb_key_name
arch            all
version         SUNWprivate
end

data            pb_key_nocache
arch            all
version         SUNWprivate
end

data            pb_key_nostat
arch            all
version         SUNWprivate
end

data            pb_key_ntrans_base
arch            all
version         SUNWprivate
end

data            pb_key_path
arch            all
version         SUNWprivate
end

data            pb_key_path_info
arch            all
version         SUNWprivate
end

data            pb_key_ppath
arch            all
version         SUNWprivate
end

data            pb_key_protocol
arch            all
version         SUNWprivate
end

data            pb_key_proxy_jroute
arch            all
version         SUNWprivate
end

data            pb_key_query
arch            all
version         SUNWprivate
end

data            pb_key_range
arch            all
version         SUNWprivate
end

data            pb_key_referer
arch            all
version         SUNWprivate
end

data            pb_key_root
arch            all
version         SUNWprivate
end

data            pb_key_script_name
arch            all
version         SUNWprivate
end

data            pb_key_status
arch            all
version         SUNWprivate
end

data            pb_key_transfer_encoding
arch            all
version         SUNWprivate
end

data            pb_key_type
arch            all
version         SUNWprivate
end

data            pb_key_uri
arch            all
version         SUNWprivate
end

data            pb_key_url
arch            all
version         SUNWprivate
end

data            pb_key_UseOutputStreamSize
arch            all
version         SUNWprivate
end

data            pb_key_user_agent
arch            all
version         SUNWprivate
end
 
data            poolTable
arch            all
version         SUNWprivate
end

data            __1cLCGIResponseJERROR_GEN_
arch            all
version         SUNWprivate
end

data            __1cHCGIBaseG__vtbl_
arch            all
version         SUNWprivate
end

data            __ldapu_table
declaration     LDAPUDispatchVector_t *__ldapu_table
include         "extcmap.h"
arch            all
version         SUNWprivate
end

function        __1cHCGIBase2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cHCGIBase2t6M_v_
arch            all
version         SUNWprivate
end

function        __1cHCGIBase2T5B6M_v_
arch            all
version         SUNWprivate
end

function        __1cHCGIBase2t5B6M_v_
arch            all
version         SUNWprivate
end

function        __1cLCGIResponse2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cLCGIResponse2t6MpkcpnHCGIBase_pnKCGIRequest__v_
arch            all
version         SUNWprivate
end

function        __1cHCGIBaseHgetVsId6M_pc_
arch            all
version         SUNWprivate
end

function        __1cHCGIBaseHprocess6M_i_
arch            all
version         SUNWprivate
end

function        __1cHCGIBaseKgetRequest6M_pnKCGIRequest__
arch            all
version         SUNWprivate
end

function        __1cHCGIBaseMgetParameter6Mpc_1_
arch            all
version         SUNWprivate
end

function        __1cHCGIBaseNgetInstanceId6M_pc_
arch            all
version         SUNWprivate
end

function        __1cHCGIBaseNgetParameters6Mpc_p1_
arch            all
version         SUNWprivate
end

function        __1cHCGIBaseQgetBoolParameter6Mpc_b_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestHgetRoot6M_pc_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestHgetUser6M_pc_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestHgetVsId6M_pc_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestJgetMethod6M_pc_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestMgetParameter6Mpc_1_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestNgetInstanceId6M_pc_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestNgetParameters6Mpc_p1_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestOgetQdDueryString6M_pc_
arch            all
version         SUNWprivate
end

function        __1cKCGIRequestQgetBoolParameter6Mpc_b_
arch            all
version         SUNWprivate
end

function        __1cLCGIResponseFflush6M_v_
arch            all
version         SUNWprivate
end

function        __1cLCGIResponseIgenerate6M_i_
arch            all
version         SUNWprivate
end

function        __1cLCGIResponseMsetGenHeader6Mb_v_
arch            all
version         SUNWprivate
end

function        __1cJWebServerDRun6F_nIPRStatus__
arch            all
version         SUNWprivate
end

function        __1cJWebServerEInit6Fippc_nIPRStatus__
arch            all
version         SUNWprivate
end

function        __1cJWebServerWRequestReconfiguration6F_v_
arch            all
version         SUNWprivate
end

function        __1cJWebServerHCleanup6F_v_
arch            all
version         SUNWprivate
end

function        __1cJWebServerHisReady6F_i_
arch            all
version         SUNWprivate
end

function        __1cLGetSecurity6FpknHSession__i_
arch            all
version         SUNWprivate
end

function        __1cNGetServerHostname6FpknHRequest__pkc_
arch            all
version         SUNWprivate
end

function        __1cNGetServerPort6FpknHRequest__i_
arch            all
version         SUNWprivate
end

function        __1cQGetUrlComponents6FpknHRequest_ppkc5pH_v_
arch            all
version         SUNWprivate
end

function        __1cSHttpMethodRegistryLGetRegistry6F_r0_
arch            all
version         SUNWprivate
end

function        __1cSHttpMethodRegistryOGetMethodIndex6kMpkc_i_
arch            all
version         SUNWprivate
end

function        __1cSHttpMethodRegistryORegisterMethod6Mpkc_i_
arch            all
version         SUNWprivate
end

function        add_client
arch            all
version         SUNWprivate
end

function        add_object
arch            all
version         SUNWprivate
end

function        add_pblock
arch            all
version         SUNWprivate
end

function        add_pblock_os
arch            all
version         SUNWprivate
end

function        add_user_dbm
arch            all
version         SUNWprivate
end

function        add_user_ncsa
arch            all
version         SUNWprivate
end

function        admconf_create
arch            all
version         SUNWprivate
end

function        admconf_scan
arch            all
version         SUNWprivate
end

function        admconf_write
arch            all
version         SUNWprivate
end

function        admin_cgi_send
arch            all
version         SUNWprivate
end

function        admin_cgivars
arch            all
version         SUNWprivate
end

function        admin_check_admpw
arch            all
version         SUNWprivate
end

function        admin_error
arch            all
version         SUNWprivate
end

function        admin_ftype
arch            all
version         SUNWprivate
end

function        admin_uri2path
arch            all
version         SUNWprivate
end

function        alert_word_wrap
arch            all
version         SUNWprivate
end

function        all_numbers
arch            all
version         SUNWprivate
end

function        all_numbers_float
arch            all
version         SUNWprivate
end

function        auth_basic
arch            all
version         SUNWprivate
end

function        auth_basic1
arch            all
version         SUNWprivate
end

function        auth_get_sslid
arch            all
version         SUNWprivate
end

function        auth_init_crits
arch            all
version         SUNWprivate
end

function        cache_create
arch            all
version         SUNWprivate
end

function        cache_create_entry
arch            all
version         SUNWprivate
end

function        cache_delete
arch            all
version         SUNWprivate
end

function        cache_destroy
arch            all
version         SUNWprivate
end

function        cache_do_lookup
arch            all
version         SUNWprivate
end

function        cache_dump
arch            all
version         SUNWprivate
end

function        cache_free_entry
arch            all
version         SUNWprivate
end

function        cache_get_use_count
arch            all
version         SUNWprivate
end

function        cache_insert
arch            all
version         SUNWprivate
end

function        cache_insert_p
arch            all
version         SUNWprivate
end

function        cache_lock
arch            all
version         SUNWprivate
end

function        cache_touch
arch            all
version         SUNWprivate
end

function        cache_unlock
arch            all
version         SUNWprivate
end

function        cache_use_decrement
arch            all
version         SUNWprivate
end

function        cache_use_increment
arch            all
version         SUNWprivate
end

function        cache_valid
arch            all
version         SUNWprivate
end

function        certmap_read_certconfig_file
arch            all
version         SUNWprivate
end

function        certmap_read_default_certinfo
arch            all
version         SUNWprivate
end

function        cgi_common_vars
arch            all
version         SUNWprivate
end

function        cgi_grabstds
arch            all
version         SUNWprivate
end

function        cgi_init
arch            all
version         SUNWprivate
end

function        cgi_query
arch            all
version         SUNWprivate
end

function        cgi_send
arch            all
version         SUNWprivate
end

function        cgi_subsystem_init
arch            all
version         SUNWprivate
end

function        cgiwatch_add_client
arch            all
version         SUNWprivate
end

function        cgiwatch_internal_init
arch            all
version         SUNWprivate
end

function        cgiwatch_remove_client
arch            all
version         SUNWprivate
end

function        cgiwatch_set_expiration_timeout
arch            all
version         SUNWprivate
end

function        cindex_init
arch            all
version         SUNWprivate
end

function        cindex_send
arch            all
version         SUNWprivate
end

function        cinfo_find_ext_type
arch            all
version         SUNWprivate
end

function        clf_init
arch            all
version         SUNWprivate
end

function        clf_record
arch            all
version         SUNWprivate
end

function        comparator_string
arch            all
version         SUNWprivate
end

function        compress_and_replace
arch            all
version         SUNWprivate
end

function        compress_spaces
arch            all
version         SUNWprivate
end

function        cond_match_variable
arch            all
version         SUNWprivate
end

function        condvar_timed_wait
arch            all
version         SUNWprivate
end

function        conf_allocate_globals
arch            all
version         SUNWprivate
end

function        conf_backup
arch            all
version         SUNWprivate
end

function        conf_deleteGlobal
arch            all
version         SUNWprivate
end

function        conf_findGlobal
arch            all
version         SUNWprivate
end

function        conf_get_vs
arch            all
version         SUNWprivate
end

function        conf_initGlobal
arch            all
version         SUNWprivate
end

function        conf_init_true_globals
arch            all
version         SUNWprivate
end

function        conf_is_late_init
arch            all
version         SUNWprivate
end

function        conf_issecurityactive
arch            all
version         SUNWprivate
end

function        conf_register_cb
arch            all
version         SUNWprivate
end

function        conf_reset_globals
arch            all
version         SUNWprivate
end

function        conf_reset_true_globals
arch            all
version         SUNWprivate
end

function        conf_get_jvm_libpath
arch            all
version         SUNWprivate
end

function        conf_setGlobal
arch            all
version         SUNWprivate
end

function        conf_set_globals
arch            all
version         SUNWprivate
end

function        conf_set_libpath
arch            all
version         SUNWprivate
end

function        conf_set_vs_globals
arch            all
version         SUNWprivate
end

function        conf_setdefaults
arch            all
version         SUNWprivate
end

function        conf_warnduplicate
arch            all
version         SUNWprivate
end

function        conf_warnunaccessed
arch            all
version         SUNWprivate
end

function        cookieValue
arch            all
version         SUNWprivate
end

function        cookie_assign
arch            all
version         SUNWprivate
end

function        cookie_init
arch            all
version         SUNWprivate
end

function        copy_dir
arch            all
version         SUNWprivate
end

function        copy_file
arch            all
version         SUNWprivate
end

function        count_objects
arch            all
version         SUNWprivate
end

function        cp_file
arch            all
version         SUNWprivate
end

function        create_dir
arch            all
version         SUNWprivate
end

function        create_subdirs
arch            all
version         SUNWprivate
end

function        crit_owner_is_me
arch            all
version         SUNWprivate
end

function        dbconf_decodeval
arch            all
version         SUNWprivate
end

function        dbconf_encodeval
arch            all
version         SUNWprivate
end

function        dbconf_free_confinfo
arch            all
version         SUNWprivate
end

function        dbconf_free_dbinfo
arch            all
version         SUNWprivate
end

function        dbconf_free_dbnames
arch            all
version         SUNWprivate
end

function        dbconf_free_propval
arch            all
version         SUNWprivate
end

function        dbconf_get_dbnames
arch            all
version         SUNWprivate
end

function        dbconf_output_db_directive
arch            all
version         SUNWprivate
end

function        dbconf_output_propval
arch            all
version         SUNWprivate
end

function        dbconf_print_confinfo
arch            all
version         SUNWprivate
end

function        dbconf_print_dbinfo
arch            all
version         SUNWprivate
end

function        dbconf_print_propval
arch            all
version         SUNWprivate
end

function        dbconf_read_config_file
arch            all
version         SUNWprivate
end

function        dbconf_read_config_file_sub
arch            all
version         SUNWprivate
end

function        dbconf_read_default_dbinfo
arch            all
version         SUNWprivate
end

function        dbconf_read_default_dbinfo_sub
arch            all
version         SUNWprivate
end

function        decompose_url
arch            all
version         SUNWprivate
end

function        delete_buffer
arch            all
version         SUNWprivate
end

function        delete_client
arch            all
version         SUNWprivate
end

function        delete_file
arch            all
version         SUNWprivate
end

function        delete_mag_init
arch            all
version         SUNWprivate
end

function        delete_mag_var
arch            all
version         SUNWprivate
end

function        delete_object
arch            all
version         SUNWprivate
end

function        delete_pblock
arch            all
version         SUNWprivate
end

function        delete_pblock_byptr
arch            all
version         SUNWprivate
end

function        delete_pblock_byptr_os
arch            all
version         SUNWprivate
end

function        delete_specific_mag_init
arch            all
version         SUNWprivate
end

function        detect_db_type
arch            all
version         SUNWprivate
end

function        directive_is
arch            all
version         SUNWprivate
end

function        directive_name2num
arch            all
version         SUNWprivate
end

function        directive_num2name
arch            all
version         SUNWprivate
end

function        display_aliases
arch            all
version         SUNWprivate
end

function        dns_cache_delete
arch            all
version         SUNWprivate
end

function        dns_cache_destroy
arch            all
version         SUNWprivate
end

function        dns_cache_init
arch            all
version         SUNWprivate
end

function        dns_cache_insert
arch            all
version         SUNWprivate
end

function        dns_cache_lookup_ip
arch            all
version         SUNWprivate
end

function        dns_cache_touch
arch            all
version         SUNWprivate
end

function        dns_cache_use_decrement
arch            all
version         SUNWprivate
end

function        dns_cache_use_increment
arch            all
version         SUNWprivate
end

function        dns_cache_valid
arch            all
version         SUNWprivate
end

function        dns_enabled
arch            all
version         SUNWprivate
end

function        do_commit
arch            all
version         SUNWprivate
end

function        do_undo
arch            all
version         SUNWprivate
end

function        do_uuencode
arch            all
version         SUNWprivate
end

function        dstats_close
arch            all
version         SUNWprivate
end

function        dstats_open
arch            all
version         SUNWprivate
end

function        dstats_poll_for_restart
arch            all
version         SUNWprivate
end

function        dstats_snapshot
arch            all
version         SUNWprivate
end

function        dump_database
arch            all
version         SUNWprivate
end

function        dump_database_tofile
arch            all
version         SUNWprivate
end

function        end_http_request
arch            all
version         SUNWprivate
end

function        escape_for_shell
arch            all
version         SUNWprivate
end

function        evalComparator
arch            all
version         SUNWprivate
end

function        fake_conflist
arch            all
version         SUNWprivate
end

function        fclose_l
arch            all
version         SUNWprivate
end

function        file_exists
arch            all
version         SUNWprivate
end

function        file_mode_init
arch            all
version         SUNWprivate
end

function        filter_create
arch            all
version         SUNWprivate
end

function        filter_create_stack
arch            all
version         SUNWprivate
end

function        filter_emulate_sendfile
arch            all
version         SUNWprivate
end

function        filter_emulate_writev
arch            all
version         SUNWprivate
end

function        filter_insert
arch            all
version         SUNWprivate
end

function        filter_layer
arch            all
version         SUNWprivate
end

function        find_user_dbm
arch            all
version         SUNWprivate
end

function        find_user_ncsa
arch            all
version         SUNWprivate
end

function        findliteralppath
arch            all
version         SUNWprivate
end

function        fix_hn_exp
arch            all
version         SUNWprivate
end

function        flex_init
arch            all
version         SUNWprivate
end

function        flex_log
arch            all
version         SUNWprivate
end

function        flush_buffer
arch            all
version         SUNWprivate
end

function        fopen_l
arch            all
version         SUNWprivate
end

function        form_unescape
arch            all
version         SUNWprivate
end

function        free_server_instance
arch            all
version         SUNWprivate
end

function        free_str
arch            all
version         SUNWprivate
end

function        free_strlist
arch            all
version         SUNWprivate
end

function        getBackupConf
arch            all
version         SUNWprivate
end

function        getCOVSNames
arch            all
version         SUNWprivate
end

function        getDefaultClass
arch            all
version         SUNWprivate
end

function        getGlobalsKey
arch            all
version         SUNWprivate
end

function        getServerConfigPath
arch            all
version         SUNWprivate
end

function        getServerLogDirPath
arch            all
version         SUNWprivate
end

function        getServerXmlPath
arch            all
version         SUNWprivate
end

function        getVSList
arch            all
version         SUNWprivate
end

function        get_accesslogfile_from_xml
arch            all
version         SUNWprivate
end

function        get_all_acl_files_list_from_serverxml
arch            all
version         SUNWprivate
end

function        get_acl_file
arch            all
version         SUNWprivate
end

function        get_adm_config
arch            all
version         SUNWprivate
end

function        get_admcf_dir
arch            all
version         SUNWprivate
end

function        get_alias_dir
arch            all
version         SUNWprivate
end

function        get_all_mag_inits
arch            all
version         SUNWprivate
end

function        get_auth_user_basic
arch            all
version         SUNWprivate
end

function        get_auth_user_ssl
arch            all
version         SUNWprivate
end

function        get_authorization_basic
arch            all
version         SUNWprivate
end

function        get_begin
arch            all
version         SUNWprivate
end

function        get_bknum
arch            all
version         SUNWprivate
end

function        get_cert2group_ldap
arch            all
version         SUNWprivate
end

function        get_cert_var
arch            all
version         SUNWprivate
end

function        get_cgi_bool
arch            all
version         SUNWprivate
end

function        get_cgi_int
arch            all
version         SUNWprivate
end

function        get_cgi_long
arch            all
version         SUNWprivate
end

function        get_cgi_multiple
arch            all
version         SUNWprivate
end

function        get_cgi_var
arch            all
version         SUNWprivate
end

function        get_cl_directive
arch            all
version         SUNWprivate
end

function        get_commit_dest
arch            all
version         SUNWprivate
end

function        get_conf_dir
arch            all
version         SUNWprivate
end

function        get_current_resource
arch            all
version         SUNWprivate
end

function        get_current_restype
arch            all
version         SUNWprivate
end

function        get_current_typestr
arch            all
version         SUNWprivate
end

function        get_file_size
arch            all
version         SUNWprivate
end

function        get_flock_path
arch            all
version         SUNWprivate
end

function        get_host_name
arch            all
version         SUNWprivate
end

function        get_httpacl_dir
arch            all
version         SUNWprivate
end

function        get_input_ptr
arch            all
version         SUNWprivate
end

function        get_ip_and_mask
arch            all
version         SUNWprivate
end

function        get_is_owner_default
arch            all
version         SUNWprivate
end

function        get_is_valid_password_ldap
arch            all
version         SUNWprivate
end

function        get_is_valid_password_null
arch            all
version         SUNWprivate
end

function        get_key_cert_files
arch            all
version         SUNWprivate
end

function        get_line_from_fd
arch            all
version         SUNWprivate
end

function        get_mag_init
arch            all
version         SUNWprivate
end

function        get_mag_var
arch            all
version         SUNWprivate
end

function        get_msg
arch            all
version         SUNWprivate
end

function        get_mtime
arch            all
version         SUNWprivate
end

function        get_mtime_str
arch            all
version         SUNWprivate
end

function        get_mult_adm_config
arch            all
version         SUNWprivate
end

function        get_nsadm_var
arch            all
version         SUNWprivate
end

function        get_num_cert_var
arch            all
version         SUNWprivate
end

function        get_num_mag_var
arch            all
version         SUNWprivate
end

function        get_org_mtime
arch            all
version         SUNWprivate
end

function        get_pb_directive
arch            all
version         SUNWprivate
end

function        get_referer
arch            all
version         SUNWprivate
end

function        get_serv_url
arch            all
version         SUNWprivate
end

function        get_specific_mag_init
arch            all
version         SUNWprivate
end

function        get_srvname
arch            all
version         SUNWprivate
end

function        get_temp_dir
arch            all
version         SUNWprivate
end

function        get_temp_filename
arch            all
version         SUNWprivate
end

function        get_user_cert_ssl
arch            all
version         SUNWprivate
end

function        get_user_exists_ldap
arch            all
version         SUNWprivate
end

function        get_user_exists_null
arch            all
version         SUNWprivate
end

function        get_user_isinrole_ldap
arch            all
version         SUNWprivate
end

function        get_user_isinrole_null
arch            all
version         SUNWprivate
end

function        get_user_ismember_ldap
arch            all
version         SUNWprivate
end

function        get_user_ismember_null
arch            all
version         SUNWprivate
end

function        get_user_login_basic
arch            all
version         SUNWprivate
end

function        get_userdb_dir
arch            all
version         SUNWprivate
end

function        get_userdn_ldap
arch            all
version         SUNWprivate
end

function        get_workacl_file
arch            all
version         SUNWprivate
end

function        getInstanceCfgRoot
arch            all
version         SUNWprivate
end

function        grab_client
arch            all
version         SUNWprivate
end

function        grab_docroot_from_xml
arch            all
version         SUNWprivate
end

function        grab_errlogfile_fromxml
arch            all
version         SUNWprivate
end

function        grab_object
arch            all
version         SUNWprivate
end

function        grab_object_file
arch            all
version         SUNWprivate
end

function        grab_pblock
arch            all
version         SUNWprivate
end

function        grab_pblock_byid
arch            all
version         SUNWprivate
end

function        grow_strlist
arch            all
version         SUNWprivate
end

function        helpJavaScript
arch            all
version         SUNWprivate
end

function        helpJavaScriptForTopic
arch            all
version         SUNWprivate
end

function        http_set_nsfc_finfo
arch            all
version         SUNWprivate
end

function        httpfilter_set_output_buffer_size
arch            all
version         SUNWprivate
end

function        httpfilter_get_output_buffer_size
arch            all
version         SUNWprivate
end

function        httpfilter_buffer_output
arch            all
version         SUNWprivate
end

function        httpfilter_suppress_flush
arch            all
version         SUNWprivate
end

function        https_SHA1_Hash
arch            all
version         SUNWprivate
end

function        https_cron_conf_create_obj
arch            all
version         SUNWprivate
end

function        https_cron_conf_delete
arch            all
version         SUNWprivate
end

function        https_cron_conf_free
arch            all
version         SUNWprivate
end

function        https_cron_conf_get
arch            all
version         SUNWprivate
end

function        https_cron_conf_get_list
arch            all
version         SUNWprivate
end

function        https_cron_conf_read
arch            all
version         SUNWprivate
end

function        https_cron_conf_set
arch            all
version         SUNWprivate
end

function        https_cron_conf_write
arch            all
version         SUNWprivate
end

function        https_sha1_pw_cmp
arch            all
version         SUNWprivate
end

function        https_sha1_pw_enc
arch            all
version         SUNWprivate
end

function        index_simple
arch            all
version         SUNWprivate
end

function        init_acl_modules
arch            all
version         SUNWprivate
end

function        init_magconfarray
arch            all
version         SUNWprivate
end

function        init_osarray
arch            all
version         SUNWprivate
end

function        insert_alias
arch            all
version         SUNWprivate
end

function        insert_ntrans
arch            all
version         SUNWprivate
end

function        insert_ntrans_an
arch            all
version         SUNWprivate
end

function        insert_pcheck_mp
arch            all
version         SUNWprivate
end

function        install_checkuser
arch            all
version         SUNWprivate
end

function        install_finish_error
arch            all
version         SUNWprivate
end

function        install_killadm
arch            all
version         SUNWprivate
end

function        internalRedirect
arch            all
version         SUNWprivate
end

function        internalRedirect_case_sensitive_request
arch            all
version         SUNWprivate
end

function        internalRedirect_ex
arch            all
version         SUNWprivate
end

function        internalRedirect_request
arch            all
version         SUNWprivate
end

function        is_admserv
arch            all
version         SUNWprivate
end

function        is_end_of_headers
arch            all
version         SUNWprivate
end

function        is_good
arch            all
version         SUNWprivate
end

function        is_remote_server
arch            all
version         SUNWprivate
end

function        is_server_running
arch            all
version         SUNWprivate
end

function        jsEscape
arch            all
version         SUNWprivate
end

function        js_open_referer
arch            all
version         SUNWprivate
end

function        lang_acceptlang_file
arch            all
version         SUNWprivate
end

function        lang_acceptlanguage
arch            all
version         SUNWprivate
end

function        ldapdn_issuffix
arch            all
version         SUNWprivate
end

function        ldapdn_normalize
arch            all
version         SUNWprivate
end

function        ldapu_VTable_set
arch            all
version         SUNWprivate
end

function        ldapu_ber_free
arch            all
version         SUNWprivate
end

function        ldapu_cert_extension_done
arch            all
version         SUNWprivate
end

function        ldapu_cert_to_ldap_entry
arch            all
version         SUNWprivate
end

function        ldapu_cert_to_ldap_entry_with_certmap
arch            all
version         SUNWprivate
end

function        ldapu_cert_to_user
arch            all
version         SUNWprivate
end

function        ldapu_certinfo_delete
arch            all
version         SUNWprivate
end

function        ldapu_certinfo_free
arch            all
version         SUNWprivate
end

function        ldapu_certinfo_modify
arch            all
version         SUNWprivate
end

function        ldapu_certinfo_save
arch            all
version         SUNWprivate
end

function        ldapu_certmap_exit
arch            all
version         SUNWprivate
end

function        ldapu_certmap_info_attrval
arch            all
version         SUNWprivate
end

function        ldapu_certmap_init
arch            all
version         SUNWprivate
end

function        ldapu_certmap_listinfo_free
arch            all
version         SUNWprivate
end

function        ldapu_count_entries
arch            all
version         SUNWprivate
end

function        ldapu_dbinfo_attrval
arch            all
version         SUNWprivate
end

function        ldapu_err2string
arch            all
version         SUNWprivate
end

function        ldapu_find
arch            all
version         SUNWprivate
end

function        ldapu_first_attribute
arch            all
version         SUNWprivate
end

function        ldapu_first_entry
arch            all
version         SUNWprivate
end

function        ldapu_free
arch            all
version         SUNWprivate
end

function        ldapu_free_cert_ava_val
arch            all
version         SUNWprivate
end

function        ldapu_free_old
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_algorithm
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_ava_val
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_der
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_end_date
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_issuer_dn
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_mapfn
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_searchfn
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_start_date
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_subject_dn
arch            all
version         SUNWprivate
end

function        ldapu_get_cert_verifyfn
arch            all
version         SUNWprivate
end

function        ldapu_get_dn
arch            all
version         SUNWprivate
end

function        ldapu_get_first_cert_extension
arch            all
version         SUNWprivate
end

function        ldapu_get_next_cert_extension
arch            all
version         SUNWprivate
end

function        ldapu_get_values
arch            all
version         SUNWprivate
end

function        ldapu_get_values_len
arch            all
version         SUNWprivate
end

function        ldapu_issuer_certinfo
arch            all
version         SUNWprivate
end

function        ldapu_list_add_info
arch            all
version         SUNWprivate
end

function        ldapu_list_add_node
arch            all
version         SUNWprivate
end

function        ldapu_list_alloc
arch            all
version         SUNWprivate
end

function        ldapu_list_copy
arch            all
version         SUNWprivate
end

function        ldapu_list_empty
arch            all
version         SUNWprivate
end

function        ldapu_list_find_node
arch            all
version         SUNWprivate
end

function        ldapu_list_free
arch            all
version         SUNWprivate
end

function        ldapu_list_move
arch            all
version         SUNWprivate
end

function        ldapu_list_print
arch            all
version         SUNWprivate
end

function        ldapu_list_remove_node
arch            all
version         SUNWprivate
end

function        ldapu_malloc
arch            all
version         SUNWprivate
end

function        ldapu_member_certificate_match
arch            all
version         SUNWprivate
end

function        ldapu_memfree
arch            all
version         SUNWprivate
end

function        ldapu_msgfree
arch            all
version         SUNWprivate
end

function        ldapu_name_certinfo
arch            all
version         SUNWprivate
end

function        ldapu_next_attribute
arch            all
version         SUNWprivate
end

function        ldapu_next_entry
arch            all
version         SUNWprivate
end

function        ldapu_propval_alloc
arch            all
version         SUNWprivate
end

function        ldapu_propval_list_free
arch            all
version         SUNWprivate
end

function        ldapu_realloc
arch            all
version         SUNWprivate
end

function        ldapu_search_s
arch            all
version         SUNWprivate
end

function        ldapu_set_cert_mapfn
arch            all
version         SUNWprivate
end

function        ldapu_set_cert_searchfn
arch            all
version         SUNWprivate
end

function        ldapu_set_cert_verifyfn
arch            all
version         SUNWprivate
end

function        ldapu_set_option
arch            all
version         SUNWprivate
end

function        ldapu_simple_bind_s
arch            all
version         SUNWprivate
end

function        ldapu_ssl_init
arch            all
version         SUNWprivate
end

function        ldapu_str_alloc
arch            all
version         SUNWprivate
end

function        ldapu_str_append
arch            all
version         SUNWprivate
end

function        ldapu_str_free
arch            all
version         SUNWprivate
end

function        ldapu_strcasecmp
arch            all
version         SUNWprivate
end

function        ldapu_strdup
arch            all
version         SUNWprivate
end

function        ldapu_unbind
arch            all
version         SUNWprivate
end

function        ldapu_value_free
arch            all
version         SUNWprivate
end

function        ldapu_value_free_len
arch            all
version         SUNWprivate
end

function        ldaputil_exit
arch            all
version         SUNWprivate
end

function        ldaputil_init
arch            all
version         SUNWprivate
end

function        lex_class_check
arch            all
version         SUNWprivate
end

function        lex_class_create
arch            all
version         SUNWprivate
end

function        lex_class_destroy
arch            all
version         SUNWprivate
end

function        lex_next_char
arch            all
version         SUNWprivate
end

function        lex_scan_over
arch            all
version         SUNWprivate
end

function        lex_scan_string
arch            all
version         SUNWprivate
end

function        lex_scan_to
arch            all
version         SUNWprivate
end

function        lex_skip_over
arch            all
version         SUNWprivate
end

function        lex_skip_to
arch            all
version         SUNWprivate
end

function        lex_stream_create
arch            all
version         SUNWprivate
end

function        lex_stream_destroy
arch            all
version         SUNWprivate
end

function        lex_token
arch            all
version         SUNWprivate
end

function        lex_token_append
arch            all
version         SUNWprivate
end

function        lex_token_destroy
arch            all
version         SUNWprivate
end

function        lex_token_get
arch            all
version         SUNWprivate
end

function        lex_token_info
arch            all
version         SUNWprivate
end

function        lex_token_new
arch            all
version         SUNWprivate
end

function        lex_token_start
arch            all
version         SUNWprivate
end

function        lex_token_take
arch            all
version         SUNWprivate
end

function        listServers
arch            all
version         SUNWprivate
end

function        listServersIn
arch            all
version         SUNWprivate
end

function        list_clients
arch            all
version         SUNWprivate
end

function        list_dir
arch            all
version         SUNWprivate
end

function        list_directory
arch            all
version         SUNWprivate
end

function        list_installed_servers
arch            all
version         SUNWprivate
end

function        list_objects
arch            all
version         SUNWprivate
end

function        list_pblocks
arch            all
version         SUNWprivate
end

function        list_pblocks_os
arch            all
version         SUNWprivate
end

function        list_user_dbs
arch            all
version         SUNWprivate
end

function        list_users_dbm
arch            all
version         SUNWprivate
end

function        list_users_ncsa
arch            all
version         SUNWprivate
end

function        log_change
arch            all
version         SUNWprivate
end

function        log_curres
arch            all
version         SUNWprivate
end

function        log_rotate
arch            all
version         SUNWprivate
end

function        log_rotate_init
arch            all
version         SUNWprivate
end

function        make_conflist
arch            all
version         SUNWprivate
end

function        make_dir
arch            all
version         SUNWprivate
end

function        make_http_request
arch            all
version         SUNWprivate
end

function        makelower
arch            all
version         SUNWprivate
end

function        modify_user_dbm
arch            all
version         SUNWprivate
end

function        modify_user_ncsa
arch            all
version         SUNWprivate
end

function        mtime_is_earlier
arch            all
version         SUNWprivate
end

function        myprintf
arch            all
version         SUNWprivate
end

function        needs_commit
arch            all
version         SUNWprivate
end

function        negate_wildcard
arch            all
version         SUNWprivate
end

function        new_buffer
arch            all
version         SUNWprivate
end

function        new_pblock
arch            all
version         SUNWprivate
end

function        new_str
arch            all
version         SUNWprivate
end

function        new_strlist
arch            all
version         SUNWprivate
end

function        next_html_line
arch            all
version         SUNWprivate
end

function        nsAdmin_rexec
arch            all
version         SUNWprivate
end

function        nsapi_deprecated
arch            all
version         SUNWprivate
end

function        nsapi_md5hash_begin
arch            all
version         SUNWprivate
end

function        nsapi_md5hash_copy
arch            all
version         SUNWprivate
end

function        nsapi_md5hash_create
arch            all
version         SUNWprivate
end

function        nsapi_md5hash_data
arch            all
version         SUNWprivate
end

function        nsapi_md5hash_destroy
arch            all
version         SUNWprivate
end

function        nsapi_md5hash_end
arch            all
version         SUNWprivate
end

function        nsapi_md5hash_update
arch            all
version         SUNWprivate
end

function        nsapi_random_create
arch            all
version         SUNWprivate
end

function        nsapi_random_destroy
arch            all
version         SUNWprivate
end

function        nsapi_random_generate
arch            all
version         SUNWprivate
end

function        nsapi_random_update
arch            all
version         SUNWprivate
end

function        nsapi_rsa_set_priv_fn
arch            all
version         SUNWprivate
end

function        nscperror_lookup
arch            all
version         SUNWprivate
end

function        nserrDispose
arch            all
version         SUNWprivate
end

function        nserrFAlloc
arch            all
version         SUNWprivate
end

function        nserrFFree
arch            all
version         SUNWprivate
end

function        nserrGenerate
arch            all
version         SUNWprivate
end

function        nsfc_cache_init
arch            all
version         SUNWprivate
end

function        nsfc_cache_list
arch            all
version         SUNWprivate
end

function        nsfc_get_stats
arch            all
version         SUNWprivate
end

function        nstp_init_saf
arch            all
version         SUNWprivate
end

function        nt_console_init_saf
arch            all
version         SUNWprivate
end

function        ntrans_assign_name
arch            all
version         SUNWprivate
end

function        ntrans_docroot
arch            all
version         SUNWprivate
end

function        ntrans_homepage
arch            all
version         SUNWprivate
end

function        ntrans_init_crits
arch            all
version         SUNWprivate
end

function        ntrans_match_browser
arch            all
version         SUNWprivate
end

function        ntrans_mozilla_redirect
arch            all
version         SUNWprivate
end

function        ntrans_pfx2dir
arch            all
version         SUNWprivate
end

function        ntrans_redirect
arch            all
version         SUNWprivate
end

function        ntrans_set_variable
arch            all
version         SUNWprivate
end

function        ntrans_strip_params
arch            all
version         SUNWprivate
end

function        ntrans_uhome_init
arch            all
version         SUNWprivate
end

function        ntrans_unix_home
arch            all
version         SUNWprivate
end

function        ntrans_user2name
arch            all
version         SUNWprivate
end

function        objset_create_pool
arch            all
version         SUNWprivate
end

function        objset_duplicate
arch            all
version         SUNWprivate
end

function        objset_pool_copy
arch            all
version         SUNWprivate
end

function        objset_pool_free
arch            all
version         SUNWprivate
end

function        open_error_file
arch            all
version         SUNWprivate
end

function        open_html_file
arch            all
version         SUNWprivate
end

function        open_html_file_lang
arch            all
version         SUNWprivate
end

function        otype_changetype
arch            all
version         SUNWprivate
end

function        otype_ext2type
arch            all
version         SUNWprivate
end

function        otype_forcetype
arch            all
version         SUNWprivate
end

function        otype_htmlswitch
arch            all
version         SUNWprivate
end

function        otype_imgswitch
arch            all
version         SUNWprivate
end

function        otype_setdefaulttype
arch            all
version         SUNWprivate
end

function        otype_shtmlhacks
arch            all
version         SUNWprivate
end

function        otype_typebyexp
arch            all
version         SUNWprivate
end

function        output_alert
arch            all
version         SUNWprivate
end

function        output_db_selector
arch            all
version         SUNWprivate
end

function        output_input
arch            all
version         SUNWprivate
end

function        output_uncommitted
arch            all
version         SUNWprivate
end

function        pageheader
arch            all
version         SUNWprivate
end

function        param_Add_PBlock
arch            all
version         SUNWprivate
end

function        param_GetElem
arch            all
version         SUNWprivate
end

function        param_GetNames
arch            all
version         SUNWprivate
end

function        param_GetSize
arch            all
version         SUNWprivate
end

function        param_Parse
arch            all
version         SUNWprivate
end

function        parse_basic_user_login
arch            all
version         SUNWprivate
end

function        parse_digest_user_login
arch            all
version         SUNWprivate
end

function        parse_http_header
arch            all
version         SUNWprivate
end

function        parse_ldap_url
arch            all
version         SUNWprivate
end

function        parse_line
arch            all
version         SUNWprivate
end

function        parse_null_url
arch            all
version         SUNWprivate
end

function        parse_status_line
arch            all
version         SUNWprivate
end

function        parse_xfile_boundary
arch            all
version         SUNWprivate
end

function        parse_xfilename
arch            all
version         SUNWprivate
end

function        pblock_create_pool
arch            all
version         SUNWprivate
end

function        pblock_findkey
arch            all
version         SUNWprivate
end

function        pblock_findkeyval
arch            all
version         SUNWprivate
end

function        pblock_key
arch            all
version         SUNWprivate
end

function        pblock_key_param_create
arch            all
version         SUNWprivate
end

function        pblock_kpinsert
arch            all
version         SUNWprivate
end

function        pblock_kvinsert
arch            all
version         SUNWprivate
end

function        pblock_kvreplace
arch            all
version         SUNWprivate
end

function        pblock_nvreplace
arch            all
version         SUNWprivate
end

function        pblock_param_create
arch            all
version         SUNWprivate
end

function        pblock_pool
arch            all
version         SUNWprivate
end

function        pblock_removekey
arch            all
version         SUNWprivate
end

function        pblock_removeone
arch            all
version         SUNWprivate
end

function        pblock_reserve_indices
arch            all
version         SUNWprivate
end

function        pcheck_acl_state
arch            all
version         SUNWprivate
end

function        pcheck_add_footer
arch            all
version         SUNWprivate
end

function        pcheck_add_header
arch            all
version         SUNWprivate
end

function        pcheck_check_acl
arch            all
version         SUNWprivate
end

function        pcheck_deny_existence
arch            all
version         SUNWprivate
end

function        pcheck_detect_vulnerable
arch            all
version         SUNWprivate
end

function        pcheck_find_index
arch            all
version         SUNWprivate
end

function        pcheck_find_links
arch            all
version         SUNWprivate
end

function        pcheck_find_path
arch            all
version         SUNWprivate
end

function        pcheck_nsconfig
arch            all
version         SUNWprivate
end

function        pcheck_require_auth
arch            all
version         SUNWprivate
end

function        pcheck_set_cache_control
arch            all
version         SUNWprivate
end

function        pcheck_set_virtual_index
arch            all
version         SUNWprivate
end

function        pcheck_ssl_check
arch            all
version         SUNWprivate
end

function        pcheck_ssl_logout
arch            all
version         SUNWprivate
end

function        pcheck_uri_clean
arch            all
version         SUNWprivate
end

function        perf_define_bucket
arch            all
version         SUNWprivate
end

function        perf_init
arch            all
version         SUNWprivate
end

function        pervs_vars
arch            all
version         SUNWprivate
end

function        post_begin
arch            all
version         SUNWprivate
end

function        qos_error
arch            all
version         SUNWprivate
end

function        qos_handler
arch            all
version         SUNWprivate
end

function        read_AbbrDescType_file
arch            all
version         SUNWprivate
end

function        read_alias_files
arch            all
version         SUNWprivate
end

function        read_aliases
arch            all
version         SUNWprivate
end

function        read_config
arch            all
version         SUNWprivate
end

function        read_config_from_file
arch            all
version         SUNWprivate
end

function        read_server_lst
arch            all
version         SUNWprivate
end

function        reconfigure_http
arch            all
version         SUNWprivate
end

function        record_keysize
arch            all
version         SUNWprivate
end

function        record_useragent
arch            all
version         SUNWprivate
end

function        redirect_to_referer
arch            all
version         SUNWprivate
end

function        redirect_to_script
arch            all
version         SUNWprivate
end

function        register_attribute_getter
arch            all
version         SUNWprivate
end

function        register_database_name
arch            all
version         SUNWprivate
end

function        register_database_type
arch            all
version         SUNWprivate
end

function        register_http_method
arch            all
version         SUNWprivate
end

function        register_method
arch            all
version         SUNWprivate
end

function        register_module
arch            all
version         SUNWprivate
end

function        remote_server_inlist
arch            all
version         SUNWprivate
end

function        remove_dir
arch            all
version         SUNWprivate
end

function        remove_directory
arch            all
version         SUNWprivate
end

function        remove_file
arch            all
version         SUNWprivate
end

function        remove_user_dbm
arch            all
version         SUNWprivate
end

function        remove_user_ncsa
arch            all
version         SUNWprivate
end

function        rename_file
arch            all
version         SUNWprivate
end

function        report_error
arch            all
version         SUNWprivate
end

function        report_warning
arch            all
version         SUNWprivate
end

function        request_pool
arch            all
version         SUNWprivate
end

function        getServerLocale
arch            all
version         SUNWprivate
end

function        get_resource_file
arch            all
version         SUNWprivate
end

function        open_resource_bundle
arch            all
version         SUNWprivate
end

function        close_resource_bundle
arch            all
version         SUNWprivate
end

function        get_message
arch            all
version         SUNWprivate
end

function        pool_get_message
arch            all
version         SUNWprivate
end

function        restart_http
arch            all
version         SUNWprivate
end

function        restart_snmp
arch            all
version         SUNWprivate
end

function        return_failure
arch            all
version         SUNWprivate
end

function        return_html_file
arch            all
version         SUNWprivate
end

function        return_html_noref
arch            all
version         SUNWprivate
end

function        return_script
arch            all
version         SUNWprivate
end

function        return_success
arch            all
version         SUNWprivate
end

function        rm_trail_slash
arch            all
version         SUNWprivate
end

function        rwlock_DemoteLock
arch            all
version         SUNWprivate
end

function        rwlock_Init
arch            all
version         SUNWprivate
end

function        rwlock_ReadLock
arch            all
version         SUNWprivate
end

function        rwlock_Terminate
arch            all
version         SUNWprivate
end

function        rwlock_Unlock
arch            all
version         SUNWprivate
end

function        rwlock_WriteLock
arch            all
version         SUNWprivate
end

function        scan_server_instance
arch            all
version         SUNWprivate
end

function        scan_tech
arch            all
version         SUNWprivate
end

function        send_line_to_fd
arch            all
version         SUNWprivate
end

function        service_debug
arch            all
version         SUNWprivate
end

function        service_disable_type
arch            all
version         SUNWprivate
end

function        service_dumpstats
arch            all
version         SUNWprivate
end

function        service_imagemap
arch            all
version         SUNWprivate
end

function        service_keytoosmall
arch            all
version         SUNWprivate
end

function        service_plain_file
arch            all
version         SUNWprivate
end

function        service_plain_range
arch            all
version         SUNWprivate
end

function        service_pool_dump
arch            all
version         SUNWprivate
end

function        service_preencrypted
arch            all
version         SUNWprivate
end

function        service_reconfig
arch            all
version         SUNWprivate
end

function        service_send_error
arch            all
version         SUNWprivate
end

function        service_toobusy
arch            all
version         SUNWprivate
end

function        service_trailer
arch            all
version         SUNWprivate
end

function        servssl_error
arch            all
version         SUNWprivate
end

function        set_all_org_mtimes
arch            all
version         SUNWprivate
end

function        set_bknum
arch            all
version         SUNWprivate
end

function        set_cert_var
arch            all
version         SUNWprivate
end

function        set_commit
arch            all
version         SUNWprivate
end

function        set_current_db
arch            all
version         SUNWprivate
end

function        set_current_resource
arch            all
version         SUNWprivate
end

function        set_default_database
arch            all
version         SUNWprivate
end

function        set_default_method
arch            all
version         SUNWprivate
end

function        set_fake_referer
arch            all
version         SUNWprivate
end

function        set_mag_init
arch            all
version         SUNWprivate
end

function        set_mag_var
arch            all
version         SUNWprivate
end

function        set_mtime_str
arch            all
version         SUNWprivate
end

function        set_nsadm_var
arch            all
version         SUNWprivate
end

function        set_org_mtime
arch            all
version         SUNWprivate
end

function        set_pblock_vals
arch            all
version         SUNWprivate
end

function        set_referer
arch            all
version         SUNWprivate
end

function        shtml_send_old
arch            all
version         SUNWprivate
end

function        shutdown_http
arch            all
version         SUNWprivate
end

function        shutdown_snmp
arch            all
version         SUNWprivate
end

function        simple_group
arch            all
version         SUNWprivate
end

function        simple_user
arch            all
version         SUNWprivate
end

function        startup_http
arch            all
version         SUNWprivate
end

function        startup_snmp
arch            all
version         SUNWprivate
end

function        stats_xml
arch            all
version         SUNWprivate
end

function        str_flag_to_int
arch            all
version         SUNWprivate
end

function        string_to_vec
arch            all
version         SUNWprivate
end

function        symTableAddSym
arch            all
version         SUNWprivate
end

function        symTableDestroy
arch            all
version         SUNWprivate
end

function        symTableEnumerate
arch            all
version         SUNWprivate
end

function        symTableFindSym
arch            all
version         SUNWprivate
end

function        symTableNew
arch            all
version         SUNWprivate
end

function        symTableRemoveSym
arch            all
version         SUNWprivate
end

function        system_get_temp_dir
arch            all
version         SUNWprivate
end

function        system_gets
arch            all
version         SUNWprivate
end

function        system_set_temp_dir
arch            all
version         SUNWprivate
end

function        system_zero
arch            all
version         SUNWprivate
end

function        total_object_count
arch            all
version         SUNWprivate
end

function        true_globals
arch            all
version         SUNWprivate
end

function        ulsAddToList
arch            all
version         SUNWprivate
end

function        ulsAlloc
arch            all
version         SUNWprivate
end

function        ulsFree
arch            all
version         SUNWprivate
end

function        ulsGetCount
arch            all
version         SUNWprivate
end

function        ulsGetEntry
arch            all
version         SUNWprivate
end

function        ulsSortName
arch            all
version         SUNWprivate
end

function        upload_file
arch            all
version         SUNWprivate
end

function        util_canonicalize_redirect
arch            all
version         SUNWprivate
end

function        util_canonicalize_uri
arch            all
version         SUNWprivate
end

function        validate_wildcard
arch            all
version         SUNWprivate
end

function        verify_adm_dbm
arch            all
version         SUNWprivate
end

function        verify_adm_ncsa
arch            all
version         SUNWprivate
end

function        verify_is_admin
arch            all
version         SUNWprivate
end

function        vs_get_conf
arch            all
version         SUNWprivate
end

function        write_adm_config
arch            all
version         SUNWprivate
end

function        write_mult_adm_config
arch            all
version         SUNWprivate
end

function        write_stats_dtd
arch            all
version         SUNWprivate
end

function        write_stats_dump
arch            all
version         SUNWprivate
end

function        write_stats_xml
arch            all
version         SUNWprivate
end

function        write_tech
arch            all
version         SUNWprivate
end

function        ACLPR_CompareCaseStrings
arch            all
version         SUNWprivate
end

function        ACLPR_HashCaseString
arch            all
version         SUNWprivate
end

function        ACLPR_StringFree
arch            all
version         SUNWprivate
end

function        ACL_AclDestroy
arch            all
version         SUNWprivate
end

function        ACL_AclGetTag
arch            all
version         SUNWprivate
end

function        ACL_AclNew
arch            all
version         SUNWprivate
end

function        ACL_AssertAcl
arch            all
version         SUNWprivate
end

function        ACL_AssertAcllist
arch            all
version         SUNWprivate
end

function        ACL_Attr2Index
arch            all
version         SUNWprivate
end

function        ACL_AttrGetterFind
arch            all
version         SUNWprivate
end

function        ACL_AttrGetterFirst
arch            all
version         SUNWprivate
end

function        ACL_AttrGetterNext
arch            all
version         SUNWprivate
end

function        ACL_AttrGetterRegister
arch            all
version         SUNWprivate
end

function        ACL_AttrGetterRegisterInit
arch            all
version         SUNWprivate
end

function        ACL_AuthInfoGetDbType
arch            all
version         SUNWprivate
end

function        ACL_AuthInfoGetDbname
arch            all
version         SUNWprivate
end

function        ACL_AuthInfoGetMethod
arch            all
version         SUNWprivate
end

function        ACL_AuthInfoSetDbname
arch            all
version         SUNWprivate
end

function        ACL_AuthInfoSetMethod
arch            all
version         SUNWprivate
end

function        ACL_Authenticate
arch            all
version         SUNWprivate
end

function        ACL_CachableAclList
arch            all
version         SUNWprivate
end

function        ACL_CacheEvalInfo
arch            all
version         SUNWprivate
end

function        ACL_CacheFlush
arch            all
version         SUNWprivate
end

function        ACL_CacheFlushRegister
arch            all
version         SUNWprivate
end

function        ACL_CritEnter
arch            all
version         SUNWprivate
end

function        ACL_CritExit
arch            all
version         SUNWprivate
end

function        ACL_CritHeld
arch            all
version         SUNWprivate
end

function        ACL_CritInit
arch            all
version         SUNWprivate
end

function        ACL_DatabaseFind
arch            all
version         SUNWprivate
end

function        ACL_DatabaseGetDefault
arch            all
version         SUNWprivate
end

function        ACL_DatabaseNamesFree
arch            all
version         SUNWprivate
end

function        ACL_DatabaseNamesGet
arch            all
version         SUNWprivate
end

function        ACL_DatabaseRegister
arch            all
version         SUNWprivate
end

function        ACL_DatabaseSetDefault
arch            all
version         SUNWprivate
end

function        ACL_DbNameHashInit
arch            all
version         SUNWprivate
end

function        ACL_DbTypeFind
arch            all
version         SUNWprivate
end

function        ACL_DbTypeGetDefault
arch            all
version         SUNWprivate
end

function        ACL_DbTypeHashInit
arch            all
version         SUNWprivate
end

function        ACL_DbTypeIsEqual
arch            all
version         SUNWprivate
end

function        ACL_DbTypeIsRegistered
arch            all
version         SUNWprivate
end

function        ACL_DbTypeNameIsEqual
arch            all
version         SUNWprivate
end

function        ACL_DbTypeParseFn
arch            all
version         SUNWprivate
end

function        ACL_DbTypeRegister
arch            all
version         SUNWprivate
end

function        ACL_DbTypeSetDefault
arch            all
version         SUNWprivate
end

function        ACL_Decompose
arch            all
version         SUNWprivate
end

function        ACL_EreportError
arch            all
version         SUNWprivate
end

function        ACL_EvalDestroy
arch            all
version         SUNWprivate
end

function        ACL_EvalDestroyContext
arch            all
version         SUNWprivate
end

function        ACL_EvalDestroyNoDecrement
arch            all
version         SUNWprivate
end

function        ACL_EvalGetResource
arch            all
version         SUNWprivate
end

function        ACL_EvalGetSubject
arch            all
version         SUNWprivate
end

function        ACL_EvalNew
arch            all
version         SUNWprivate
end

function        ACL_EvalSetACL
arch            all
version         SUNWprivate
end

function        ACL_EvalSetResource
arch            all
version         SUNWprivate
end

function        ACL_EvalSetSubject
arch            all
version         SUNWprivate
end

function        ACL_EvalTestRights
arch            all
version         SUNWprivate
end

function        ACL_ExprAddArg
arch            all
version         SUNWprivate
end

function        ACL_ExprAddAuthInfo
arch            all
version         SUNWprivate
end

function        ACL_ExprAnd
arch            all
version         SUNWprivate
end

function        ACL_ExprAppend
arch            all
version         SUNWprivate
end

function        ACL_ExprClearPFlags
arch            all
version         SUNWprivate
end

function        ACL_ExprDestroy
arch            all
version         SUNWprivate
end

function        ACL_ExprDisplay
arch            all
version         SUNWprivate
end

function        ACL_ExprGetDenyWith
arch            all
version         SUNWprivate
end

function        ACL_ExprNew
arch            all
version         SUNWprivate
end

function        ACL_ExprNot
arch            all
version         SUNWprivate
end

function        ACL_ExprOr
arch            all
version         SUNWprivate
end

function        ACL_ExprSetDenyWith
arch            all
version         SUNWprivate
end

function        ACL_ExprSetPFlags
arch            all
version         SUNWprivate
end

function        ACL_ExprTerm
arch            all
version         SUNWprivate
end

function        ACL_FileDeleteAcl
arch            all
version         SUNWprivate
end

function        ACL_FileGetAcl
arch            all
version         SUNWprivate
end

function        ACL_FileGetNameList
arch            all
version         SUNWprivate
end

function        ACL_FileMergeAcl
arch            all
version         SUNWprivate
end

function        ACL_FileMergeFile
arch            all
version         SUNWprivate
end

function        ACL_FileRenameAcl
arch            all
version         SUNWprivate
end

function        ACL_FileSetAcl
arch            all
version         SUNWprivate
end

function        ACL_FlushEvalInfo
arch            all
version         SUNWprivate
end

function        ACL_FreeDbnames
arch            all
version         SUNWprivate
end

function        ACL_GetAttribute
arch            all
version         SUNWprivate
end

function        ACL_GetDbnames
arch            all
version         SUNWprivate
end

function        ACL_GetDefaultResult
arch            all
version         SUNWprivate
end

function        ACL_GetPathAcls
arch            all
version         SUNWprivate
end

function        ACL_Init
arch            all
version         SUNWprivate
end

function        ACL_CryptCritInit
arch            all
version         SUNWprivate
end

function        ACL_Crypt
arch            all
version         SUNWprivate
end

function        ACL_CryptCompare
arch            all
version         SUNWprivate
end

function        ACL_InitAttr2Index
arch            all
version         SUNWprivate
end

function        ACL_InitFrame
arch            all
version         SUNWprivate
end

function        ACL_InitFramePostMagnus
arch            all
version         SUNWprivate
end

function        ACL_InitHttp
arch            all
version         SUNWprivate
end

function        ACL_InitHttpPostMagnus
arch            all
version         SUNWprivate
end

function        ACL_InitPostMagnus
arch            all
version         SUNWprivate
end

function        ACL_InitSafs
arch            all
version         SUNWprivate
end

function        ACL_InitSafsPostMagnus
arch            all
version         SUNWprivate
end

function        ACL_IsUserInRole
arch            all
version         SUNWprivate
end

function        ACL_LDAPDatabaseHandle
arch            all
version         SUNWprivate
end

function        ACL_LDAPSessionAllocate
arch            all
version         SUNWprivate
end

function        ACL_LDAPSessionFree
arch            all
version         SUNWprivate
end

function        ACL_LasFindEval
arch            all
version         SUNWprivate
end

function        ACL_LasFindFlush
arch            all
version         SUNWprivate
end

function        ACL_LasHashDestroy
arch            all
version         SUNWprivate
end

function        ACL_LasHashInit
arch            all
version         SUNWprivate
end

function        ACL_LasRegister
arch            all
version         SUNWprivate
end

function        ACL_LateInitPostMagnus
arch            all
version         SUNWprivate
end

function        ACL_ListAclDelete
arch            all
version         SUNWprivate
end

function        ACL_ListAppend
arch            all
version         SUNWprivate
end

function        ACL_ListConcat
arch            all
version         SUNWprivate
end

function        ACL_ListDecrement
arch            all
version         SUNWprivate
end

function        ACL_ListDestroy
arch            all
version         SUNWprivate
end

function        ACL_ListFind
arch            all
version         SUNWprivate
end

function        ACL_ListGetFirst
arch            all
version         SUNWprivate
end

function        ACL_ListGetNameList
arch            all
version         SUNWprivate
end

function        ACL_ListGetNext
arch            all
version         SUNWprivate
end

function        ACL_ListNew
arch            all
version         SUNWprivate
end

function        ACL_ListPostParseForAuth
arch            all
version         SUNWprivate
end

function        ACL_MethodFind
arch            all
version         SUNWprivate
end

function        ACL_MethodGetDefault
arch            all
version         SUNWprivate
end

function        ACL_MethodHashInit
arch            all
version         SUNWprivate
end

function        ACL_MethodIsEqual
arch            all
version         SUNWprivate
end

function        ACL_MethodNameIsEqual
arch            all
version         SUNWprivate
end

function        ACL_MethodNamesFree
arch            all
version         SUNWprivate
end

function        ACL_MethodNamesGet
arch            all
version         SUNWprivate
end

function        ACL_MethodRegister
arch            all
version         SUNWprivate
end

function        ACL_MethodSetDefault
arch            all
version         SUNWprivate
end

function        ACL_ModuleRegister
arch            all
version         SUNWprivate
end

function        ACL_NameListDestroy
arch            all
version         SUNWprivate
end

function        ACL_NeedLDAPOverSSL
arch            all
version         SUNWprivate
end

function        ACL_ParseFile
arch            all
version         SUNWprivate
end

function        ACL_ParseString
arch            all
version         SUNWprivate
end

function        ACL_PermAllocEntry
arch            all
version         SUNWprivate
end

function        ACL_PermAllocTable
arch            all
version         SUNWprivate
end

function        ACL_PermFreeEntry
arch            all
version         SUNWprivate
end

function        ACL_PermFreeTable
arch            all
version         SUNWprivate
end

function        ACL_ReadDbMapFile
arch            all
version         SUNWprivate
end

function        ACL_RegisterDbFromACL
arch            all
version         SUNWprivate
end

function        ACL_SetDefaultResult
arch            all
version         SUNWprivate
end

function        ACL_SetupEval
arch            all
version         SUNWprivate
end

function        ACL_UserDBFind
arch            all
version         SUNWprivate
end

function        ACL_UserDBFindDbHandle
arch            all
version         SUNWprivate
end

function        ACL_UserDBGetAttr
arch            all
version         SUNWprivate
end

function        ACL_UserDBGetDbHandle
arch            all
version         SUNWprivate
end

function        ACL_UserDBGetDbName
arch            all
version         SUNWprivate
end

function        ACL_UserDBGetDbType
arch            all
version         SUNWprivate
end

function        ACL_UserDBLookup
arch            all
version         SUNWprivate
end

function        ACL_UserDBReadLock
arch            all
version         SUNWprivate
end

function        ACL_UserDBSetAttr
arch            all
version         SUNWprivate
end

function        ACL_UserDBUnlock
arch            all
version         SUNWprivate
end

function        ACL_UserDBWriteLock
arch            all
version         SUNWprivate
end

function        ACL_WriteFile
arch            all
version         SUNWprivate
end

function        ACL_WriteString
arch            all
version         SUNWprivate
end

function        ADMUTIL_Init
arch            all
version         SUNWprivate
end

function        ADM_GetAuthorizationString
arch            all
version         SUNWprivate
end

function        ADM_GetCurrentPassword
arch            all
version         SUNWprivate
end

function        ADM_GetCurrentUsername
arch            all
version         SUNWprivate
end

function        ADM_GetUXSSid
arch            all
version         SUNWprivate
end

function        ADM_GetUserDNString
arch            all
version         SUNWprivate
end

function        ADM_Init
arch            all
version         SUNWprivate
end

function        ADM_InitializePermissions
arch            all
version         SUNWprivate
end

function        ADM_bk_addFile
arch            all
version         SUNWprivate
end

function        ADM_bk_doBackups
arch            all
version         SUNWprivate
end

function        ADM_bk_done
arch            all
version         SUNWprivate
end

function        ADM_bk_expire
arch            all
version         SUNWprivate
end

function        ADM_bk_findFile
arch            all
version         SUNWprivate
end

function        ADM_bk_initTree
arch            all
version         SUNWprivate
end

function        ADM_bk_makeBackups
arch            all
version         SUNWprivate
end

function        ADM_bk_newConfFile
arch            all
version         SUNWprivate
end

function        ADM_bk_removeFile
arch            all
version         SUNWprivate
end

function        ADM_bk_restoreFile
arch            all
version         SUNWprivate
end

function        ADM_bk_restoreToTime
arch            all
version         SUNWprivate
end

function        ADM_bk_setMaxTags
arch            all
version         SUNWprivate
end

function        ADM_bk_setQuota
arch            all
version         SUNWprivate
end

function        ADM_check_security
arch            all
version         SUNWprivate
end

function        ADM_copy_directory
arch            all
version         SUNWprivate
end

function        ADM_js_passwordDialog
arch            all
version         SUNWprivate
end

function        ADM_mkdir_p
arch            all
version         SUNWprivate
end

function        ADM_pc_check_security_http
arch            all
version         SUNWprivate
end

function        ADM_pc_quietly
arch            all
version         SUNWprivate
end

function        ADM_pi_canDo
arch            all
version         SUNWprivate
end

function        ADM_pi_done
arch            all
version         SUNWprivate
end

function        ADM_pi_init
arch            all
version         SUNWprivate
end

function        ADM_pi_list
arch            all
version         SUNWprivate
end

function        ADM_pi_setBoolean
arch            all
version         SUNWprivate
end

function        ADM_pi_setCapability
arch            all
version         SUNWprivate
end

function        ADM_pi_setValue
arch            all
version         SUNWprivate
end

function        ADM_pi_value
arch            all
version         SUNWprivate
end

function        ADM_pi_when
arch            all
version         SUNWprivate
end

function        ADM_pi_write
arch            all
version         SUNWprivate
end

function        ADM_remove_directory
arch            all
version         SUNWprivate
end

function        CA_GetClientCert
arch            all
version         SUNWprivate
end

function        CA_Initialize
arch            all
version         SUNWprivate
end

function        CA_RequestClientCert
arch            all
version         SUNWprivate
end

function        CA_getcert
arch            all
version         SUNWprivate
end

function        CL_emptyDatabase
arch            all
version         SUNWprivate
end

function        CL_getAdminURL
arch            all
version         SUNWprivate
end

function        CL_getRequest
arch            all
version         SUNWprivate
end

function        CL_getTargetServers
arch            all
version         SUNWprivate
end

function        CL_getUserAuth
arch            all
version         SUNWprivate
end

function        CL_reportDetails
arch            all
version         SUNWprivate
end

function        CL_reportLayout
arch            all
version         SUNWprivate
end

function        CL_reportStatus
arch            all
version         SUNWprivate
end

function        CL_rexec
arch            all
version         SUNWprivate
end

function        DecrementRecursionDepth
arch            all
version         SUNWprivate
end

function        EvalToRes
arch            all
version         SUNWprivate
end

function        GetCgiInitEnv
arch            all
version         SUNWprivate
end

function        GetCurrentRecursionDepth
arch            all
version         SUNWprivate
end

function        GetDNSCacheInfo
arch            all
version         SUNWprivate
end

function        GetMagnusSecurityOptions
arch            all
version         SUNWprivate
end

function        GetMtaHost
arch            all
version         SUNWprivate
end

function        GetNntpHost
arch            all
version         SUNWprivate
end

function        GetServerFileCache
arch            all
version         SUNWprivate
end

function        INTchild_fork
arch            all
version         SUNWprivate
end

function        INTcinfo_dump_database
arch            all
version         SUNWprivate
end

function        INTcinfo_find
arch            all
version         SUNWprivate
end

function        INTcinfo_init
arch            all
version         SUNWprivate
end

function        INTcinfo_lookup
arch            all
version         SUNWprivate
end

function        INTcinfo_merge
arch            all
version         SUNWprivate
end

function        INTcinfo_terminate
arch            all
version         SUNWprivate
end

function        INTcondvar_init
arch            all
version         SUNWprivate
end

function        INTcondvar_notify
arch            all
version         SUNWprivate
end

function        INTcondvar_notifyAll
arch            all
version         SUNWprivate
end

function        INTcondvar_terminate
arch            all
version         SUNWprivate
end

function        INTcondvar_wait
arch            all
version         SUNWprivate
end

function        INTconf_getServerString
arch            all
version         SUNWprivate
end

function        INTconf_getboolean
arch            all
version         SUNWprivate
end

function        INTconf_getboundedinteger
arch            all
version         SUNWprivate
end

function        INTconf_getfilename
arch            all
version         SUNWprivate
end

function        INTconf_getglobals
arch            all
version         SUNWprivate
end

function        INTconf_getinteger
arch            all
version         SUNWprivate
end

function        INTconf_getstring
arch            all
version         SUNWprivate
end

function        INTcrit_enter
arch            all
version         SUNWprivate
end

function        INTcrit_exit
arch            all
version         SUNWprivate
end

function        INTcrit_init
arch            all
version         SUNWprivate
end

function        INTcrit_terminate
arch            all
version         SUNWprivate
end

function        INTcs_init
arch            all
version         SUNWprivate
end

function        INTcs_release
arch            all
version         SUNWprivate
end

function        INTcs_terminate
arch            all
version         SUNWprivate
end

function        INTcs_trywait
arch            all
version         SUNWprivate
end

function        INTcs_wait
arch            all
version         SUNWprivate
end

function        INTdaemon_atrestart
arch            all
version         SUNWprivate
end

function        INTdaemon_dorestart
arch            all
version         SUNWprivate
end

function        INTdir_create_all
arch            all
version         SUNWprivate
end

function        INTdir_create_mode
arch            all
version         SUNWprivate
end

function        INTdirective_get_client_pblock
arch            all
version         SUNWprivate
end

function        INTdirective_get_funcstruct
arch            all
version         SUNWprivate
end

function        INTdirective_get_pblock
arch            all
version         SUNWprivate
end

function        INTdirective_table_get_directive
arch            all
version         SUNWprivate
end

function        INTdirective_table_get_num_directives
arch            all
version         SUNWprivate
end

function        INTdns_guess_domain
arch            all
version         SUNWprivate
end

function        INTereport
arch            all
version         SUNWprivate
end

function        INTereport_getfd
arch            all
version         SUNWprivate
end

function        INTereport_init
arch            all
version         SUNWprivate
end

function        INTereport_terminate
arch            all
version         SUNWprivate
end

function        INTereport_v
arch            all
version         SUNWprivate
end

function        INTfile_are_files_distinct
arch            all
version         SUNWprivate
end

function        INTfile_basename
arch            all
version         SUNWprivate
end

function        INTfile_canonicalize_path
arch            all
version         SUNWprivate
end

function        INTfile_is_path_abs
arch            all
version         SUNWprivate
end

function        INTfile_notfound
arch            all
version         SUNWprivate
end

function        INTfile_setinherit
arch            all
version         SUNWprivate
end

function        INTfilebuf_buf2sd
arch            all
version         SUNWprivate
end

function        INTfilebuf_close
arch            all
version         SUNWprivate
end

function        INTfilebuf_close_buffer
arch            all
version         SUNWprivate
end

function        INTfilebuf_close_nommap
arch            all
version         SUNWprivate
end

function        INTfilebuf_create
arch            all
version         SUNWprivate
end

function        INTfilebuf_grab
arch            all
version         SUNWprivate
end

function        INTfilebuf_open
arch            all
version         SUNWprivate
end

function        INTfilebuf_open_nostat
arch            all
version         SUNWprivate
end

function        INTfilebuf_open_nostat_nommap
arch            all
version         SUNWprivate
end

function        INTfunc_current
arch            all
version         SUNWprivate
end

function        INTfunc_exec
arch            all
version         SUNWprivate
end

function        INTfunc_find
arch            all
version         SUNWprivate
end

function        INTfunc_find_str
arch            all
version         SUNWprivate
end

function        INTfunc_init
arch            all
version         SUNWprivate
end

function        INTfunc_insert
arch            all
version         SUNWprivate
end

function        INTfunc_participates_in_NSAPI_cache
arch            all
version         SUNWprivate
end

function        INTfunc_replace
arch            all
version         SUNWprivate
end

function        INTfunc_set_native_thread_flag
arch            all
version         SUNWprivate
end

function        INTgetThreadMallocKey
arch            all
version         SUNWprivate
end

function        http_format_etag
arch            all
version         SUNWprivate
end

function        http_match_etag
arch            all
version         SUNWprivate
end

function        http_check_preconditions
arch            all
version         SUNWprivate
end

function        INThttp_dump822
arch            all
version         SUNWprivate
end

function        INThttp_finish_request
arch            all
version         SUNWprivate
end

function        INThttp_get_keepalive_timeout
arch            all
version         SUNWprivate
end

function        INThttp_hdrs2env
arch            all
version         SUNWprivate
end

function        INThttp_parse_request
arch            all
version         SUNWprivate
end

function        INThttp_set_finfo
arch            all
version         SUNWprivate
end

function        INThttp_set_keepalive_timeout
arch            all
version         SUNWprivate
end

function        INThttp_start_response
arch            all
version         SUNWprivate
end

function        INThttp_status
arch            all
version         SUNWprivate
end

function        INThttp_status_message
arch            all
version         SUNWprivate
end

function        INThttp_uri2url
arch            all
version         SUNWprivate
end

function        INThttp_uri2url_dynamic
arch            all
version         SUNWprivate
end

function        INTlog_ereport
arch            all
version         SUNWprivate
end

function        INTlog_ereport_v
arch            all
version         SUNWprivate
end

function        INTlog_error
arch            all
version         SUNWprivate
end

function        INTlog_error_v
arch            all
version         SUNWprivate
end

function        INTnet_accept
arch            all
version         SUNWprivate
end

function        INTnet_bind
arch            all
version         SUNWprivate
end

function        INTnet_cancelIO
arch            all
version         SUNWprivate
end

function        INTnet_close
arch            all
version         SUNWprivate
end

function        INTnet_connect
arch            all
version         SUNWprivate
end

function        INTnet_create_listener
arch            all
version         SUNWprivate
end

function        INTnet_create_listener_alt
arch            all
version         SUNWprivate
end

function        INTnet_dup2
arch            all
version         SUNWprivate
end

function        INTnet_flush
arch            all
version         SUNWprivate
end

function        INTnet_getpeername
arch            all
version         SUNWprivate
end

function        INTnet_getsockopt
arch            all
version         SUNWprivate
end

function        INTnet_has_ip
arch            all
version         SUNWprivate
end

function        INTnet_inet_ntoa
arch            all
version         SUNWprivate
end

function        INTnet_init
arch            all
version         SUNWprivate
end

function        INTnet_ioctl
arch            all
version         SUNWprivate
end

function        INTnet_ip2host
arch            all
version         SUNWprivate
end

function        INTnet_is_STDIN
arch            all
version         SUNWprivate
end

function        INTnet_is_STDOUT
arch            all
version         SUNWprivate
end

function        INTnet_isalive
arch            all
version         SUNWprivate
end

function        INTnet_listen
arch            all
version         SUNWprivate
end

function        INTnet_native_handle
arch            all
version         SUNWprivate
end

function        INTnet_read
arch            all
version         SUNWprivate
end

function        INTnet_select
arch            all
version         SUNWprivate
end

function        INTnet_sendfile
arch            all
version         SUNWprivate
end

function        INTnet_setsockopt
arch            all
version         SUNWprivate
end

function        INTnet_shutdown
arch            all
version         SUNWprivate
end

function        INTnet_socket
arch            all
version         SUNWprivate
end

function        INTnet_socket_alt
arch            all
version         SUNWprivate
end

function        INTnet_socketpair
arch            all
version         SUNWprivate
end

function        INTnet_write
arch            all
version         SUNWprivate
end

function        INTnet_writev
arch            all
version         SUNWprivate
end

function        INTnetbuf_buf2file
arch            all
version         SUNWprivate
end

function        INTnetbuf_buf2sd
arch            all
version         SUNWprivate
end

function        INTnetbuf_buf2sd_timed
arch            all
version         SUNWprivate
end

function        INTnetbuf_close
arch            all
version         SUNWprivate
end

function        INTnetbuf_getbytes
arch            all
version         SUNWprivate
end

function        INTnetbuf_grab
arch            all
version         SUNWprivate
end

function        INTnetbuf_next
arch            all
version         SUNWprivate
end

function        INTnetbuf_open
arch            all
version         SUNWprivate
end

function        INTnetbuf_replace
arch            all
version         SUNWprivate
end

function        INTobject_add_directive
arch            all
version         SUNWprivate
end

function        INTobject_create
arch            all
version         SUNWprivate
end

function        INTobject_execute
arch            all
version         SUNWprivate
end

function        INTobject_free
arch            all
version         SUNWprivate
end

function        INTobject_get_directive_table
arch            all
version         SUNWprivate
end

function        INTobject_get_name
arch            all
version         SUNWprivate
end

function        INTobject_get_num_directives
arch            all
version         SUNWprivate
end

function        INTobjset_add_init
arch            all
version         SUNWprivate
end

function        INTobjset_add_object
arch            all
version         SUNWprivate
end

function        INTobjset_create
arch            all
version         SUNWprivate
end

function        INTobjset_findbyname
arch            all
version         SUNWprivate
end

function        INTobjset_findbyppath
arch            all
version         SUNWprivate
end

function        INTobjset_free
arch            all
version         SUNWprivate
end

function        INTobjset_free_setonly
arch            all
version         SUNWprivate
end

function        INTobjset_get_initfns
arch            all
version         SUNWprivate
end

function        INTobjset_get_number_objects
arch            all
version         SUNWprivate
end

function        INTobjset_get_object
arch            all
version         SUNWprivate
end

function        INTobjset_new_object
arch            all
version         SUNWprivate
end

function        INTobjset_scan_buffer
arch            all
version         SUNWprivate
end

function        INTparam_create
arch            all
version         SUNWprivate
end

function        INTparam_free
arch            all
version         SUNWprivate
end

function        INTpblock_copy
arch            all
version         SUNWprivate
end

function        INTpblock_create
arch            all
version         SUNWprivate
end

function        INTpblock_dup
arch            all
version         SUNWprivate
end

function        INTpblock_findval
arch            all
version         SUNWprivate
end

function        INTpblock_fr
arch            all
version         SUNWprivate
end

function        INTpblock_free
arch            all
version         SUNWprivate
end

function        INTpblock_nninsert
arch            all
version         SUNWprivate
end

function        INTpblock_nvinsert
arch            all
version         SUNWprivate
end

function        INTpblock_pb2env
arch            all
version         SUNWprivate
end

function        INTpblock_pblock2str
arch            all
version         SUNWprivate
end

function        INTpblock_pinsert
arch            all
version         SUNWprivate
end

function        INTpblock_replace
arch            all
version         SUNWprivate
end

function        INTpblock_str2pblock
arch            all
version         SUNWprivate
end

function        INTpblock_str2pblock_lowercasename
arch            all
version         SUNWprivate
end

function        INTpool_calloc
arch            all
version         SUNWprivate
end

function        INTpool_create
arch            all
version         SUNWprivate
end

function        INTpool_destroy
arch            all
version         SUNWprivate
end

function        INTpool_enabled
arch            all
version         SUNWprivate
end

function        INTpool_free
arch            all
version         SUNWprivate
end

function        INTpool_init
arch            all
version         SUNWprivate
end

function        INTpool_malloc
arch            all
version         SUNWprivate
end

function        INTpool_mark
arch            all
version         SUNWprivate
end

function        INTpool_realloc
arch            all
version         SUNWprivate
end

function        INTpool_recycle
arch            all
version         SUNWprivate
end

function        INTpool_strdup
arch            all
version         SUNWprivate
end

function        INTprepare_nsapi_thread
arch            all
version         SUNWprivate
end

function        INTrequest_create
arch            all
version         SUNWprivate
end

function        INTrequest_create_copy
arch            all
version         SUNWprivate
end

function        INTrequest_free
arch            all
version         SUNWprivate
end

function        INTrequest_get_start_interval
arch            all
version         SUNWprivate
end

function        INTrequest_get_vs
arch            all
version         SUNWprivate
end

function        INTrequest_has_default_type
arch            all
version         SUNWprivate
end

function        INTrequest_header
arch            all
version         SUNWprivate
end

function        INTrequest_info_path
arch            all
version         SUNWprivate
end

function        INTrequest_initialize
arch            all
version         SUNWprivate
end

function        INTrequest_is_default_type_set
arch            all
version         SUNWprivate
end

function        INTrequest_is_internal
arch            all
version         SUNWprivate
end

function        INTrequest_is_restarted
arch            all
version         SUNWprivate
end

function        INTrequest_prepare_for_restart
arch            all
version         SUNWprivate
end

function        INTrequest_restart_internal
arch            all
version         SUNWprivate
end

function        INTrequest_stat_path
arch            all
version         SUNWprivate
end

function        INTsem_grab
arch            all
version         SUNWprivate
end

function        INTsem_init
arch            all
version         SUNWprivate
end

function        INTsem_release
arch            all
version         SUNWprivate
end

function        INTsem_terminate
arch            all
version         SUNWprivate
end

function        INTsem_tgrab
arch            all
version         SUNWprivate
end

function        INTservact_fileinfo
arch            all
version         SUNWprivate
end

function        INTservact_finderror
arch            all
version         SUNWprivate
end

function        INTservact_handle_processed
arch            all
version         SUNWprivate
end

function        INTservact_lookup_subreq
arch            all
version         SUNWprivate
end

function        servact_nowaytohandle
arch            all
version         SUNWprivate
end

function        INTservact_objset_uri2path
arch            all
version         SUNWprivate
end

function        INTservact_pathchecks
arch            all
version         SUNWprivate
end

function        INTservact_service
arch            all
version         SUNWprivate
end

function        INTservact_translate_uri
arch            all
version         SUNWprivate
end

function        INTservact_translate_uri2
arch            all
version         SUNWprivate
end

function        INTservact_uri2path
arch            all
version         SUNWprivate
end

function        INTsession_alloc
arch            all
version         SUNWprivate
end

function        session_alloc_thread_slot
arch            all
version         SUNWprivate
end

function        INTsession_get_thread_data
arch            all
version         SUNWprivate
end

function        INTsession_set_thread_data
arch            all
version         SUNWprivate
end

function        INTsession_cleanup
arch            all
version         SUNWprivate
end

function        INTsession_create
arch            all
version         SUNWprivate
end

function        INTsession_dns_lookup
arch            all
version         SUNWprivate
end

function        INTsession_fill
arch            all
version         SUNWprivate
end

function        INTsession_fill_ssl
arch            all
version         SUNWprivate
end

function        INTsession_free
arch            all
version         SUNWprivate
end

function        INTshexp_casecmp
arch            all
version         SUNWprivate
end

function        INTshexp_cmp
arch            all
version         SUNWprivate
end

function        INTshexp_match
arch            all
version         SUNWprivate
end

function        INTshexp_noicmp
arch            all
version         SUNWprivate
end

function        INTshexp_valid
arch            all
version         SUNWprivate
end

function        INTshmem_alloc
arch            all
version         SUNWprivate
end

function        INTshmem_free
arch            all
version         SUNWprivate
end

function        INTsystem_calloc
arch            all
version         SUNWprivate
end

function        INTsystem_calloc_perm
arch            all
version         SUNWprivate
end

function        INTsystem_errmsg
arch            all
version         SUNWprivate
end

function        INTsystem_errmsg_fn
arch            all
version         SUNWprivate
end

function        INTsystem_errmsg_init
arch            all
version         SUNWprivate
end

function        INTsystem_fclose
arch            all
version         SUNWprivate
end

function        INTsystem_flock
arch            all
version         SUNWprivate
end

function        INTsystem_fopenRO
arch            all
version         SUNWprivate
end

function        INTsystem_fopenRW
arch            all
version         SUNWprivate
end

function        INTsystem_fopenWA
arch            all
version         SUNWprivate
end

function        INTsystem_fopenWT
arch            all
version         SUNWprivate
end

function        INTsystem_fread
arch            all
version         SUNWprivate
end

function        INTsystem_free
arch            all
version         SUNWprivate
end

function        INTsystem_free_perm
arch            all
version         SUNWprivate
end

function        INTsystem_fwrite
arch            all
version         SUNWprivate
end

function        INTsystem_fwrite_atomic
arch            all
version         SUNWprivate
end

function        INTsystem_lseek
arch            all
version         SUNWprivate
end

function        INTsystem_malloc
arch            all
version         SUNWprivate
end

function        INTsystem_malloc_perm
arch            all
version         SUNWprivate
end

function        INTsystem_nocoredumps
arch            all
version         SUNWprivate
end

function        INTsystem_pool
arch            all
version         SUNWprivate
end

function        INTsystem_realloc
arch            all
version         SUNWprivate
end

function        INTsystem_realloc_perm
arch            all
version         SUNWprivate
end

function        INTsystem_rename
arch            all
version         SUNWprivate
end

function        INTsystem_setnewhandler
arch            all
version         SUNWprivate
end

function        INTsystem_stat
arch            all
version         SUNWprivate
end

function        INTsystem_stat64
arch            all
version         SUNWprivate
end

function        INTsystem_lstat
arch            all
version         SUNWprivate
end

function        INTsystem_lstat64
arch            all
version         SUNWprivate
end

function        INTsystem_strdup
arch            all
version         SUNWprivate
end

function        INTsystem_strdup_perm
arch            all
version         SUNWprivate
end

function        INTsystem_tlock
arch            all
version         SUNWprivate
end

function        INTsystem_ulock
arch            all
version         SUNWprivate
end

function        INTsystem_unlink
arch            all
version         SUNWprivate
end

function        INTsystem_version
arch            all
version         SUNWprivate
end

function        INTsystem_version_set
arch            all
version         SUNWprivate
end

function        INTsysthread_attach
arch            all
version         SUNWprivate
end

function        INTsysthread_current
arch            all
version         SUNWprivate
end

function        INTsysthread_detach
arch            all
version         SUNWprivate
end

function        INTsysthread_getdata
arch            all
version         SUNWprivate
end

function        INTsysthread_init
arch            all
version         SUNWprivate
end

function        INTsysthread_newkey
arch            all
version         SUNWprivate
end

function        INTsysthread_set_default_stacksize
arch            all
version         SUNWprivate
end

function        INTsysthread_setdata
arch            all
version         SUNWprivate
end

function        INTsysthread_sleep
arch            all
version         SUNWprivate
end

function        INTsysthread_start
arch            all
version         SUNWprivate
end

function        INTsysthread_terminate
arch            all
version         SUNWprivate
end

function        INTsysthread_timerset
arch            all
version         SUNWprivate
end

function        INTsysthread_yield
arch            all
version         SUNWprivate
end

function        INTutil_asctime
arch            all
version         SUNWprivate
end

function        INTutil_can_exec
arch            all
version         SUNWprivate
end

function        INTutil_chdir2path
arch            all
version         SUNWprivate
end

function        INTutil_cookie_find
arch            all
version         SUNWprivate
end

function        INTutil_cookie_next
arch            all
version         SUNWprivate
end

function        INTutil_cookie_next_av_pair
arch            all
version         SUNWprivate
end

function        INTutil_ctime
arch            all
version         SUNWprivate
end

function        INTutil_env_copy
arch            all
version         SUNWprivate
end

function        INTutil_env_create
arch            all
version         SUNWprivate
end

function        INTutil_env_find
arch            all
version         SUNWprivate
end

function        INTutil_env_free
arch            all
version         SUNWprivate
end

function        INTutil_env_replace
arch            all
version         SUNWprivate
end

function        INTutil_env_str
arch            all
version         SUNWprivate
end

function        INTutil_format_http_version
arch            all
version         SUNWprivate
end

function        INTutil_getboolean
arch            all
version         SUNWprivate
end

function        INTutil_getline
arch            all
version         SUNWprivate
end

function        INTutil_getpwnam
arch            all
version         SUNWprivate
end

function        INTutil_gmtime
arch            all
version         SUNWprivate
end

function        INTutil_hostname
arch            all
version         SUNWprivate
end

function        INTutil_init_PRNetAddr
arch            all
version         SUNWprivate
end

function        INTutil_is_mozilla
arch            all
version         SUNWprivate
end

function        INTutil_is_url
arch            all
version         SUNWprivate
end

function        INTutil_itoa
arch            all
version         SUNWprivate
end

function        INTutil_later_than
arch            all
version         SUNWprivate
end

function        INTutil_localtime
arch            all
version         SUNWprivate
end

function        INTutil_mime_separator
arch            all
version         SUNWprivate
end

function        INTutil_random
arch            all
version         SUNWprivate
end

function        INTutil_random_init
arch            all
version         SUNWprivate
end

function        INTutil_sh_escape
arch            all
version         SUNWprivate
end

function        INTutil_snprintf
arch            all
version         SUNWprivate
end

function        INTutil_sprintf
arch            all
version         SUNWprivate
end

function        INTutil_str_time_equal
arch            all
version         SUNWprivate
end

function        INTutil_str_time_expired
arch            all
version         SUNWprivate
end

function        INTutil_strerror
arch            all
version         SUNWprivate
end

function        INTutil_strftime
arch            all
version         SUNWprivate
end

function        INTutil_strtok
arch            all
version         SUNWprivate
end

function        INTutil_time_equal
arch            all
version         SUNWprivate
end

function        INTutil_uri_escape
arch            all
version         SUNWprivate
end

function        INTutil_uri_is_evil
arch            all
version         SUNWprivate
end

function        INTutil_uri_is_evil_internal
arch            all
version         SUNWprivate
end

function        INTutil_uri_parse
arch            all
version         SUNWprivate
end

function        INTutil_uri_strip_params
arch            all
version         SUNWprivate
end

function        INTutil_uri_unescape
arch            all
version         SUNWprivate
end

function        INTutil_uri_unescape_plus
arch            all
version         SUNWprivate
end

function        INTutil_uri_unescape_strict
arch            all
version         SUNWprivate
end

function        INTutil_url_escape
arch            all
version         SUNWprivate
end

function        INTutil_vsnprintf
arch            all
version         SUNWprivate
end

function        INTutil_vsprintf
arch            all
version         SUNWprivate
end

function        INTutil_waitpid
arch            all
version         SUNWprivate
end

function        INTvs_alloc_slot
arch            all
version         SUNWprivate
end

function        INTvs_directive_register_cb
arch            all
version         SUNWprivate
end

function        INTvs_find_ext_type
arch            all
version         SUNWprivate
end

function        INTvs_get_acllist
arch            all
version         SUNWprivate
end

function        INTvs_get_data
arch            all
version         SUNWprivate
end

function        INTvs_get_default_httpd_object
arch            all
version         SUNWprivate
end

function        INTvs_get_doc_root
arch            all
version         SUNWprivate
end

function        INTvs_get_httpd_objset
arch            all
version         SUNWprivate
end

function        INTvs_get_id
arch            all
version         SUNWprivate
end

function        INTvs_get_mime_type
arch            all
version         SUNWprivate
end

function        INTvs_is_default_vs
arch            all
version         SUNWprivate
end

function        INTvs_lookup_config_var
arch            all
version         SUNWprivate
end

function        INTvs_register_cb
arch            all
version         SUNWprivate
end

function        INTvs_set_acllist
arch            all
version         SUNWprivate
end

function        INTvs_set_data
arch            all
version         SUNWprivate
end

function        INTvs_substitute_vars
arch            all
version         SUNWprivate
end

function        INTvs_translate_uri
arch            all
version         SUNWprivate
end

function        IncrementRecursionDepth
arch            all
version         SUNWprivate
end

function        InitThreadMallocKey
arch            all
version         SUNWprivate
end

function        IsCurrentTemplateNSPlugin
arch            all
version         SUNWprivate
end

function        LASCipherEval
arch            all
version         SUNWprivate
end

function        LASDayOfWeekEval
arch            all
version         SUNWprivate
end

function        LASDayOfWeekFlush
arch            all
version         SUNWprivate
end

function        LASDnsEval
arch            all
version         SUNWprivate
end

function        LASDnsFlush
arch            all
version         SUNWprivate
end

function        LASDnsGetter
arch            all
version         SUNWprivate
end

function        LASGroupEval
arch            all
version         SUNWprivate
end

function        LASIpEval
arch            all
version         SUNWprivate
end

function        LASIpFlush
arch            all
version         SUNWprivate
end

function        LASIpGetter
arch            all
version         SUNWprivate
end

function        LASProgramEval
arch            all
version         SUNWprivate
end

function        LASRoleEval
arch            all
version         SUNWprivate
end

function        LASSSLEval
arch            all
version         SUNWprivate
end

function        LASSSLFlush
arch            all
version         SUNWprivate
end

function        LASTimeOfDayEval
arch            all
version         SUNWprivate
end

function        LASTimeOfDayFlush
arch            all
version         SUNWprivate
end

function        LASUserEval
arch            all
version         SUNWprivate
end

function        LA_EndHtml
arch            all
version         SUNWprivate
end

function        LA_PrintBody
arch            all
version         SUNWprivate
end

function        LA_PrintButtons
arch            all
version         SUNWprivate
end

function        LA_PrintHead
arch            all
version         SUNWprivate
end

function        LA_PrintHeader
arch            all
version         SUNWprivate
end

function        LA_PrintHelpButton
arch            all
version         SUNWprivate
end

function        LA_PrintSubheader
arch            all
version         SUNWprivate
end

function        LA_StartHtml
arch            all
version         SUNWprivate
end

function        NSKW_CreateNamespace
arch            all
version         SUNWprivate
end

function        NSKW_DefineKeyword
arch            all
version         SUNWprivate
end

function        NSKW_DestroyNamespace
arch            all
version         SUNWprivate
end

function        NSKW_GetInfo
arch            all
version         SUNWprivate
end

function        NSKW_HashKeyword
arch            all
version         SUNWprivate
end

function        NSKW_LookupKeyword
arch            all
version         SUNWprivate
end

function        NSSInitAdmin
arch            all
version         SUNWprivate
end

function        PListAssignValue
arch            all
version         SUNWprivate
end

function        PListCreate
arch            all
version         SUNWprivate
end

function        PListDefProp
arch            all
version         SUNWprivate
end

function        PListDeleteProp
arch            all
version         SUNWprivate
end

function        PListDestroy
arch            all
version         SUNWprivate
end

function        PListDuplicate
arch            all
version         SUNWprivate
end

function        PListEnumerate
arch            all
version         SUNWprivate
end

function        PListFindValue
arch            all
version         SUNWprivate
end

function        PListGetPool
arch            all
version         SUNWprivate
end

function        PListGetValue
arch            all
version         SUNWprivate
end

function        PListInitProp
arch            all
version         SUNWprivate
end

function        PListNameProp
arch            all
version         SUNWprivate
end

function        PListNew
arch            all
version         SUNWprivate
end

function        PListSetType
arch            all
version         SUNWprivate
end

function        PListSetValue
arch            all
version         SUNWprivate
end

function        SetMtaHost
arch            all
version         SUNWprivate
end

function        SetNntpHost
arch            all
version         SUNWprivate
end

function        ValidatePort
arch            all
version         SUNWprivate
end

function        __1cMStringParserJgetNVPair6Mppc2_1_
arch            all
version         SUNWprivate
end

function        __1cMStringParserJsetString6Mpc_v_
arch            all
version         SUNWprivate
end

function        __1cMStringParserMsetSeperator6Mkc_v_
arch            all
version         SUNWprivate
end

function        __1cMStringParser2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cMStringParser2t6M_v_
arch            all
version         SUNWprivate
end

function        __1cKFileParserJwriteFile6M_i_
arch            all
version         SUNWprivate
end

function        __1cKFileParserLinsertLines6Mppcii_i_
arch            all
version         SUNWprivate
end

function        __1cKFileParserLdeleteLines6Mi_i_
arch            all
version         SUNWprivate
end

function        __1cKFileParserSgetNextLineFromBuf6M_pc_
arch            all
version         SUNWprivate
end

function        __1cKFileParserOgetCommentChar6M_c_
arch            all
version         SUNWprivate
end

function        __1cKFileParserKresetIndex6Mi_v_
arch            all
version         SUNWprivate
end

function        __1cKFileParserIreadFile6M_i_
arch            all
version         SUNWprivate
end

function        __1cKFileParserMskipComments6Mi_v_
arch            all
version         SUNWprivate
end

function        __1cKFileParserIopenFile6Mpkc2_i_
arch            all
version         SUNWprivate
end

function        __1cKFileParser2t6M_v_
arch            all
version         SUNWprivate
end

function        __1cKFileParserLreplaceLine6Mpc_i_
arch            all
version         SUNWprivate
end

function        __1cKFileParserLreplaceLine6Mpc1_i_
arch            all
version         SUNWprivate
end

function        __1cKFileParserSreplaceCurrentLine6Mpc_i_
arch            all
version         SUNWprivate
end

function        __1cDstdbG__RTTI__1nDstdIios_baseHfailure__
arch            all
version         SUNWprivate
end

function        __1cDstdFctype4Cc_2t6Mpkn0AKctype_baseEmask_bI_v_
arch            sparc, i386
version         SUNWprivate
end

function        __1cDstdFctype4Cc_Cid_
arch            all
version         SUNWprivate
end

function        __1cDstdGlocaleP__make_explicit6kMrkn0BCid_bipFipkcI_pnH__rwstdJfacet_imp__9A_
arch            sparc, i386
version         SUNWprivate
end

function        __1cDstdIios_baseHfailure2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cDstdIios_baseHfailure2t6Mrkn0AMbasic_string4Ccn0ALchar_traits4Cc__n0AJallocator4Cc_____v_
arch            all
version         SUNWprivate
end

function        __1cDstdMbasic_string4Ccn0ALchar_traits4Cc__n0AJallocator4Cc___2t6Mpkcrkn0C__v_
arch            all
version         SUNWprivate
end

function        __1cDstdMbasic_string4Ccn0ALchar_traits4Cc__n0AJallocator4Cc___J__nullref_
arch            all
version         SUNWprivate
end

function        __1cDstdMctype_byname4Cc_2t6MpkcI_v_
arch            sparc, i386
version         SUNWprivate
end

function        __1cDstdNbasic_ostream4Ccn0ALchar_traits4Cc___Fflush6M_r1_
arch            all
version         SUNWprivate
end

function        __1cDstdObasic_ifstream4Ccn0ALchar_traits4Cc___2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cDstdObasic_ifstream4Ccn0ALchar_traits4Cc___2t6Mpkcil_v_
arch            all
version         SUNWprivate
end

function        __1cDstdObasic_ifstream4Ccn0ALchar_traits4Cc___Fclose6M_v_
arch            all
version         SUNWprivate
end

function        __1cH__rwstdPrwse_badbit_set_
arch            all
version         SUNWprivate
end

function        __1cH__rwstdPrwse_eofbit_set_
arch            all
version         SUNWprivate
end

function        __1cH__rwstdQrwse_failbit_set_
arch            all
version         SUNWprivate
end

function        __1cH__rwstdRexcept_msg_string2t6MIE_v_
arch            all
version         SUNWprivate
end

function        __1cDstd2l6Frn0ANbasic_ostream4Ccn0ALchar_traits4Cc____c_2_
arch            all
version         SUNWprivate
end

function        __1cDstd2l6Frn0ANbasic_ostream4Ccn0ALchar_traits4Cc____pkc_2_
arch            all
version         SUNWprivate
end

function        __1cDstdNbasic_ostream4Ccn0ALchar_traits4Cc___2l6MH_r1_
arch            all
version         SUNWprivate
end

function        __1cDstdNbasic_ostream4Ccn0ALchar_traits4Cc___2l6MpFrn0AIios_base__3_r1_
arch            all
version         SUNWprivate
end

function        __1cDstdNbasic_ostream4Ccn0ALchar_traits4Cc___Fflush6M_r1_
arch            all
version         SUNWprivate
end

function        __1cHSemPoolEinit6Fi_v_
arch            all
version         SUNWprivate
end

function        __1cHSemPoolDget6Fpkc_i_
arch            all
version         SUNWprivate
end

function        __1cHSemPoolDadd6Fpkci_i_
arch            all
version         SUNWprivate
end

function        _fini
arch            all
version         SUNWprivate
end

function        _init
arch            all
version         SUNWprivate
end

#FileRealm class of file acl
function        __1cJFileRealm2t6Mpkc_v_
arch            all
version         SUNWprivate
end

function        __1cJFileRealm2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cJFileRealmEload6Mpkc2_i_
arch            all
version         SUNWprivate
end

function        __1cJFileRealmEsave6Mpkc_i_
arch            all
version         SUNWprivate
end

function        __1cJFileRealmEfind6Mpkc_pnNFileRealmUser__
arch            all
version         SUNWprivate
end

function        __1cJFileRealmDadd6MpknNFileRealmUser__v_
arch            all
version         SUNWprivate
end

function        __1cJFileRealmElist6MpFpnNFileRealmUser__v_v_
arch            all
version         SUNWprivate
end

function        __1cJFileRealmGremove6MpnNFileRealmUser__v_
arch            all
version         SUNWprivate
end


#FileRealmUser class of file acl
function        __1cNFileRealmUser2t6Mpkc22_v_
arch            all
version         SUNWprivate
end

function        __1cNFileRealmUser2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cNFileRealmUserJgetGroups6M_nINSString__
arch            all
version         SUNWprivate
end

function        __1cNFileRealmUserMgetHashedPwd6M_nINSString__
arch            all
version         SUNWprivate
end

function        __1cNFileRealmUserHgetName6M_nINSString__
arch            all
version         SUNWprivate
end

function        __1cNFileRealmUserJsetGroups6Mpkc_v_
arch            all
version         SUNWprivate
end

function        __1cNFileRealmUserLsetPassword6Mpkc_v_
arch            all
version         SUNWprivate
end

function        __1cKsshaPasswd6Fpkcki1rnINSString__v_
arch            all
version         SUNWprivate
end

function        __1cJha1Passwd6Fpkc11rnINSString__v_
arch            all
version         SUNWprivate
end

function        __1cNhtcryptPasswd6Fpkc1rnINSString__v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairs2G6Mrk0_r0_
arch            all
version         SUNWprivate
end

function        __1cHNVPairs2T5B6M_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairs2t5B6Mi_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairs2t5B6Mrk0_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairs2T6M_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairs2t6Mi_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairs2t6Mrk0_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairsDdup6kM_pnGpblock__
arch            all
version         SUNWprivate
end

function        __1cHNVPairsG__vtbl_
arch            all
version         SUNWprivate
end

function        __1cHNVPairsHaddPair6Mpkc2_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairsHaddPair6Mpkci_v_
arch            all
version         SUNWprivate
end

function        __1cHNVPairsIaddPairs6Mpkc_i_
arch            all
version         SUNWprivate
end

function        __1cHNVPairsIfindName6kMi_pkc_
arch            all
version         SUNWprivate
end

function        __1cHNVPairsJfindValue6kMpkc_2_
arch            all
version         SUNWprivate
end

function        __1cHNVPairsKremovePair6Mpkc_i_
arch            all
version         SUNWprivate
end

function        __RTTI__1CpknHNVPairs_
arch            all
version         SUNWprivate
end

function        __RTTI__1CpnHNVPairs_
arch            all
version         SUNWprivate
end

function        __RTTI__1nHNVPairs_
arch            all
version         SUNWprivate
end

function        ap_unescape_url
arch            all
version         SUNWprivate
end

function        ap_getparents
arch            all
version         SUNWprivate
end

function        apr_hash_make
arch            all
version         SUNWprivate
end

function        apr_hash_copy
arch            all
version         SUNWprivate
end

function        apr_hash_set
arch            all
version         SUNWprivate
end

function        apr_hash_get
arch            all
version         SUNWprivate
end

function        apr_hash_first
arch            all
version         SUNWprivate
end

function        apr_hash_next
arch            all
version         SUNWprivate
end

function        apr_hash_this
arch            all
version         SUNWprivate
end

function        apr_hash_merge
arch            all
version         SUNWprivate
end

function        apr_file_perms_set
arch            all
version         SUNWprivate
end

function        apr_fill_file_info
arch            all
version         SUNWprivate
end
function        apr_file_open
arch            all
version         SUNWprivate
end
function        apr_file_read
arch            all
version         SUNWprivate
end
function        apr_stat
arch            all
version         SUNWprivate
end

function        apr_lstat
arch            all
version         SUNWprivate
end

function        apr_file_remove
arch            all
version         SUNWprivate
end

function        apr_file_rename
arch            all
version         SUNWprivate
end

function        apr_file_seek
arch            all
version         SUNWprivate
end

function        apr_file_write
arch            all
version         SUNWprivate
end

function        apr_file_write_full
arch            all
version         SUNWprivate
end

function        apr_file_close
arch            all
version         SUNWprivate
end

function        apr_dir_read
arch            all
version         SUNWprivate
end

function        apr_dir_make
arch            all
version         SUNWprivate
end

function        apr_dir_remove
arch            all
version         SUNWprivate
end

function        apr_dir_close
arch            all
version         SUNWprivate
end

function        apr_dir_open
arch            all
version         SUNWprivate
end

function        apr_system_stat
arch            all
version         SUNWprivate
end

function        apr_system_stat_exists
arch            all
version         SUNWprivate
end

function        apr_pstrmemdup
arch            all
version         SUNWprivate
end

function        apr_psprintf
arch            all
version         SUNWprivate
end

function        apr_pstrcat
arch            all
version         SUNWprivate
end

function        apr_array_make
arch            all
version         SUNWprivate
end

function        apr_array_push
arch            all
version         SUNWprivate
end

function        ap_make_dirstr_parent
arch            all
version         SUNWprivate
end

function        ap_escape_html
arch            all
version         SUNWprivate
end

function        apr_pmemdup
arch            all
version         SUNWprivate
end

function        ap_getword_white
arch            all
version         SUNWprivate
end
