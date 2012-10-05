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

/* nscperrors.c
 * Very crude error handling for nspr and libsec.
 */

#include "prerror.h"
#include "stdlib.h"

#define NSCP_NSPR_ERROR_BASE            (PR_NSPR_ERROR_BASE)
#if 0
#define NSCP_NSPR_MAX_ERROR             ((PR_MAX_ERROR) - 1)
#endif
#define NSCP_NSPR_MAX_ERROR             (NSCP_NSPR_ERROR_BASE + 75)
#define NSCP_LIBSEC_ERROR_BASE 		(-8192)
#define NSCP_LIBSEC_MAX_ERROR           (NSCP_LIBSEC_ERROR_BASE + 171)
#define NSCP_LIBSSL_ERROR_BASE 		(-12288)
#define NSCP_LIBSSL_MAX_ERROR           (NSCP_LIBSSL_ERROR_BASE + 112)

typedef struct nscp_error_t {
    int errorNumber;
    const char *errorString;
} nscp_error_t;

nscp_error_t nscp_nspr_errors[]  =  {
    {  0, "Out of memory" },
    {  1, "Bad file descriptor" },
    {  2, "Data temporarily not available" },
    {  3, "Access fault" },
    {  4, "Invalid method" },
    {  5, "Illegal access" },
    {  6, "Unknown error" },
    {  7, "Pending interrupt" },
    {  8, "Not implemented" },
    {  9, "IO error" },
    { 10, "IO timeout error" },
    { 11, "IO already pending error" },
    { 12, "Directory open error" },
    { 13, "Invalid Argument" },
    { 14, "Address not available" },
    { 15, "Address not supported" },
    { 16, "Already connected" },
    { 17, "Bad address" },
    { 18, "Address already in use" },
    { 19, "Connection refused" },
    { 20, "Network unreachable" },
    { 21, "Connection timed out" },
    { 22, "Not connected" },
    { 23, "Load library error" },
    { 24, "Unload library error" },
    { 25, "Find symbol error" },
    { 26, "Insufficient resources" },
    { 27, "Directory lookup error" },
    { 28, "Invalid thread private data key" },
    { 29, "PR_PROC_DESC_TABLE_FULL_ERROR" },
    { 30, "PR_SYS_DESC_TABLE_FULL_ERROR" },
    { 31, "Descriptor is not a socket" },
    { 32, "Descriptor is not a TCP socket" },
    { 33, "Socket address is already bound" },
    { 34, "No access rights" },
    { 35, "Operation not supported" },
    { 36, "Protocol not supported" },
    { 37, "Remote file error" },
    { 38, "Buffer overflow error" },
    { 39, "Connection reset by peer" },
    { 40, "Range error" },
    { 41, "Deadlock error" },
    { 42, "File is locked" },
    { 43, "File is too big" },
    { 44, "No space on device" },
    { 45, "Pipe error" },
    { 46, "No seek on device" },
    { 47, "File is a directory" },
    { 48, "Loop error" },
    { 49, "Name too long" },
    { 50, "File not found" },
    { 51, "File is not a directory" },
    { 52, "Read-only filesystem" },
    { 53, "Directory not empty" },
    { 54, "Filesystem mounted" },
    { 55, "Not same device" },
    { 56, "Directory corrupted" },
    { 57, "File exists" },
    { 58, "Maximum directory entries" },
    { 59, "Invalid device state" },
    { 60, "Device is locked" },
    { 61, "No more files" },
    { 62, "End of file" },
    { 63, "File seek error" },
    { 64, "File is busy" },
    { 65, "NSPR error 65" },
    { 66, "In progress error" },
    { 67, "Already initiated" },
    { 68, "Group empty" },
    { 69, "Invalid state" },
    { 70, "Network down" },
    { 71, "Socket shutdown" },
    { 72, "Connect aborted" },
    { 73, "Host unreachable" },
    { 74, "Library not loaded" },
    { 75, "The one-time function was previously called and failed. Its error code is no longer available"}
};

