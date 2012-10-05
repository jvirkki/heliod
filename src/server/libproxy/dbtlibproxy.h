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

/**************************************************************************/
/* CONFIDENTIAL AND PROPRIETARY SOURCE CODE                               */
/* OF NETSCAPE COMMUNICATIONS CORPORATION                                 */
/*                                                                        */
/* Copyright © 1996,1997 Netscape Communications Corporation.  All Rights */
/* Reserved.  Use of this Source Code is subject to the terms of the      */
/* applicable license agreement from Netscape Communications Corporation. */
/*                                                                        */
/* The copyright notice(s) in this Source Code does not indicate actual   */
/* or intended publication of this Source Code.                           */
/**************************************************************************/
#ifndef DBT_LIBPROXY
#define DBT_LIBPROXY
#define LIBRARY_NAME "libproxy"

static char dbtlibproxyid[] = "$DBT: libproxy referenced v1 $";

#include "i18n.h"

/* Message IDs reserved for this file: CORE7000-CORE7999, HTTP7000-HTTP7999 */
BEGIN_STR(libproxy)

/* error messages for the obj.conf cache functions */
    ResDef(DBT_cache_getconnectmode, 1, "CORE7001: invalid connect-mode: %s (expecting normal, fast-demo, or never)")
    ResDef(DBT_cache_authdoc, 2, "CORE7002: authenticated doc - forced refresh: %s") 


/* error messages for the open partiton/section */
    ResDef(DBT_cache_openpartition, 31, "CORE7031: error while opening partition: %s")
    ResDef(DBT_cache_opensection, 32, "CORE7022: error while opening section")
    ResDef(DBT_cache_opensectionMsg1, 33, "CORE7033: bad section index %d; expecting range 0..%d")
    ResDef(DBT_cache_opensectionMsg2, 34, "CORE7034: can't open dir %s")
    ResDef(DBT_cache_opensectionMsg3, 35, "CORE7035: bad sect.dim %d ; expecting dim between 0..8")
    ResDef(DBT_cache_opensectionMsg4, 36, "CORE7036: cache section with wrong dim %s %s, dim is %d, corresponding sections = %d (actual dim=%d, sections=%d)")
    ResDef(DBT_cache_opensectionMsg5, 37, "CORE7037: invalid section name %s found; with dim %d the section name length should be %d (%d digits in the end)")
    ResDef(DBT_cache_opensectionMsg6, 38, "CORE7038: bad section index %d; expecting 0..%d")
/*    ResDef(DBT_cache_capacity, 39, "CORE7039: invalid cache capacity value - can take only the following values: 125, 250, 500, 1000, 2000, 4000, 8000, 16000, 32000 ") */
    ResDef(DBT_cache_partitionstatfail, 60, "CORE7060: can't stat partition %s")
    ResDef(DBT_cache_disabled, 61, "CORE7061: cache %s disabled ")
    ResDef(DBT_cache_partitiondisabled, 62, "CORE7062: cache partition %s disabled ")
    ResDef(DBT_cache_maxpartitions, 63, "CORE7063: too many cache partitions; max %d" )


/* error messages regarding system calls used in caching */
    ResDef(DBT_cache_filesystemfull, 81, "CORE7081: partition %s filesystem full ")
    ResDef(DBT_cache_dumpmmapfail, 82, "CORE7082: mmap() for %s offset %ld size %ld failed (%s)")
    ResDef(DBT_cache_dumpunmapfail, 83, "CORE7083: munmap() for %s offset %ld size %ld failed (%s)")
    ResDef(DBT_cache_renamefail, 84, "CORE7084: could not rename %s as %s")
    ResDef(DBT_cache_closefilemapfail, 85, "CORE7085: closefilemap for %s failed")



