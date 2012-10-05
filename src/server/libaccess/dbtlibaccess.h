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

#define LIBRARY_NAME "libaccess"

static char dbtlibaccessid[] = "$DBT: libaccess referenced v1 $";

#include "i18n.h"

/* Message IDs reserved for this file: HTTP5000-HTTP5999 */
BEGIN_STR(libaccess)
	ResDef( DBT_LibraryID_, -1, dbtlibaccessid )/* extracted from dbtlibaccess.h*/
	ResDef( DBT_lasdnsbuildUnableToAllocateHashT_, 19, "HTTP5019: LASDnsBuild unable to allocate hash table header\n" )/*extracted from lasdns.cpp*/
	ResDef( DBT_lasdnsbuildUnableToAddKeySN_, 20, "HTTP5020: LASDnsBuild unable to add key %s\n" )/*extracted from lasdns.cpp*/
	ResDef( DBT_lasDnsBuildReceivedRequestForAtt_, 25, "HTTP5025: LAS DNS build received request for attribute %s\n" )/*extracted from lasdns.cpp*/
	ResDef( DBT_lasdnsevalIllegalComparatorDN_, 26, "HTTP5026: LASDnsEval - illegal comparator %s\n" )/*extracted from lasdns.cpp*/
	ResDef( DBT_lasdnsevalUnableToAllocateContex_, 27, "HTTP5027: LASDnsEval unable to allocate Context struct\n\n" )/*extracted from lasdns.cpp*/
	ResDef( DBT_lasdnsevalUnableToGetDnsErrorDN_, 29, "HTTP5029: LASDnsEval unable to get DNS (%s)\n" )/*extracted from lasdns.cpp*/
	ResDef( DBT_lasGroupEvalReceivedRequestForAt_, 30, "HTTP5030: LASGroupEval: received request for attribute %s\n" )/*extracted from lasgroup.cpp*/
	ResDef( DBT_lasgroupevalIllegalComparatorDN_, 31, "HTTP5031: LASGroupEval: illegal comparator %s\n" )/*extracted from lasgroup.cpp*/
	ResDef( DBT_lasiptreeallocNoMemoryN_, 43, "HTTP5043: LASIpTreeAlloc - no memory\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_ipLasUnableToAllocateTreeNodeN_, 44, "HTTP5044: IP LAS unable to allocate tree node\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_ipLasUnableToAllocateTreeNodeN_1, 45, "HTTP5045: IP LAS unable to allocate tree node\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_lasIpBuildReceivedRequestForAttr_, 46, "HTTP5046: LAS IP build received request for attribute %s\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_lasipevalIllegalComparatorDN_, 47, "HTTP5047: LASIpEval - illegal comparator %s\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_lasipevalUnableToGetSessionAddre_, 48, "HTTP5048: LASIpEval unable to get session address (%s)\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_lasipevalUnableToAllocateContext_, 49, "HTTP5049: LASIpEval unable to allocate Context struct\n\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_lasipevalReach32BitsWithoutConcl_, 50, "HTTP5050: LASIpEval - reach %d bits without conclusion value=%s" )/*extracted from lasip.cpp*/
	ResDef( DBT_lasProgramEvalReceivedRequestFor_, 51, "HTTP5051: LAS Program Eval received request for attribute %s\n" )/*extracted from lasprogram.cpp*/
	ResDef( DBT_lasprogramevalIllegalComparatorD_, 52, "HTTP5052: LASProgramEval - illegal comparator %s\n" )/*extracted from lasprogram.cpp*/
	ResDef( DBT_lasIpIncorrentIPPattern,           53, "HTTP5053: IP LAS found incorrect ip pattern %s\n" )/*extracted from lasip.cpp*/
	ResDef( DBT_unexpectedAttributeInDayofweekSN_, 60, "HTTP5060: Unexpected attribute in dayOfWeek - %s\n" )/*extracted from lastod.cpp*/
	ResDef( DBT_illegalComparatorForDayofweekDN_, 61, "HTTP5061: Illegal comparator for dayOfWeek - %s\n" )/*extracted from lastod.cpp*/
	ResDef( DBT_unexpectedAttributeInTimeofdaySN_, 62, "HTTP5062: Unexpected attribute in timeOfDay - %s\n" )/*extracted from lastod.cpp*/
	ResDef( DBT_lasUserEvalReceivedRequestForAtt_, 63, "HTTP5063: LAS User Eval received request for attribute %s\n" )/*extracted from lasuser.cpp*/
	ResDef( DBT_lasuserevalIllegalComparatorDN_, 64, "HTTP5064: LASUserEval - illegal comparator %s\n" )/*extracted from lasuser.cpp*/
	ResDef( DBT_lasuserevalRanOutOfMemoryN_, 65, "HTTP5065: LASUserEval - ran out of memory\n" )/*extracted from lasuser.cpp*/
	ResDef( DBT_lasProgramUnableToGetRequest_, 77, "HTTP5077: LASProgram unable to get request address (%s)" ) /*extracted from lasprogram.cpp*/
	ResDef( DBT_lasProgramRejectingRequestForProgram_, 78, "HTTP5078: LASProgram rejecting request for program %s from pattern %s" ) /*extracted from lasprogram.cpp*/
	ResDef( DBT_illegalComparatorForTimeOfDayDN_, 82, "HTTP5082: Illegal comparator for timeOfDay - %s\n" )/*extracted from lastod.cpp*/
	ResDef( DBT_EvalBuildContextUnableToCreateHash, 83, "HTTP5083: ACL_EvalBuildContext unable to create hash table\n")
	ResDef( DBT_EvalBuildContextUnableToAllocAceEntry, 85, "HTTP5085: ACL_EvalBuildContext unable to allocate ACE entry\n")
	ResDef( DBT_EvalBuildContextUnableToAllocAuthPointerArray, 86, "HTTP5086: ACL_EvalBuildContext unable to allocate auth pointer array\n")
	ResDef( DBT_EvalBuildContextUnableToAllocAuthPlist, 87, "HTTP5087: ACL_EvalBuildContext unable to allocate auth plist\n")
	ResDef( DBT_EvalTestRightsInterimAbsoluteNonAllowValue, 88, "HTTP5088: ACL_EvalTestRights: an interim, absolute non-allow value was encountered. right=%s, value=%s\n")
	ResDef( DBT_EvalTestRightsEvalBuildContextFailed, 89, "HTTP5089: ACL_INTEvalTestRights: call to ACL_EvalBuildContext returned failure status\n")
	ResDef( DBT_ModuleRegisterModuleNameMissing, 90, "HTTP5090: ACL_ModuleRegister: module name is missing\n")
	ResDef( DBT_ModuleRegisterFailed, 91, "HTTP5091: ACL_ModuleRegister: call to module init function returned a failed status\n")
	ResDef( DBT_GetAttributeCouldntDetermineMethod, 92, "HTTP5092: ACL_GetAttribute: couldn't determine method for %s\n")
	ResDef( DBT_GetAttributeCouldntLocateGetter, 93,  "HTTP5093: ACL_GetAttribute: couldn't locate getter for %s")
	ResDef( DBT_GetAttributeDidntGetAttr, 94, "HTTP5094: while trying to get attribute \"%s\"")
	ResDef( DBT_GetAttributeDidntSetAttr, 95, "HTTP5095: ACL_GetAttribute: failed to set attribute \"%s\"")
	ResDef( DBT_GetAttributeAllGettersDeclined, 96, "HTTP5096: ACL_GetAttribute: All attribute getters declined for attr \"%s\"")
	ResDef( DBT_DatabaseRegisterDatabaseNameMissing, 98, "HTTP5098: ACL_DatabaseRegister: database name is missing")
	ResDef( DBT_lasGroupEvalUnableToGetDatabaseName, 105, "HTTP5105: LASGroupEval unable to get database name (%s)")
	ResDef( DBT_lasProgramReceivedInvalidProgramExpression, 106, "HTTP5106: received invalid program expression %s")
	ResDef( DBT_ldapaclDatabaseUrlIsMissing, 107, "HTTP5107: parse_ldap_url: database URL is missing")
	ResDef( DBT_ldapaclDatabaseNameIsMissing, 108, "HTTP5108: parse_ldap_url: database name is missing")
	ResDef( DBT_ldapaclErrorParsingLdapUrl, 109, "HTTP5109: parse_ldap_url: error parsing ldap URL")
	ResDef( DBT_ldapaclUnableToGetDatabaseName, 110,  "HTTP5110: ldap password check: unable to get database name (%s)")
	ResDef( DBT_ldapaclUnableToGetParsedDatabaseName, 111, "HTTP5111: ldap password check: unable to get parsed database %s")
	ResDef( DBT_ldapaclPassworkCheckLdapError, 113, "HTTP5113: ldap password check: LDAP error: \"%s\"")
	ResDef( DBT_GetUserIsMemberLdapUnabelToGetDatabaseName, 114, "HTTP5114: get_user_ismember_ldap unable to get database name (%s)")
	ResDef( DBT_GetUserIsMemberLdapUnableToGetParsedDatabaseName, 115, "HTTP5115: get_user_ismember_ldap unable to get parsed database %s")
	ResDef( DBT_GetUserIsMemberLdapError, 118, "HTTP5118: get_user_ismember_ldap: LDAP error: \"%s\"")
	ResDef( DBT_LdapDatabaseHandleNotARegisteredDatabase, 119, "HTTP5119: ACL_LDAPDatabaseHandle: %s is not a registered database")
	ResDef( DBT_LdapDatabaseHandleNotAnLdapDatabase, 120, "HTTP5120: ACL_LDAPDatabaseHandle: %s is not an LDAP database")
	ResDef( DBT_AclErrorCheckingGroupMembership, 121, "HTTP5121: Error while checking group membership of %s in %s: %s")
	ResDef( DBT_AclErrorCheckingAuthentication, 122, "HTTP5122: Error while checking authentication of %s in %s: %s")
	ResDef( DBT_AclErrorCheckingRoleMembership, 123, "HTTP5123: Error while checking role membership of %s in %s: %s")
	ResDef( DBT_AclerrfmtAclerrnomem, 124, "insufficient dynamic memory")
	ResDef( DBT_AclerrfmtAclerropen, 125, "error opening file %s (%s)")
	ResDef( DBT_AclerrfmtAclerrdupsym1, 126, "duplicate definition of %s")
	ResDef( DBT_AclerrfmtAclerrdupsym3, 127,  "file %s, line %s: duplicate definition of %s")
	ResDef( DBT_AclerrfmtAclerrsyntax, 128, "file %s, line %s: syntax error")
	ResDef( DBT_AclerrfmtAclerrundef, 129, "file %s, line %s: %s is undefined")
	ResDef( DBT_AclerrfmtAclaclundef, 130, "in acl %s, %s %s is undefined")
	ResDef( DBT_AclerrfmtAclerradb, 131, "database %s: error accessing %s")
	ResDef( DBT_AclerrfmtAclerrparse1, 132, "%s")
	ResDef( DBT_AclerrfmtAclerrparse2, 133, "file %s, line %s: invalid syntax")
	ResDef( DBT_AclerrfmtAclerrparse3, 134, "file %s, line %s: syntax error at \"%s\"")
	ResDef( DBT_AclerrfmtAclerrnorlm, 135, "realm %s is not defined")
	ResDef( DBT_AclerrfmtUnknownerr, 136, "error code = %d")
	ResDef( DBT_AclerrfmtAclerrinternal, 137, "internal ACL error")
	ResDef( DBT_AclerrfmtAclerrinval, 138, "invalid argument")
	ResDef( DBT_DbtypeNotDefinedYet, 139, "HTTP5139: ACL_DatabaseRegister: dbtype for database \"%s\" is not defined yet!")
	ResDef( DBT_CouldntDetermineDbtype, 140, "HTTP5140: Couldn't determine dbtype from URL %s")
	ResDef( DBT_RegisterDatabaseFailed, 141,  "HTTP5141: Failed to register database %s")
	ResDef( DBT_AclerrfmtAclerrfail, 142, "ACL call returned failed status")
	ResDef( DBT_AclerrfmtAclerrio, 143, "file %s: ACL IO error - %s")
	ResDef( DBT_AclUserExistsOutOfMemory, 144, "HTTP5144: acl_user_exists: out of memory")
	ResDef( DBT_AclUserExistsNot, 145, "HTTP5145: acl_user_exists: user doesn't exist anymore")
	ResDef( DBT_AclUserPlistError, 146, "HTTP5146: acl_user_exists: plist error")
	ResDef( DBT_nullaclDatabaseUrlIsMissing, 150, "HTTP5150: parse_null_url: database URL is missing")
	ResDef( DBT_nullaclDatabaseNameIsMissing, 151, "HTTP5151: parse_null_url: database name is missing")
 	ResDef( DBT_nullaclMustHaveNullPrefix, 152, "HTTP5152: parse_null_url: must have \"null:\" prefix")
 	ResDef( DBT_nullaclMustHaveSlashes, 153, "HTTP5153: parse_null_url: must have \"null:///\" prefix")
 	ResDef( DBT_nullaclOnlyAllOrNone, 154, "HTTP5154: parse_null_url:  only \"null:///all\" and \"null:///none\" are valid")
 	ResDef( DBT_ldapaclInvalidNumberOfSessions, 155, "HTTP5155: parse_ldap_url: invalid \"sessions\" property: must be >=1 and <= 512")
 	ResDef( DBT_ldapaclInvalidDynGroupsMode, 156, "HTTP5156: parse_ldap_url: invalid \"dyngroups\" property: must be either \"on\", \"off\" or \"recursive\"")
 	ResDef( DBT_aclcacheNullPath, 158, "HTTP5158: ACL_GetPathAcls - Null Path failure")
 	ResDef( DBT_aclcachePath2Long, 159, "HTTP5159: ACL_GetPathAcls - the path is too long for ACL_GetPathAcls to handle")
 	ResDef( DBT_ldapACLDBHandleNotSupported, 160, "HTTP5160: ACL_LDAPDatabaseHandle() called for %s, but it is no longer supported")
 	ResDef( DBT_ldapACLUserExistErr, 161, "HTTP5161: Error while checking the existence of user: %s")
 	ResDef( DBT_nullACLDBNameErr, 162, "HTTP5162: get_is_valid_password_basic_null: unable to get database name (%d)")
 	ResDef( DBT_nullUserInfoErr, 163, "HTTP5163: get_is_valid_password_basic_null: unable to get info for %s (%d)")
 	ResDef( DBT_nullDBNameErr2, 164, "HTTP5164: get_user_exists_null: unable to get database name (%d)")
 	ResDef( DBT_nullUserInfoErr2, 165, "HTTP5165: get_user_exists_null: unable to get info for %s (%d)")
 	ResDef( DBT_nullUserInfoErr3, 167, "HTTP5167: get_user_ismember_null: unable to get info for %s (%d)")
 	ResDef( DBT_usrcacheDisabled, 168, "HTTP5168: User authentication cache is disabled")
 	ResDef( DBT_usrcacheExpiryTimeout, 169, "HTTP5169: User authentication cache entries expire in %d seconds")
 	ResDef( DBT_usrcacheSize, 170, "HTTP5170: User authentication cache holds %d users")
 	ResDef( DBT_usrcacheGroupPerUsrSize, 171, "HTTP5171: Up to %d groups are cached for each cached user.")
        ResDef( DBT_usrcacheAuthUsrViaCache, 172, "HTTP5172: Authenticated user %s via ACL user cache")
        ResDef( DBT_usrcacheGrpMembershipViaCache, 173, "HTTP5173: User %s membership in group %s found in ACL user cache")
        ResDef( DBT_aclFrameSubjectPropNotFound,174, "HTTP5174: Unable to allocate Subject property list.\n")
        ResDef( DBT_aclFrameSubjectPropCannotSet,175, "HTTP5175: Unable to set session ptr in Subject property list (Error %d)\n")
	ResDef(DBT_aclFrameResourcePropListCannotAlloc,176, "HTTP5176: Unable to set request ptr in Resource property list (Error %d)\n")
        ResDef( DBT_aclFrameProgramGroupNotFound,177, "HTTP5177: Unable to get program group in R esource property list (Error %d)\n")
        ResDef( DBT_aclFrameResourcePropListAllocFailure,180, "HTTP5180: Unable to allocate Resource property list.\n")
        ResDef( DBT_aclFrameLASIpGetter1,181, "HTTP5181: LASIpGetter unable to get session address %d\n")
        ResDef( DBT_aclFrameLASIpGetter2,182, "HTTP5182: LASIpGetter unable to save ip address %d\n")
        ResDef( DBT_aclFrameLASDnsGetter1,183, "HTTP5183: ACL evaluation requires dns to be enabled. Failing ACL evaluation")
        ResDef( DBT_aclFrameLASDnsGetter2,184, "HTTP5184: LASDnsGetter unable to save DNS address %d\n")
        ResDef( DBT_aclFrameAccessDenied1,187, "HTTP5187: access of %s denied because evaluation of ACL %s directive %d failed")
        ResDef( DBT_aclFrameAccessDeniedRedirectURL,188, "HTTP5188: access to the DENY REDIRECT URL %s also denied by ACL %s directive %d")
        ResDef( DBT_aclFrameAccessDeniedRespFileUnreadable,189, "HTTP5189: access denied response file %s is unreadable")
        ResDef( DBT_aclFrameAccessDeniedRespTypeUndefined,190, "HTTP5190: access denied response type %s is undefined")
        ResDef( DBT_aclFrameAccessDenied,191, "HTTP5191: access of %s denied by ACL %s directive %d")
	ResDef( DBT_sslLasUnexpectedAttribute, 192, "HTTP5192: Unexpected attribute in ssl LAS - %s\n" )
	ResDef( DBT_sslLasIllegalComparator, 193, "HTTP5193: Illegal comparator for ssl LAS - %s\n" )
	ResDef( DBT_sslLasIllegalValue, 194, "HTTP5194: Illegal value for ssl LAS  - %s\n" )
	ResDef( DBT_sslLasUnableToGetSessionAddr, 195, "HTTP5195: Unable to get session address in ssl LAS\n" )
	ResDef( DBT_dnsLasUnableToGetSessionAddr, 196, "HTTP5196: Unable to get session address in dns LAS\n" )
	ResDef( DBT_GetUserIsInRoleLdapUnableToGetDatabaseName, 197, "HTTP5197 get_user_isinrole_ldap: unable to get database name (%s)")
	ResDef( DBT_GetUserIsInRoleLdapUnableToGetParsedDatabaseName, 198, "HTTP5198: get_user_isinrole_ldap: unable to get parsed database: %s")
	ResDef( DBT_GetUserIsInRoleLdapError, 199, "HTTP5199: get_user_isinrole_ldap: LDAP error: \"%s\"")
 	ResDef( DBT_nullIsInRoleUnableToGetDbhandle, 201, "HTTP5201: get_user_isinrole_null: unable to get dbhandle (%d)")
	ResDef( DBT_lasRoleEvalReceivedRequestForAt_, 202, "HTTP5202: LASRoleEval: received request for attribute %s\n" )
	ResDef( DBT_lasRoleEvalIllegalComparatorDN_, 203, "HTTP5203: LASRoleEval: illegal comparator %s\n" )
	ResDef( DBT_lasRoleEvalUnableToGetDatabaseName, 204, "HTTP5204: LASRoleEval: unable to get database name (%s)")
	ResDef( DBT_getLdapBasednMustHaveName, 205, "HTTP5205: get_ldap_basedn: for dc tree lookup, virtual server %s must have a servername")
	ResDef( DBT_getLdapBasednInternalError, 206, "HTTP5206: get_ldap_basedn: internal error")
	ResDef( DBT_getLdapBasednOutOfMemory, 207, "HTTP5207: get_ldap_basedn: out of memory")
	ResDef( DBT_getLdapBasednInvalidServername, 208, "HTTP5208: get_ldap_basedn: vs %s: invalid servername \"%s\": must have at least two domain components")
	ResDef( DBT_getLdapBasednDomainNotFound, 209, "HTTP5209: get_ldap_basedn: (%s, %s): domain entry not found in DC tree")
	ResDef( DBT_getLdapBasednDomainNotActive, 210, "HTTP5210: get_ldap_basedn: (%s, %s): domain is not active")
	ResDef( DBT_getLdapBasednNoInetbasedomainAttr, 211, "HTTP5211: get_ldap_basedn: (%s, %s): no inetBaseDomain attribute in DC tree entry")
	ResDef( DBT_getIsValidPasswordLdapStepDown, 212, "HTTP5212: Client tried to step down authentication method.")
	ResDef( DBT_getIsValidPasswordLdapDigestAuthNotSupported, 213, "HTTP5213: Database \"%s\" (which maps to \"%s\" on this VS) does not support digest authentication")
	ResDef( DBT_ldapaclErrorIncompatibleDatabases, 214, "HTTP5214: parse_ldap_url: the specified LDAP databases are not compatible")

	ResDef( DBT_LDAPSessionAllocateNoAssociatedVS, 215, "HTTP5215: ACL_LDAPSessionAllocate: No Associated VS")
	ResDef( DBT_LDAPSessionAllocateNotARegisteredVirtDb, 216, "HTTP5216: ACL_LDAPSessionAllocate: database %s in virtual server %s does not exist")
	ResDef( DBT_LDAPSessionAllocateNotARegisteredDatabase, 217, "HTTP5217: ACL_LDAPSessionAllocate: %s is not a registered database")
	ResDef( DBT_LDAPSessionAllocateNotAnLdapDatabase, 218, "HTTP5218: ACL_LDAPSessionAllocate: %s is not an LDAP database")
	ResDef( DBT_invalidDigestRequest, 219, "Digest request header has insufficient data")

	ResDef( DBT_fileaclDatabaseUrlIsMissing, 230, "HTTP5230: fileacl_parse_url: database URL is missing")
	ResDef( DBT_fileaclDatabaseNameIsMissing,231, "HTTP5231: fileacl_parse_url: database name is missing")
	ResDef( DBT_fileaclErrorParsingFileUrl,  232, "HTTP5232: fileacl_parse_url: error parsing file URL")
	ResDef( DBT_fileaclDBHandleNotFound,    233, "HTTP5233: fileacl: unable to get parsed database %s")
	ResDef( DBT_fileaclDigestAuthNotSupport,236, "HTTP5236: fileacl: digest auth is not supported for this file")
	ResDef( DBT_fileaclUserRealmNotFound,   237, "HTTP5237: fileacl: user %s in realm %s not found")
	ResDef( DBT_notARegisteredDatabase,     239, "HTTP5239: %s is not a registered database")
	ResDef( DBT_fileaclErrorOpenFile,       240, "HTTP5240: fileacl: error opening file %s (%s)")
	ResDef( DBT_fileaclErrorReadingFile,    241, "HTTP5241: fileacl: error reading file %s")
        ResDef( DBT_pamDbUrlIsWrong,            242, "HTTP5242: PAM authdb URL must be 'pam'")
        ResDef( DBT_pamNotRoot,                 243, "HTTP5243: PAM authdb not supported unless server runs as root")
	ResDef( DBT_lasIsLockOwnerEvalReceivedRequestForAtt_, 244, "HTTP5244: LASIsLockOwnerEval received request for attribute %s\n" )
	ResDef( DBT_lasIsLockOwnerEvalIllegalComparatorDN_, 245, "HTTP5245: LASIsLockOwnerEval - illegal comparator %s\n" )
	ResDef( DBT_lasIsLockOwnerEvalIllegalAttrPattern_, 246, "HTTP5246: LASIsLockOwnerEval - illegal attribute pattern %s\n" )
	ResDef( DBT_lasOwnerEvalReceivedRequestForAtt_, 247, "HTTP5247: LASOwnerEval received request for attribute %s\n" )
	ResDef( DBT_lasOwnerEvalIllegalComparatorDN_, 248, "HTTP5248: LASOwnerEval - illegal comparator %s\n" )
	ResDef( DBT_lasOwnerEvalIllegalAttrPattern_, 249, "HTTP5249: LASOwnerEval - illegal attribute pattern %s\n" )
        ResDef( DBT_gssNoKeytab, 250, "HTTP5250: Internal error: No value for krb5-keytab-file element" )
        ResDef( DBT_kerberosDbUrlIsWrong, 251, "HTTP5251: kerberos authdb: URL must be 'kerberos'")
        ResDef( DBT_kerberosOM, 252, "HTTP5252: kerberos authdb: out of memory")
        ResDef( DBT_gssapiNoRq, 253, "HTTP5253: gssapi: internal error: no request object" )
        ResDef( DBT_gssapiNoDb, 254, "HTTP5254: gssapi: internal error: no auth-db name" )
        ResDef( DBT_gssapiNoDbInfo, 255, "HTTP5255: gssapi: internal error: no auth-db properties available" )
        ResDef( DBT_gssapiNotKerberos, 256, "HTTP5256: gssapi: database %s is not a kerberos auth-db" )
        ResDef( DBT_gssapiNoAuthz, 257, "HTTP5257: gssapi: internal error: authorization header not present" )
        ResDef( DBT_gssapiNoSn, 258, "HTTP5258: gssapi: internal error: no session object" )
        ResDef( DBT_gssapiNoNego, 259, "HTTP5259: gssapi: invalid authorization header type: [%s]" )
        ResDef( DBT_gssapiCantDecode, 260, "HTTP5260: gssapi: unable to decode authorization header: [%s]" )
        ResDef( DBT_gssapiNoServerCreds, 261, "HTTP5261: gssapi: unable to obtain server credentials" )
        ResDef( DBT_gssapiNoUserName, 262, "HTTP5262: gssapi: unable to obtain client name" )
END_STR(libaccess)