#if 0
#if (PR_MAX_ERROR - PR_NSPR_ERROR_BASE) > 76
#error NSPR error table is too small
#endif
#endif

nscp_error_t nscp_libsec_errors[] = {
    {  0, "SEC_ERROR_IO - I/O Error" },
    {  1, "SEC_ERROR_LIBRARY_FAILURE - Library Failure" },
    {  2, "SEC_ERROR_BAD_DATA - Bad data was received" },
    {  3, "SEC_ERROR_OUTPUT_LEN" },
    {  4, "SEC_ERROR_INPUT_LEN" },
    {  5, "SEC_ERROR_INVALID_ARGS" },
    {  6, "SEC_ERROR_INVALID_ALGORITHM - Certificate contains invalid encryption or signature algorithm" },
    {  7, "SEC_ERROR_INVALID_AVA" },
    {  8, "SEC_ERROR_INVALID_TIME - Certificate contains an invalid time value" },
    {  9, "SEC_ERROR_BAD_DER - Certificate is improperly DER encoded" },
    { 10, "SEC_ERROR_BAD_SIGNATURE - Certificate has invalid signature" },
    { 11, "SEC_ERROR_EXPIRED_CERTIFICATE - Certificate has expired" },
    { 12, "SEC_ERROR_REVOKED_CERTIFICATE - Certificate has been revoked" },
    { 13, "SEC_ERROR_UNKNOWN_ISSUER - Certificate is signed by an unknown issuer" },
    { 14, "SEC_ERROR_BAD_KEY - Invalid public key in certificate." },
    { 15, "SEC_ERROR_BAD_PASSWORD" },
    { 16, "SEC_ERROR_UNUSED" },
    { 17, "SEC_ERROR_NO_NODELOCK" },
    { 18, "SEC_ERROR_BAD_DATABASE - Problem using certificate or key database" },
    { 19, "SEC_ERROR_NO_MEMORY - Out of Memory" },
    { 20, "SEC_ERROR_UNTRUSTED_ISSUER - Certificate is signed by an untrusted issuer" },
    { 21, "SEC_ERROR_UNTRUSTED_CERT" },
    { 22, "SEC_ERROR_DUPLICATE_CERT" },
    { 23, "SEC_ERROR_DUPLICATE_CERT_TIME" },
    { 24, "SEC_ERROR_ADDING_CERT" },
    { 25, "SEC_ERROR_FILING_KEY" },
    { 26, "SEC_ERROR_NO_KEY" },
    { 27, "SEC_ERROR_CERT_VALID" },
    { 28, "SEC_ERROR_CERT_NOT_VALID" },
    { 29, "SEC_ERROR_CERT_NO_RESPONSE" },
    { 30, "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE" },
    { 31, "SEC_ERROR_CRL_EXPIRED" },
    { 32, "SEC_ERROR_CRL_BAD_SIGNATURE" },
    { 33, "SEC_ERROR_CRL_INVALID" },
    { 34, "SEC_ERROR_EXTENSION_VALUE_INVALID" },
    { 35, "SEC_ERROR_EXTENSION_NOT_FOUND" },
    { 36, "SEC_ERROR_CA_CERT_INVALID" },
    { 37, "SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID" },
    { 38, "SEC_ERROR_CERT_USAGES_INVALID" },
    { 39, "SEC_INTERNAL_ONLY" },
    { 40, "SEC_ERROR_INVALID_KEY" },
    { 41, "SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION" },
    { 42, "SEC_ERROR_OLD_CRL" },
    { 43, "SEC_ERROR_NO_EMAIL_CERT" },
    { 44, "SEC_ERROR_NO_RECIPIENT_CERTS_QUERY" },
    { 45, "SEC_ERROR_NOT_A_RECIPIENT" },
    { 46, "SEC_ERROR_PKCS7_KEYALG_MISMATCH" },
    { 47, "SEC_ERROR_PKCS7_BAD_SIGNATURE" },
    { 48, "SEC_ERROR_UNSUPPORTED_KEYALG" },
    { 49, "SEC_ERROR_DECRYPTION_DISALLOWED" },
    { 50, "XP_SEC_FORTEZZA_BAD_CARD" },
    { 51, "XP_SEC_FORTEZZA_NO_CARD" },
    { 52, "XP_SEC_FORTEZZA_NONE_SELECTED" },
    { 53, "XP_SEC_FORTEZZA_MORE_INFO" },
    { 54, "XP_SEC_FORTEZZA_PERSON_NOT_FOUND" },
    { 55, "XP_SEC_FORTEZZA_NO_MORE_INFO" },
    { 56, "XP_SEC_FORTEZZA_BAD_PIN" },
    { 57, "XP_SEC_FORTEZZA_PERSON_ERROR" },
    { 58, "SEC_ERROR_NO_KRL" },
    { 59, "SEC_ERROR_KRL_EXPIRED" },
    { 60, "SEC_ERROR_KRL_BAD_SIGNATURE" },
    { 61, "SEC_ERROR_REVOKED_KEY" },
    { 62, "SEC_ERROR_KRL_INVALID" },
    { 63, "SEC_ERROR_NEED_RANDOM" },
    { 64, "SEC_ERROR_NO_MODULE" },
    { 65, "SEC_ERROR_NO_TOKEN" },
    { 66, "SEC_ERROR_READ_ONLY" },
    { 67, "SEC_ERROR_NO_SLOT_SELECTED" },
    { 68, "SEC_ERROR_CERT_NICKNAME_COLLISION" },
    { 69, "SEC_ERROR_KEY_NICKNAME_COLLISION" },
    { 70, "SEC_ERROR_SAFE_NOT_CREATED" },
    { 71, "SEC_ERROR_BAGGAGE_NOT_CREATED" },
    { 72, "XP_JAVA_REMOVE_PRINCIPAL_ERROR" },
    { 73, "XP_JAVA_DELETE_PRIVILEGE_ERROR" },
    { 74, "XP_JAVA_CERT_NOT_EXISTS_ERROR" },
    { 75, "SEC_ERROR_BAD_EXPORT_ALGORITHM" },
    { 76, "SEC_ERROR_EXPORTING_CERTIFICATES" },
    { 77, "SEC_ERROR_IMPORTING_CERTIFICATES" },
    { 78, "SEC_ERROR_PKCS12_DECODING_PFX" },
    { 79, "SEC_ERROR_PKCS12_INVALID_MAC" },
    { 80, "SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM" },
    { 81, "SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE" },
    { 82, "SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE" },
    { 83, "SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM" },
    { 84, "SEC_ERROR_PKCS12_UNSUPPORTED_VERSION" },
    { 85, "SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT" },
    { 86, "SEC_ERROR_PKCS12_CERT_COLLISION" },
    { 87, "SEC_ERROR_USER_CANCELLED" },
    { 88, "SEC_ERROR_PKCS12_DUPLICATE_DATA" },
    { 89, "SEC_ERROR_MESSAGE_SEND_ABORTED" },
    { 90, "SEC_ERROR_INADEQUATE_KEY_USAGE" },
    { 91, "SEC_ERROR_INADEQUATE_CERT_TYPE" },
    { 92, "SEC_ERROR_CERT_ADDR_MISMATCH" },
    { 93, "SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY" },
    { 94, "SEC_ERROR_PKCS12_IMPORTING_CERT_CHAIN" },
    { 95, "SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME" },
    { 96, "SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY" },
    { 97, "SEC_ERROR_PKCS12_UNABLE_TO_WRITE" },
    { 98, "SEC_ERROR_PKCS12_UNABLE_TO_READ" },
    { 99, "SEC_ERROR_PKCS12_KEY_DATABASE_NOT_INITIALIZED" },
    { 100, "SEC_ERROR_KEYGEN_FAIL" },
    { 101, "SEC_ERROR_INVALID_PASSWORD" },
    { 102, "SEC_ERROR_RETRY_OLD_PASSWORD" },
    { 103, "SEC_ERROR_BAD_NICKNAME" },
    { 104, "SEC_ERROR_NOT_FORTEZZA_ISSUER" },
    { 105, "unused error" },
    { 106, "SEC_ERROR_JS_INVALID_MODULE_NAME" },
    { 107, "SEC_ERROR_JS_INVALID_DLL" },
    { 108, "SEC_ERROR_JS_ADD_MOD_FAILURE" },
    { 109, "SEC_ERROR_JS_DEL_MOD_FAILURE" },
    { 110, "SEC_ERROR_OLD_KRL" },
    { 111, "SEC_ERROR_CKL_CONFLICT" },
    { 112, "SEC_ERROR_CERT_NOT_IN_NAME_SPACE" },
    { 113, "SEC_ERROR_KRL_NOT_YET_VALID" },
    { 114, "SEC_ERROR_CRL_NOT_YET_VALID" },
    { 115, "SEC_ERROR_CERT_STATUS_SERVER_ERROR" },
    { 116, "SEC_ERROR_CERT_STATUS_UNKNOWN" },
    { 117, "SEC_ERROR_CERT_REVOKED_SINCE" },
    { 118, "SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE" }, 
    { 119, "SEC_ERROR_OCSP_BAD_HTTP_RESPONSE: The OCSP server returned unexpected/invalid HTTP data." },
    { 120, "SEC_ERROR_OCSP_MALFORMED_REQUEST: The OCSP server found the request to be corrupted or improperly formed." },
    { 121, "SEC_ERROR_OCSP_SERVER_ERROR: The OCSP server experienced an internal error." },
    { 122, "SEC_ERROR_OCSP_TRY_SERVER_LATER: The OCSP server suggests trying again later." },
    { 123, "SEC_ERROR_OCSP_REQUEST_NEEDS_SIG: The OCSP server requires a signature on this request." },
    { 124, "SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST: The OCSP server has refused this request as unauthorized."},
    { 125, "SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS: The OCSP server returned an unrecognizable status." },
    { 126, "SEC_ERROR_OCSP_UNKNOWN_CERT: The OCSP server has no status for the certificate." },
    { 127, "SEC_ERROR_OCSP_NOT_ENABLED: OCSP is not enabled." },
    { 128, "SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER: OCSP responder not set." },
    { 129, "SEC_ERROR_OCSP_MALFORMED_RESPONSE: Response from OCSP server was corrupted or improperly formed." },
    { 130, "SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE: The signer of the OCSP response is not authorized to give status for this certificate." },
    { 131, "SEC_ERROR_OCSP_FUTURE_RESPONSE: The OCSP response is not yet valid (contains a date in the future)." },
    { 132, "SEC_ERROR_OCSP_OLD_RESPONSE: The OCSP response contains out of date information." },
    { 133, "SEC_ERROR_DIGEST_NOT_FOUND: The CMS or PKCS#7 digest was not found in signed message." },
    { 134, "SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE: The CMS or PKCS#7 message type is unsupported." },
    { 135, "SEC_ERROR_MODULE_STUCK: PKCS#11 module could not be removed because it is still in use." },
    { 136, "SEC_ERROR_BAD_TEMPLATE: Could not decode ASN.1 data, specified template is invalid." },
    { 137, "SEC_ERROR_CRL_NOT_FOUND: No matching CRL was found." },
    { 138, "SEC_ERROR_REUSED_ISSUER_AND_SERIAL: Attempting to import a cert which conflicts with issuer/serial of existing cert." },
    { 139, "SEC_ERROR_BUSY: NSS cannot shut down, objects are still in use." },
    { 140, "SEC_ERROR_EXTRA_INPUT: DER-encoded message contains extra unused data." },
    { 141, "SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE: Unsupported elliptic curve." },
    { 142, "SEC_ERROR_UNSUPPORTED_EC_POINT_FORM: Unsupported elliptic curve point form." },
    { 143, "SEC_ERROR_UNRECOGNIZED_OID: Unrecognized Object Identifier." },
    { 144, "SEC_ERROR_OCSP_INVALID_SIGNING_CERT: Invalid OCSP signing certificate in response." },
    { 145, "SEC_ERROR_REVOKED_CERTIFICATE_CRL: Certificate is revoked in issuer's CRL." },
    { 146, "SEC_ERROR_REVOKED_CERTIFICATE_OCSP: Issuer's OCSP reports certificate is revoked." },
    { 147, "SEC_ERROR_CRL_INVALID_VERSION: Issuer's CRL has unknown version number." },
    { 148, "SEC_ERROR_CRL_V1_CRITICAL_EXTENSION: Issuer's v1 CRL has a critical extension." },
    { 149, "SEC_ERROR_CRL_UNKNOWN_CRITICAL_EXTENSION: Issuer's v2 CRL has an unknown critical extension." },
    { 150, "SEC_ERROR_UNKNOWN_OBJECT_TYPE: Unknown object type specified." },
    { 151, "SEC_ERROR_INCOMPATIBLE_PKCS11: PKCS #11 driver violates the spec in an incompatible way." },
    { 152, "SEC_ERROR_NO_EVENT: No new slot event is available at this time." },
    { 153, "SEC_ERROR_CRL_ALREADY_EXISTS: CRL already exists." },

    { 154, "SEC_ERROR_NOT_INITIALIZED: NSS is not initialized." },
    { 155, "SEC_ERROR_TOKEN_NOT_LOGGED_IN: The operation failed because the PKCS#11 token is not logged in." },
    { 156, "SEC_ERROR_OCSP_RESPONDER_CERT_INVALID: Configured OCSP responder's certificate is invalid." },
    { 157, "SEC_ERROR_OCSP_BAD_SIGNATURE: OCSP response has an invalid signature." },
    { 158, "SEC_ERROR_OUT_OF_SEARCH_LIMITS: Cert validation search is out of search limits" },
    { 159, "SEC_ERROR_INVALID_POLICY_MAPPING: Policy mapping contains anypolicy" },
    { 160, "SEC_ERROR_POLICY_VALIDATION_FAILED: Cert chain fails policy validation" },
    { 161, "SEC_ERROR_UNKNOWN_AIA_LOCATION_TYPE: Unknown location type in cert AIA extension" },
    { 162, "SEC_ERROR_BAD_HTTP_RESPONSE: Server returned bad HTTP response" },
    { 163, "SEC_ERROR_BAD_LDAP_RESPONSE: Server returned bad LDAP response" },
    { 164, "SEC_ERROR_FAILED_TO_ENCODE_DATA: Failed to encode data with ASN1 encoder" },
    { 165, "SEC_ERROR_BAD_INFO_ACCESS_LOCATION: Bad information access location in cert extension" },
    { 166, "SEC_ERROR_LIBPKIX_INTERNAL: Libpkix internal error occured during cert validation." },
    { 167, "SEC_ERROR_PKCS11_GENERAL_ERROR: PKCS11  general error occured during cert validation." },
    { 168, "SEC_ERROR_PKCS11_FUNCTION_FAILED: PKCS11 function failed." },
    { 169, "SEC_ERROR_PKCS11_DEVICE_ERROR: PKCS11 device  error." },
    { 170, "SEC_ERROR_BAD_INFO_ACCESS_METHOD: Bad access info." },
    { 171, "SEC_ERROR_CRL_IMPORT_FAILED: CRL import failed." }

};