/* error messages regarding cio/cif */
    ResDef(DBT_cache_ciommapfail, 101, "CORE7101: mmap() for %s failed (%s)")
    ResDef(DBT_cache_cioclosefail, 102, "CORE7102: close of %s failed (%s)")
    ResDef(DBT_cache_ciomunmap, 103, "CORE7103: close of %s failed (%s)")
    ResDef(DBT_cache_cioopen, 104, "CORE7104: Bad CIF entry in %s (%s) deleted")
    ResDef(DBT_cache_ciofinishoutputMsg1, 105, "CORE7105: can't stat new cache file %s")
    ResDef(DBT_cache_ciofinishoutputMsg2, 106, "CORE7106: incorrect amount of data written to disk (fs full?)")
    ResDef(DBT_cache_cifwriteentryMsg1, 117, "CORE7117: filename is NULL")
    ResDef(DBT_cache_cifwriteentryMsg2, 118, "CORE7118: content type is NULL")
    ResDef(DBT_cache_cifwriteentryMsg3, 19, "CORE7119: url is NULL")
    ResDef(DBT_cache_cifwritefail, 130, "CORE7130: incomplete cache file removed for %s")


/* error messages regarding cache util functions */
    ResDef(DBT_cache_utilblkstodisk, 151, "CORE7151: can't open %s for writing ")
    ResDef(DBT_cache_utilblksfromdiskMsg1, 152, "CORE7152: can't open %s for reading ")
    ResDef(DBT_cache_utilblksfromdiskMsg2, 153, "CORE7153: can't read from %s")
    ResDef(DBT_cache_utilputstringtofile, 184, "CORE7184: can't open file %s for writing")



/* error messages regarding cache functions in retrieve/pobj  200-400*/
    ResDef(DBT_cache_dirname, 200, "CORE7200: section with section index %d  for the dir %s does not exist")
    ResDef(DBT_writetocachefileMsg1, 201, "CORE7201: file %s becoming too big to cache; caching aborted")
    ResDef(DBT_writetocachefileMsg2, 202, "CORE7202: fd for cache file %s is NULL")
    ResDef(DBT_writetocachefileMsg3, 203, "CORE7203: %s too large file for caching (size %d KB)")
    ResDef(DBT_writetocachefileMsg4, 204, "CORE7204: %s too small file for caching (size %d KB)")
    ResDef(DBT_writetocachefileMsg5, 205, "CORE7205: document %s will not be cached, expired on %s")
    ResDef(DBT_writetocachefileMsg6, 206, "CORE7206: no last-mod time, %s will not be cached")
    ResDef(DBT_writetocachefileMsg7, 207, "CORE7207: document %s will not be cached, incorrect last-mod time %s")

    ResDef(DBT_cache_validlm, 275, "CORE7275: last-modified in future (not caching): %s")
/*error messages regarding batch update. 500-600*/
    ResDef(DBT_no_conf, 500, "CORE7500: unable to stat config file %s")
    ResDef(DBT_invalid_conf, 501, "CORE7501: invalid configuration in %s")
    ResDef(DBT_invalidxml_conf, 502, "CORE7502: invalid xml file: %s")
/*net*/
    ResDef(DBT_bu_net_noip, 510, "CORE7510: could not find IP-address for host  %s:%d")
    ResDef(DBT_bu_net_sock, 511, "CORE7511: invalid socket connection to %s:%d")
    ResDef(DBT_bu_end_traversal, 515, "CORE7515: traversal ceased (past end time.)")
/*config*/
    ResDef(DBT_bu_bad_regex, 516, "CORE7516: invalid regex (%s) in parsing configuration.")
    ResDef(DBT_bu_nested_obj, 517, "CORE7517: nested batch update objects are not allowed.")
    ResDef(DBT_bu_unknown_directive, 518, "CORE7518: unknown batch update directive (%s).")
    ResDef(DBT_bu_conf_eof, 519, "CORE7519: unexpected end of file for batch update config file.")
    ResDef(DBT_bu_multiple_directive, 520, "CORE7520: multiple batch update directive (%s).")
    ResDef(DBT_bu_bogus_time_directive, 521, "CORE7521: bogus batch update time directive.")
    ResDef(DBT_bu_bogus_day_directive, 522, "CORE7522: bogus batch update day directive.")
/*error messages regarding cachegc*/
    ResDef(DBT_cache_unknown, 550, "CORE7550: cache load failed.")
    ResDef(DBT_cache_lookup, 551, "CORE7551: cache lookup (%s) failed.")
    ResDef(DBT_gc_lock, 552, "CORE7552: gc exclusive lock failed.")
    ResDef(DBT_gc_init, 553, "CORE7553: gc init failed.")
    ResDef(DBT_gc_thread, 554, "CORE7554: gc internal thread error.")
    ResDef(DBT_open_partition, 555, "CORE7555: open partition (%s) failed.")
    ResDef(DBT_invalid_partition, 556, "CORE7556: invalid partition path: (%s).")
    ResDef(DBT_open_dir, 557, "CORE7557: open directory failed: (%s).")
    ResDef(DBT_move_file, 557, "CORE7557: move file failed: (%s to %s).")
    ResDef(DBT_open_file, 558, "CORE7558: open file failed: (%s).")

