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

#define LIBRARY_NAME "fastcgi"

static char dbtFastcgiId[] = "$DBT: fastcgi referenced v1 $";

#include "i18n.h"

/* Message IDs reserved for this file: FCGI1000-FCGI1999 */
BEGIN_STR(fastcgi)
    ResDef(DBT_LibraryID_, -1, dbtFastcgiId)
    ResDef(DBT_build_info, 0, "FCGI1000: %s")
    ResDef(DBT_init_fastcgi_not_called, 1, "FCGI1001: init-fastcgi has not been called")
    ResDef(DBT_init_fastcgi_ignored, 2, "FCGI1002: init-fastcgi directive ignored")
    ResDef(DBT_error_connecting_to_X_Y, 3, "FCGI1003: error connecting to %s (%s)")
    ResDef(DBT_errorurl_not_specified, 4, "FCGI1004: error-url is not specified")
    ResDef(DBT_no_servers_defined, 5, "FCGI1005: no servers defined")
    ResDef(DBT_set_rlimit_failure, 6, "FCGI1006: error while setting rlimit values")
    ResDef(DBT_set_priority_failure, 7, "FCGI1007: error while setting priority")
    ResDef(DBT_set_group_failure, 8, "FCGI1008: could not set the application process group")
    ResDef(DBT_set_user_failure, 9, "FCGI1009: could not set the application process user")
    ResDef(DBT_chdir_failure, 10, "FCGI1010: could not change the working directory of the application process")
    ResDef(DBT_chroot_failure, 11, "FCGI1011: could not change the root directory of the application process")
    ResDef(DBT_invalid_param_value, 12, "FCGI1012: invalid parameter")
    ResDef(DBT_invalid_user, 13, "FCGI1013: invalid user name [%s] specified for %s")
    ResDef(DBT_invalid_group, 14, "FCGI1014: invalid group name %s specified for %s")
    ResDef(DBT_application_exec_failure, 15, "FCGI1015: execution of %s failed")
    ResDef(DBT_no_application_info, 16, "FCGI1016: application %s is not running")
    ResDef(DBT_application_socket_bind_error, 17, "FCGI1017: %s unable to bind to the specified address")
    ResDef(DBT_application_socket_listen_error, 18, "FCGI1018: listen failed for %s bind address")
    ResDef(DBT_incomplete_request, 19, "FCGI1019: incomplete request sent to Fastcgistub")
    ResDef(DBT_invalid_stub_version, 20, "FCGI1020: Fastcgistub supports only 1.0 request version")
    ResDef(DBT_overloaded_status, 21, "FCGI1021: application returned FCGI_OVERLOADED protocol status")
    ResDef(DBT_unknown_fcgi_role, 22, "FCGI1022: application could not service the request as unknown role information was sent")
    ResDef(DBT_no_server_available, 23, "FCGI1023: no application to process the request")
    ResDef(DBT_invalid_fcgi_response, 24, "FCGI1024: malformed header from %s")
    ResDef(DBT_fcgi_filter_file_open_error, 25, "FCGI1025: Unable to read the filter file %s")
    ResDef(DBT_semaphore_creation_failure, 26, "FCGI1026: Unable to create the lock for the Fastcgistub pid file")
    ResDef(DBT_stub_pid_create_error, 27, "FCGI1027: Error occurred when trying to create the Fastcgistub pid file")
    ResDef(DBT_error_sending_request_X, 28, "FCGI1028: error sending request (%s)")
    ResDef(DBT_request_body_timeout, 29, "FCGI1029: timed out waiting for request body")
    ResDef(DBT_response_header_timeout_X, 30, "FCGI1030: timed out waiting for response header from %s")
    ResDef(DBT_response_body_timeout_X, 31, "FCGI1031: timed out waiting for response body from %s")
    ResDef(DBT_X_returned_error_Y, 32, "FCGI1032: %s returned error code %d")
    ResDef(DBT_X_invalid_response, 33, "FCGI1033: %s did not return a valid FastCGI response")
    ResDef(DBT_rewriting_location_X_from_Y_to_Z, 34, "FCGI1034: rewriting \"Location: %s\" from %s to \"Location: %s\"")
    ResDef(DBT_not_rewriting_location_X_from_Y, 35, "FCGI1035: not rewriting \"Location: %s\" from %s")
    ResDef(DBT_no_bind_path, 36, "FCGI1036: \"bind-path\" value must be specified for Windows")
    ResDef(DBT_no_app_bind_path, 37, "FCGI1037: both \"app-path\" and \"bind-path\" values are missing")
    ResDef(DBT_missing_params, 38, "FCGI1038: Required parameters for application config are missing")
    ResDef(DBT_stub_start_error, 39, "FCGI1039: Error while starting Fastcgistub")
    ResDef(DBT_bind_error, 40, "FCGI1040: Unable to bind to address")
    ResDef(DBT_listen_error, 41, "FCGI1041: Listen error")
    ResDef(DBT_stub_accept_error, 42, "FCGI1042: Error occurred during Accept within Fastcgistub")
    ResDef(DBT_creating_fcgi_header, 43, "FCGI1043: Preparing Fastcgi Header data")
    ResDef(DBT_scan_http_response_header, 44, "FCGI1044: Scanning response headers from Fastcgi application")
    ResDef(DBT_parsing_auth_header, 45, "FCGI1045: Parsing authorizer application response headers")
    ResDef(DBT_invalid_response, 46, "FCGI1046: Invalid fastcgi response header - %s")
    ResDef(DBT_invalid_location_header, 47, "FCGI1047: Invalid header value \"Location: %s\"")
    ResDef(DBT_parsing_http_response_header, 48, "FCGI1048: Parsing HTTP response headers")
    ResDef(DBT_parsing_fcgi_response, 49, "FCGI1049: Parsing fastcgi application response of bytes %d")
    ResDef(DBT_invalid_header_version, 50, "FCGI1050: Received invalid fastcgi header version - %d")
    ResDef(DBT_invalid_header_type, 51, "FCGI1051: Received invalid fastcgi header type - %d")
    ResDef(DBT_formating_request, 52, "FCGI1052: Formatting request data to be sent to fastcgi application")
    ResDef(DBT_invalid_end_request_record, 53, "FCGI1053: Received invalid EndRequest record of size %d")
    ResDef(DBT_cannot_multiplex, 54, "FCGI1054: Application does not support multiplexing")
    ResDef(DBT_pid_file_creation_failure, 55, "FCGI1055: FastcgiStub pid file creation failure")
    ResDef(DBT_stub_not_responding, 56, "FCGI1056: FastcgiStub process is not responding")
    ResDef(DBT_pipe_create_error, 57, "FCGI1057: Unable to create pipe")
    ResDef(DBT_fork_error, 58, "FCGI1058: Process creation failure")
    ResDef(DBT_stub_exec_failure, 59, "FCGI1059: Cannot start FastcgiStub")
    ResDef(DBT_socket_create_error, 60, "FCGI1060: Unable to create socket")
    ResDef(DBT_stub_socket_connect_error, 61, "FCGI1061: Unable to connect to FastcgiStub")
    ResDef(DBT_stat_failure, 62, "FCGI1062: Application (%s) path is invalid or not accessible")
    ResDef(DBT_no_permission, 63, "FCGI1063: Not enough permissions")
    ResDef(DBT_no_exec_permission, 64, "FCGI1064: No execute permission")
    ResDef(DBT_write_other, 65, "FCGI1065: Others have write permission")
    ResDef(DBT_stub_request_send_error, 66, "FCGI1066: Error occurred while sending request to FastcgiStub")
    ResDef(DBT_memory_allocation_failure, 67, "FCGI1067: Memory allocation failure")
    ResDef(DBT_request_creation_error, 68, "FCGI1068: FastcgiStub request creation failure")
    ResDef(DBT_invalid_stub_response, 69, "FCGI1069: invalid FastcgiStub response")
    ResDef(DBT_unknow_stub_request_type, 70, "FCGI1070: unknown FastcgiStub request type")
    ResDef(DBT_internal_error, 71, "FCGI1071: internal Error")
    ResDef(DBT_process_exists, 72, "FCGI1072: application process is already running")
    ResDef(DBT_exceeded_number_of_retries, 73, "FCGI1073: Unable to service the request even after trying %d times")
    ResDef(DBT_invalid_min_parameter, 74, "FCGI1074: invalid min-procs parameter value (%s) - setting it to the default value %s")
    ResDef(DBT_invalid_max_parameter, 75, "FCGI1075: invalid max-procs parameter value (%s) - setting it to the default value %s")
    ResDef(DBT_invalid_nice_parameter, 76, "FCGI1076: invalid nice parameter value (%s) - setting it to the default value %s")
    ResDef(DBT_invalid_listen_q_parameter, 77, "FCGI1077: invalid listen-queue parameter value (%s) - setting it to the default value %s")
    ResDef(DBT_invalid_req_retry_parameter, 78, "FCGI1078: invalid req-retry parameter value (%s) - setting it to the default value %d")
    ResDef(DBT_stub_stat_failure, 79, "FCGI1079: Unable to start Fastcgistub (%s) - stub path is invalid or not accessible")
    ResDef(DBT_application_stderr_msg, 80, "FCGI1080: application error: %s")
    ResDef(DBT_remote_connection_failure, 81, "FCGI1081: Unable to connect to the remote server at %s")
END_STR(fastcgi)