nscp_error_t nscp_libssl_errors[] = {
    {  0, "SSL_ERROR_EXPORT_ONLY_SERVER - client does not support high-grade encryption." },
    {  1, "SSL_ERROR_US_ONLY_SERVER - client requires high-grade encryption which is not supported." },
    {  2, "SSL_ERROR_NO_CYPHER_OVERLAP - no common encryption algorithm(s) with client." },
    {  3, "SSL_ERROR_NO_CERTIFICATE - unable to find the certificate or key necessary for authentication." },
    {  4, "SSL_ERROR_BAD_CERTIFICATE - unable to communicate securely wih peer: peer's certificate was rejected." },
    {  5, "unused SSL error #5" },
    {  6, "SSL_ERROR_BAD_CLIENT - protocol error." },
    {  7, "SSL_ERROR_BAD_SERVER - protocol error." },
    {  8, "SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE - unsupported certificate type." },
    {  9, "SSL_ERROR_UNSUPPORTED_VERSION - client is using unsupported SSL version." },
    { 10, "unused SSL error #10" },
    { 11, "SSL_ERROR_WRONG_CERTIFICATE - the public key in the server's own certificate does not match its private key" },
    { 12, "SSL_ERROR_BAD_CERT_DOMAIN - requested domain name does not match the server's certificate." },
    { 13, "SSL_ERROR_POST_WARNING" },
    { 14, "SSL_ERROR_SSL2_DISABLED - peer only supports SSL version 2, which is locally disabled" },
    { 15, "SSL_ERROR_BAD_MAC_READ - SSL has received a record with an incorrect Message Authentication Code." },
    { 16, "SSL_ERROR_BAD_MAC_ALERT - SSL has received an error indicating an incorrect Message Authentication Code." },
    { 17, "SSL_ERROR_BAD_CERT_ALERT - SSL client cannot verify your certificate." },
    { 18, "SSL_ERROR_REVOKED_CERT_ALERT - the server has rejected your certificate as revoked." },
    { 19, "SSL_ERROR_EXPIRED_CERT_ALERT - the server has rejected your certificate as expired." },
    { 20, "SSL_ERROR_SSL_DISABLED - cannot connect: SSL is disabled." },
    { 21, "SSL_ERROR_FORTEZZA_PQG - cannot connect: SSL peer is in another Fortezza domain" },
    { 22, "SSL_ERROR_UNKNOWN_CIPHER_SUITE - an unknown SSL cipher suite has been requested" },
    { 23, "SSL_ERROR_NO_CIPHERS_SUPPORTED - no cipher suites are present and enabled in this program" },
    { 24, "SSL_ERROR_BAD_BLOCK_PADDING" },
    { 25, "SSL_ERROR_RX_RECORD_TOO_LONG" },
    { 26, "SSL_ERROR_TX_RECORD_TOO_LONG" },
    { 27, "SSL_ERROR_RX_MALFORMED_HELLO_REQUEST" },
    { 28, "SSL_ERROR_RX_MALFORMED_CLIENT_HELLO" },
    { 29, "SSL_ERROR_RX_MALFORMED_SERVER_HELLO" },
    { 30, "SSL_ERROR_RX_MALFORMED_CERTIFICATE" },
    { 31, "SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH" },
    { 32, "SSL_ERROR_RX_MALFORMED_CERT_REQUEST" },
    { 33, "SSL_ERROR_RX_MALFORMED_HELLO_DONE" },
    { 34, "SSL_ERROR_RX_MALFORMED_CERT_VERIFY" },
    { 35, "SSL_ERROR_RX_MALFORMED_CLIENT_KEY_EXCH" },
    { 36, "SSL_ERROR_RX_MALFORMED_FINISHED" },
    { 37, "SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER" },
    { 38, "SSL_ERROR_RX_MALFORMED_ALERT" },
    { 39, "SSL_ERROR_RX_MALFORMED_HANDSHAKE" },
    { 40, "SSL_ERROR_RX_MALFORMED_APPLICATION_DATA" },
    { 41, "SSL_ERROR_RX_UNEXPECTED_HELLO_REQUEST" },
    { 42, "SSL_ERROR_RX_UNEXPECTED_CLIENT_HELLO" },
    { 43, "SSL_ERROR_RX_UNEXPECTED_SERVER_HELLO" },
    { 44, "SSL_ERROR_RX_UNEXPECTED_CERTIFICATE" },
    { 45, "SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH" },
    { 46, "SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST" },
    { 47, "SSL_ERROR_RX_UNEXPECTED_HELLO_DONE" },
    { 48, "SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY" },
    { 49, "SSL_ERROR_RX_UNEXPECTED_CLIENT_KEY_EXCH" },
    { 50, "SSL_ERROR_RX_UNEXPECTED_FINISHED" },
    { 51, "SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER" },
    { 52, "SSL_ERROR_RX_UNEXPECTED_ALERT" },
    { 53, "SSL_ERROR_RX_UNEXPECTED_HANDSHAKE" },
    { 54, "SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA" },
    { 55, "SSL_ERROR_RX_UNKNOWN_RECORD_TYPE" },
    { 56, "SSL_ERROR_RX_UNKNOWN_HANDSHAKE" },
    { 57, "SSL_ERROR_RX_UNKNOWN_ALERT" },
    { 58, "SSL_ERROR_CLOSE_NOTIFY_ALERT - SSL peer has closed the connection" },
    { 59, "SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT" },
    { 60, "SSL_ERROR_DECOMPRESSION_FAILURE_ALERT" },
    { 61, "SSL_ERROR_HANDSHAKE_FAILURE_ALERT" },
    { 62, "SSL_ERROR_ILLEGAL_PARAMETER_ALERT" },
    { 63, "SSL_ERROR_UNSUPPORTED_CERT_ALERT" },
    { 64, "SSL_ERROR_CERTIFICATE_UNKNOWN_ALERT" },
    { 65, "SSL_ERROR_GENERATE_RANDOM_FAILURE" },
    { 66, "SSL_ERROR_SIGN_HASHES_FAILURE" },
    { 67, "SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE" },
    { 68, "SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE" },
    { 69, "SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE" },
    { 70, "SSL_ERROR_ENCRYPTION_FAILURE" },
    { 71, "SSL_ERROR_DECRYPTION_FAILURE" },
    { 72, "SSL_ERROR_SOCKET_WRITE_FAILURE" },
    { 73, "SSL_ERROR_MD5_DIGEST_FAILURE" },
    { 74, "SSL_ERROR_SHA_DIGEST_FAILURE" },
    { 75, "SSL_ERROR_MAC_COMPUTATION_FAILURE" },
    { 76, "SSL_ERROR_SYM_KEY_CONTEXT_FAILURE" },
    { 77, "SSL_ERROR_SYM_KEY_UNWRAP_FAILURE" },
    { 78, "SSL_ERROR_PUB_KEY_SIZE_LIMIT_EXCEEDED" },
    { 79, "SSL_ERROR_IV_PARAM_FAILURE" },
    { 80, "SSL_ERROR_INIT_CIPHER_SUITE_FAILURE" },
    { 81, "SSL_ERROR_SESSION_KEY_GEN_FAILURE" },
    { 82, "SSL_ERROR_NO_SERVER_KEY_FOR_ALG" },
    { 83, "SSL_ERROR_TOKEN_INSERTION_REMOVAL" },
    { 84, "SSL_ERROR_TOKEN_SLOT_NOT_FOUND" },
    { 85, "SSL_ERROR_NO_COMPRESSION_OVERLAP" },
    { 86, "SSL_ERROR_HANDSHAKE_NOT_COMPLETED" },
    { 87, "SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE" },
    { 88, "SSL_ERROR_CERT_KEA_MISMATCH" },
    { 89, "SSL_ERROR_NO_TRUSTED_SSL_CLIENT_CA - the CA that signed the client certificate is not trusted locally" },
    { 90, "SSL_ERROR_SESSION_NOT_FOUND: Client SSL session ID not found in server session cache." },
    { 91, "SSL_ERROR_DECRYPTION_FAILED_ALERT: Client was unable to decrypt an SSL record it received." },
    { 92, "SSL_ERROR_RECORD_OVERFLOW_ALERT: Client received an SSL record that was longer than permitted." },
    { 93, "SSL_ERROR_UNKNOWN_CA_ALERT: Client does not recognize and trust the CA that issues server certificate." },
    { 94, "SSL_ERROR_ACCESS_DENIED_ALERT: Client received a valid certificate but denied access." },
    { 95, "SSL_ERROR_DECODE_ERROR_ALERT: Client could not decode an SSL handshake message." },
    { 96, "SSL_ERROR_DECRYPT_ERROR_ALERT: Client reports signature verification or key exchange failure." },
    { 97, "SSL_ERROR_EXPORT_RESTRICTION_ALERT: Client reports negotiation not in compliance with export regulations." },
    { 98, "SSL_ERROR_PROTOCOL_VERSION_ALERT: Client reports incompatible or unsupported protocol version." },
    { 99, "SSL_ERROR_INSUFFICIENT_SECURITY_ALERT: Server configuration requires ciphers more secure than those supported by client." },
    { 100, "SSL_ERROR_INTERNAL_ERROR_ALERT: Client reports it experienced an internal error." },
    { 101, "SSL_ERROR_USER_CANCELED_ALERT: Client canceled handshake." },
    { 102, "SSL_ERROR_NO_RENEGOTIATION_ALERT: Client does not permit renegotiation of SSL security parameters." },
    { 103, "SSL_ERROR_SERVER_CACHE_NOT_CONFIGURED: SSL server cache not configured and not disabled for this socket." },
    { 104, "SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT: SSL peer does not support requested TLS hello extension." },
    { 105, "SSL_ERROR_CERTIFICATE_UNOBTAINABLE_ALERT: SSL peer could not obtain your certificate from the supplied URL." },
    { 106, "SSL_ERROR_UNRECOGNIZED_NAME_ALERT: SSL peer has no certificate for the requested DNS name." },
    { 107, "SSL_ERROR_BAD_CERT_STATUS_RESPONSE_ALERT: SSL peer was unable to get an OCSP response for its certificate." },
    { 108, "SSL_ERROR_BAD_CERT_HASH_VALUE_ALERT: SSL peer reported bad certificate hash value." },
    { 109, "SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET: SSL received an unexpected New Session Ticket handshake message." },
    { 110, "SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET: SSL received a malformed New Session Ticket handshake message." },
    { 111, "SSL_ERROR_DECOMPRESSION_FAILURE: SSL decompression failed." },
    { 112, "SSL_ERROR_RENEGOTIATION_NOT_ALLOWED: SSL renegotiation not allowed." }

};

#ifdef WIN32
#define __EXPORT __declspec(dllexport)
#else
#define __EXPORT
#endif

__EXPORT const char* nscperror_lookup(int error)
{
    const char *errmsg;

    if ((error >= NSCP_NSPR_ERROR_BASE) && (error <= NSCP_NSPR_MAX_ERROR)) {
        errmsg = nscp_nspr_errors[error-NSCP_NSPR_ERROR_BASE].errorString;
        return errmsg;
    } else if ((error >= NSCP_LIBSEC_ERROR_BASE) &&
        (error <= NSCP_LIBSEC_MAX_ERROR)) {
        return nscp_libsec_errors[error-NSCP_LIBSEC_ERROR_BASE].errorString;
    } else if ((error >= NSCP_LIBSSL_ERROR_BASE) &&
        (error <= NSCP_LIBSSL_MAX_ERROR)) {
        return nscp_libssl_errors[error-NSCP_LIBSSL_ERROR_BASE].errorString;
    } else {
        PRInt32 msglen = PR_GetErrorTextLength();
        if (msglen > 0) {
             char *msg = (char *)malloc(msglen); 
             PR_GetErrorText(msg);
             return (const char *)msg;
        }
        return (const char *)NULL;
    }
}