/*CLI stuff*/
    ResDef(DBT_mandatory_option, 600, "CORE7600: mandatory option (%s) not provided.")
    ResDef(DBT_internal_error, 601, "CORE7601: %s was not completed successfully.")
    ResDef(DBT_completed, 602, "CORE7602: %s completed successfully.")
    ResDef(DBT_invalidnumber, 603, "CORE7603: invalid number (%s).")

    ResDef(DBT_GopherIndex, 604,
"<TITLE>Gopher Index %.256s</TITLE><H1>%.256s <BR>Gopher Search</H1>\nThis is a searchable Gopher index.\n\
Use the search function of your browser to enter search terms.\n<ISINDEX>")

    ResDef(DBT_CSOSearch, 605,
"<TITLE>CSO Search of %.256s</TITLE><H1>%.256s CSO Search</H1>\nA CSO database usually contains a phonebook or directory.\n\
Use the search function of your browser to enter search terms.\n<ISINDEX>")
    ResDef(DBT_ServerReturnedNoData, 606, "The remote computer returned no data")

/*protocols*/
    ResDef(DBT_ftp_nologin,        1001, "<TITLE>FTP Error</TITLE>\n<H1>FTP Error</H1>\n<h2>Could not login to FTP server</h2>\n<PRE>")
    ResDef(DBT_ftp_errortransfer, 1002, "<TITLE>FTP Error</TITLE>\n<H1>FTP Error</H1>\n<h2>FTP Transfer failed:</h2>\n<PRE>")
    ResDef(DBT_ftp_error,        1003, "<TITLE>FTP Error</TITLE>\n<H1>FTP Error</H1>\n<h2>Unable to continue</h2>\n<PRE>")

/* Misc SAF parameter validations, etc. */
    ResDef(DBT_need_server_or_hostname_and_port, 700, "CORE7700: missing parameter (need server or hostname and port)")
    ResDef(DBT_invalid_X_value_Y, 701, "CORE7701: invalid %s value: %s")
    ResDef(DBT_invalid_X_value_Y_expected_boolean, 702, "CORE7702: invalid %s value: %s (expected boolean)")
    ResDef(DBT_invalid_X_value_Y_expected_seconds, 703, "CORE7703: invalid %s value: %s (expected interval in seconds)")
    ResDef(DBT_need_from_and_to, 704, "CORE7704: missing parameter (need from and to)")
    ResDef(DBT_need_hostname_and_port, 705, "CORE7705: missing parameter (need hostname and port)")
    ResDef(DBT_invalid_url_X, 706, "CORE7706: invalid URL: %s")
    ResDef(DBT_error_creating_thread_because_X, 707, "CORE7707: error creating thread (%s)")
    ResDef(DBT_cert_X_not_valid_client_cert_because_Y, 708, "CORE7708: certificate %s does not appear to be a valid client certificate (%s)")

/* system_errmsg() error strings */
    ResDef(DBT_unknown_cert_X, 730, "Unable to find certificate %s")
    ResDef(DBT_no_key_for_X_because_Y, 731, "Unable to find the private key for certificate %s: %s")
    ResDef(DBT_reached_max_X_servers, 732, "Reached maximum of %d servers")
    ResDef(DBT_error_running_connect, 733, "Error processing Connect directives")
    ResDef(DBT_src_dst_match, 734, "Source and destination addresses match")
    ResDef(DBT_reached_max_X_conn_to_Y_Z, 735, "Reached maximum of %d connections to %s:%d")

/* proxyerror_channel_error log message */
    ResDef(DBT_unable_to_contact_X_Y_because_Z, 740, "CORE7740: unable to contact %s:%d (%s)")
    ResDef(DBT_cannot_open_bong_file, 741, "CORE7741: error opening %s (%s)")
    ResDef(DBT_cannot_open_bong_file_buffer, 742, "CORE7742: error opening buffer from %s (%s)")

/* HTTP client */
    ResDef(DBT_server_X_online, 750, "HTTP7750: server %s online")
    ResDef(DBT_server_X_offline, 751, "HTTP7751: server %s offline")
    ResDef(DBT_sticky_req_for_unknown_jroute_X, 752, "HTTP7752: received sticky request for unrecognized server (jroute %.*s)")
    ResDef(DBT_sticky_req_for_offline_X_using_Y, 753, "HTTP7753: received sticky request for offline server %s (using server %s instead)")
    ResDef(DBT_res_hdr_max_X, 754, "HTTP7754: response header too large (maximum %d bytes)")
    ResDef(DBT_incomplete_res_hdr, 755, "HTTP7755: received incomplete response header")
    ResDef(DBT_invalid_res_hdr, 756, "HTTP7756: received invalid response header")
    ResDef(DBT_chunked_to_pre_http11, 757, "HTTP7757: client attempted to send chunked request message body to pre-HTTP/1.1 server")
    ResDef(DBT_error_send_req_because_X, 758, "HTTP7758: error sending request (%s)")
    ResDef(DBT_bogus_chunked_res_body, 759, "HTTP7759: received invalid chunked response body")
    ResDef(DBT_error_read_req_body_client_closed, 760, "HTTP7760: error reading request body (Client closed connection)")
    ResDef(DBT_error_read_req_body_because_X, 761, "HTTP7761: error reading request body (%s)")
    ResDef(DBT_error_send_res_because_X, 762, "HTTP7762: error sending response (%s)")
    ResDef(DBT_client_disconn_before_res_begin, 763, "HTTP7763: client disconnected before remote server responded")
    ResDef(DBT_client_disconn_before_res_done, 764, "HTTP7764: client disconnected before response was complete")
    ResDef(DBT_res_hdr_server_closed, 765, "HTTP7765: error reading response header (Server closed connection)")
    ResDef(DBT_res_hdr_because_X, 766, "HTTP7766: error reading response header (%s)")
    ResDef(DBT_res_body_server_closed, 767, "HTTP7767: error reading response body (Server closed connection)")
    ResDef(DBT_res_body_because_X, 768, "HTTP7768: error reading response body (%s)")
    ResDef(DBT_req_body_because_X, 769, "HTTP7769: error sending request body (%s)")
    ResDef(DBT_poll_error_because_X, 770, "HTTTP7770: poll error (%s)")
    ResDef(DBT_unexpected_error, 771, "HTTP7771: unexpected error")
    ResDef(DBT_req_hdr_too_large_max_X, 772, "HTTP7772: request header too large (maximum %d bytes)")
    ResDef(DBT_dup_cl_res_header, 773, "HTTP7773: received duplicate Content-Length response header")

/* HTML error pages */
    ResDef(DBT_html_error_content_type, 800, "text/html")
    ResDef(DBT_html_error_X_Y_Z, 801, "<HTML>\n<HEAD><TITLE>The Proxy Was Unable to Fulfill Your Request</TITLE></HEAD>\n<BODY>\n<H1>The Proxy Was Unable to Fulfill Your Request</H1>\n<HR SIZE=\"1\">\n<P>%s</P>\n<P>%s</P>\n<HR SIZE=\"1\">\n<P><ADDRESS>%s</ADDRESS></P>\n</BODY>\n</HTML>\n")
    ResDef(DBT_product_X_at_Y_Z, 802, "%s at %s:%d")
    ResDef(DBT_check_url, 803, "If you typed the web address, please check for typographical errors. If the address is correct and later attempts to access this website are still unsuccessful, you may wish to contact the website's administrator.")
    ResDef(DBT_try_later, 804, "The website may be temporarily unavailable. If later attempts to access this website are still unsuccessful, you may wish to contact the website's administrator.")
    ResDef(DBT_unexpected_error_X_Y_X_Y_because_Z, 805, "The proxy was unable to fulfill your request because the computer at %.128s:%d does not appear to be functioning properly. While attempting to communicate with %.128s:%d, the proxy encountered the following error: %s")
    ResDef(DBT_hostname_X_not_found, 806, "The proxy was unable to fulfill your request because it could not find a directory entry for the website %.128s.")
    ResDef(DBT_connect_timeout_X_Y, 807, "The proxy was unable to fulfill your request because the computer at %.128s:%d did not respond to the proxy's attempt(s) to connect.")
    ResDef(DBT_connect_refused_X_Y, 808, "The proxy was unable to fulfill your request because the computer at %.128s:%d refused the proxy's connection(s).")
    ResDef(DBT_connect_X_Y_failed_because_Z, 809, "The proxy was unable to fulfill your request because it could not contact the computer at %.128s:%d (%s).")
    ResDef(DBT_mime_type_blocked, 810, "The mime type is blocked.")
    ResDef(DBT_html_error_mime_type_blocked, 811, "<H1>Blocked</H1>\n<BLOCKQUOTE><B>\n<HR SIZE=4><P>\nThe requested item has a MIME type that is blocked\nby the proxy.<P>")

/* dns cache errors */
    ResDef(DBT_hostDnsCache_init_hashSize_lt_zero, 850, "CORE7850: hash_size <= 0, using %d")
    ResDef(DBT_hostDnsCache_init_caheSize_lt_zero, 851, "CORE7851: cash_size <= %d, using %d")
    ResDef(DBT_hostDnsCache_init_cacheSizeTooLarge, 852, "CORE7852: cash_size %d is too large, using %d.")
    ResDef(DBT_hostDnsCache_init_expireTime_lt_zero, 853, "CORE7853: expire_time <= 0, using %d")
    ResDef(DBT_hostDnsCache_init_expireTimeTooLarge, 854, "CORE7854: expire_time %d is too large, using %d seconds.")
    ResDef(DBT_hostDnsCache_init_invalidNegativeDnsCache_value, 855, "CORE7855: invalid negative-dns-cache value, default is no")
    ResDef(DBT_hostDnsCache_init_errorCreatingDnsCache, 856, "CORE7856: Error creating dns cache")
    ResDef(DBT_hostDnsCache_insert_errorAllocatingEnt, 857, "CORE7857: Error allocating entry")
    ResDef(DBT_hostDnsCache_insert_mallocFailure, 858, "CORE7858: malloc failure")

/* common client errors */    /*reserv 900 - 925 */
    ResDef(DBT_common_unable_to_contact, 901, "unable to contact %s:%d (%s)")
    ResDef(DBT_common_invalid_url, 902, "invalid URL: %s")
    ResDef(DBT_common_no_hostname_or_address, 903, "invalid URL: %s")
    ResDef(DBT_common_unable_to_create_server_data_transfer, 904, "unable to create server for dcata transfer(%s)")
    ResDef(DBT_common_timed_out_server, 905, "timed out waiting for response from server")
    ResDef(DBT_common_client_disconnected, 906, "client disconnected")
    ResDef(DBT_common_server_disconnected, 907, "server disconnected")
    ResDef(DBT_common_timed_out_request_body_from_client, 908, "timed out waiting for request body from client")
    ResDef(DBT_common_timed_out_sending_to_server, 909, "timed out sending to server")
    ResDef(DBT_common_newlines_in_url, 910, "embedded newlines in URL %s")
    ResDef(DBT_common_timed_out_sending_to_client, 911, "timed out sending to client")

    ResDef(DBT_common_invalid_timeout, 925, "init-proxy: invalid timeout parameter")
    ResDef(DBT_common_invalid_pool_size, 926, "init-proxy: invalid pool-size parameter")

/* proxy filter functionality */
    ResDef(DBT_NeedStartTag, 951, "HTTP7951: missing parameter (need start tag)")
    ResDef(DBT_NeedEndTag, 952, "HTTP7952: missing parameter (need end tag)")
    ResDef(DBT_NeedMimeRegexp, 953, "HTTP7953: missing parameter (need MIME type regular expression)")
    ResDef(DBT_NeedFilterProgramPath, 954, "HTTP7954: missing parameter (need filter program path)")
    ResDef(DBT_CouldNotCreateFilterProcess, 955, "HTTP7955: could not create filter process (%s)")

/* socks routing */
    ResDef(DBT_socks_connect_error, 960, "HTTP7960: Could not connect to SOCKS server (%d)")
/* unsupported protocol */
    ResDef(DBT_unsupported_protocol, 999, "HTTP7999: %s not supported ")
END_STR(libproxy)
#endif
