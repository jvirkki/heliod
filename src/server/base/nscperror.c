/* nscperrors.c
 * Very crude error handling for nspr and libsec.
 */

#include "netsite.h"
#include "prerror.h"
#include "base/dbtbase.h"
#include "nscperror.h"
#include "sslerr.h"
#include "secerr.h"

#define NSCP_NSPR_ERROR_BASE            (PR_NSPR_ERROR_BASE)
#if 0
#define NSCP_NSPR_MAX_ERROR             ((PR_MAX_ERROR) - 1)
#endif
#define NSCP_NSPR_MAX_ERROR             (NSCP_NSPR_ERROR_BASE + 75)

int nscp_nspr_errors[NSCP_NSPR_MAX_ERROR - NSCP_NSPR_ERROR_BASE + 1] = {
    DBT_nspr_errors_0,
    DBT_nspr_errors_1,
    DBT_nspr_errors_2,
    DBT_nspr_errors_3,
    DBT_nspr_errors_4,
    DBT_nspr_errors_5,
    DBT_nspr_errors_6,
    DBT_nspr_errors_7,
    DBT_nspr_errors_8,
    DBT_nspr_errors_9,
    DBT_nspr_errors_10,
    DBT_nspr_errors_11,
    DBT_nspr_errors_12,
    DBT_nspr_errors_13,
    DBT_nspr_errors_14,
    DBT_nspr_errors_15,
    DBT_nspr_errors_16,
    DBT_nspr_errors_17,
    DBT_nspr_errors_18,
    DBT_nspr_errors_19,
    DBT_nspr_errors_20,
    DBT_nspr_errors_21,
    DBT_nspr_errors_22,
    DBT_nspr_errors_23,
    DBT_nspr_errors_24,
    DBT_nspr_errors_25,
    DBT_nspr_errors_26,
    DBT_nspr_errors_27,
    DBT_nspr_errors_28,
    DBT_nspr_errors_29,
    DBT_nspr_errors_30,
    DBT_nspr_errors_31,
    DBT_nspr_errors_32,
    DBT_nspr_errors_33,
    DBT_nspr_errors_34,
    DBT_nspr_errors_35,
    DBT_nspr_errors_36,
    DBT_nspr_errors_37,
    DBT_nspr_errors_38,
    DBT_nspr_errors_39,
    DBT_nspr_errors_40,
    DBT_nspr_errors_41,
    DBT_nspr_errors_42,
    DBT_nspr_errors_43,
    DBT_nspr_errors_44,
    DBT_nspr_errors_45,
    DBT_nspr_errors_46,
    DBT_nspr_errors_47,
    DBT_nspr_errors_48,
    DBT_nspr_errors_49,
    DBT_nspr_errors_50,
    DBT_nspr_errors_51,
    DBT_nspr_errors_52,
    DBT_nspr_errors_53,
    DBT_nspr_errors_54,
    DBT_nspr_errors_55,
    DBT_nspr_errors_56,
    DBT_nspr_errors_57,
    DBT_nspr_errors_58,
    DBT_nspr_errors_59,
    DBT_nspr_errors_60,
    DBT_nspr_errors_61,
    DBT_nspr_errors_62,
    DBT_nspr_errors_63,
    DBT_nspr_errors_64,
    DBT_nspr_errors_65,
    DBT_nspr_errors_66,
    DBT_nspr_errors_67,
    DBT_nspr_errors_68,
    DBT_nspr_errors_69,
    DBT_nspr_errors_70,
    DBT_nspr_errors_71,
    DBT_nspr_errors_72,
    DBT_nspr_errors_73,
    DBT_nspr_errors_74,
    DBT_nspr_errors_75
};

#define NSCP_LIBSEC_ERROR_BASE 		(-8192)
#define NSCP_LIBSEC_MAX_ERROR           (NSCP_LIBSEC_ERROR_BASE + 171)

int nscp_libsec_errors[NSCP_LIBSEC_MAX_ERROR - NSCP_LIBSEC_ERROR_BASE + 1] = {
    DBT_libsec_errors_0,
    DBT_libsec_errors_1,
    DBT_libsec_errors_2,
    DBT_libsec_errors_3,
    DBT_libsec_errors_4,
    DBT_libsec_errors_5,
    DBT_libsec_errors_6,
    DBT_libsec_errors_7,
    DBT_libsec_errors_8,
    DBT_libsec_errors_9,
    DBT_libsec_errors_10,
    DBT_libsec_errors_11,
    DBT_libsec_errors_12,
    DBT_libsec_errors_13,
    DBT_libsec_errors_14,
    DBT_libsec_errors_15,
    DBT_libsec_errors_16,
    DBT_libsec_errors_17,
    DBT_libsec_errors_18,
    DBT_libsec_errors_19,
    DBT_libsec_errors_20,
    DBT_libsec_errors_21,
    DBT_libsec_errors_22,
    DBT_libsec_errors_23,
    DBT_libsec_errors_24,
    DBT_libsec_errors_25,
    DBT_libsec_errors_26,
    DBT_libsec_errors_27,
    DBT_libsec_errors_28,
    DBT_libsec_errors_29,
    DBT_libsec_errors_30,
    DBT_libsec_errors_31,
    DBT_libsec_errors_32,
    DBT_libsec_errors_33,
    DBT_libsec_errors_34,
    DBT_libsec_errors_35,
    DBT_libsec_errors_36,
    DBT_libsec_errors_37,
    DBT_libsec_errors_38,
    DBT_libsec_errors_39,
    DBT_libsec_errors_40,
    DBT_libsec_errors_41,
    DBT_libsec_errors_42,
    DBT_libsec_errors_43,
    DBT_libsec_errors_44,
    DBT_libsec_errors_45,
    DBT_libsec_errors_46,
    DBT_libsec_errors_47,
    DBT_libsec_errors_48,
    DBT_libsec_errors_49,
    DBT_libsec_errors_50,
    DBT_libsec_errors_51,
    DBT_libsec_errors_52,
    DBT_libsec_errors_53,
    DBT_libsec_errors_54,
    DBT_libsec_errors_55,
    DBT_libsec_errors_56,
    DBT_libsec_errors_57,
    DBT_libsec_errors_58,
    DBT_libsec_errors_59,
    DBT_libsec_errors_60,
    DBT_libsec_errors_61,
    DBT_libsec_errors_62,
    DBT_libsec_errors_63,
    DBT_libsec_errors_64,
    DBT_libsec_errors_65,
    DBT_libsec_errors_66,
    DBT_libsec_errors_67,
    DBT_libsec_errors_68,
    DBT_libsec_errors_69,
    DBT_libsec_errors_70,
    DBT_libsec_errors_71,
    DBT_libsec_errors_72,
    DBT_libsec_errors_73,
    DBT_libsec_errors_74,
    DBT_libsec_errors_75,
    DBT_libsec_errors_76,
    DBT_libsec_errors_77,
    DBT_libsec_errors_78,
    DBT_libsec_errors_79,
    DBT_libsec_errors_80,
    DBT_libsec_errors_81,
    DBT_libsec_errors_82,
    DBT_libsec_errors_83,
    DBT_libsec_errors_84,
    DBT_libsec_errors_85,
    DBT_libsec_errors_86,
    DBT_libsec_errors_87,
    DBT_libsec_errors_88,
    DBT_libsec_errors_89,
    DBT_libsec_errors_90,
    DBT_libsec_errors_91,
    DBT_libsec_errors_92,
    DBT_libsec_errors_93,
    DBT_libsec_errors_94,
    DBT_libsec_errors_95,
    DBT_libsec_errors_96,
    DBT_libsec_errors_97,
    DBT_libsec_errors_98,
    DBT_libsec_errors_99,
    DBT_libsec_errors_100,
    DBT_libsec_errors_101,
    DBT_libsec_errors_102,
    DBT_libsec_errors_103,
    DBT_libsec_errors_104,
    DBT_libsec_errors_105,
    DBT_libsec_errors_106,
    DBT_libsec_errors_107,
    DBT_libsec_errors_108,
    DBT_libsec_errors_109,
    DBT_libsec_errors_110,
    DBT_libsec_errors_111,
    DBT_libsec_errors_112,
    DBT_libsec_errors_113,
    DBT_libsec_errors_114,
    DBT_libsec_errors_115,
    DBT_libsec_errors_116,
    DBT_libsec_errors_117,
    DBT_libsec_errors_118,
    DBT_libsec_errors_119,
    DBT_libsec_errors_120,
    DBT_libsec_errors_121,
    DBT_libsec_errors_122,
    DBT_libsec_errors_123,
    DBT_libsec_errors_124,
    DBT_libsec_errors_125,
    DBT_libsec_errors_126,
    DBT_libsec_errors_127,
    DBT_libsec_errors_128,
    DBT_libsec_errors_129,
    DBT_libsec_errors_130,
    DBT_libsec_errors_131,
    DBT_libsec_errors_132,
    DBT_libsec_errors_133,
    DBT_libsec_errors_134,
    DBT_libsec_errors_135,
    DBT_libsec_errors_136,
    DBT_libsec_errors_137,
    DBT_libsec_errors_138,
    DBT_libsec_errors_139,
    DBT_libsec_errors_140,
    DBT_libsec_errors_141,
    DBT_libsec_errors_142,
    DBT_libsec_errors_143,
    DBT_libsec_errors_144,
    DBT_libsec_errors_145,
    DBT_libsec_errors_146,
    DBT_libsec_errors_147,
    DBT_libsec_errors_148,
    DBT_libsec_errors_149,
    DBT_libsec_errors_150,
    DBT_libsec_errors_151,
    DBT_libsec_errors_152,
    DBT_libsec_errors_153,
    DBT_libsec_errors_154,
    DBT_libsec_errors_155,
    DBT_libsec_errors_156,
    DBT_libsec_errors_157,
    DBT_libsec_errors_158,
    DBT_libsec_errors_159,
    DBT_libsec_errors_160,
    DBT_libsec_errors_161,
    DBT_libsec_errors_162,
    DBT_libsec_errors_163,
    DBT_libsec_errors_164,
    DBT_libsec_errors_165,
    DBT_libsec_errors_166,
    DBT_libsec_errors_167,
    DBT_libsec_errors_168,
    DBT_libsec_errors_169,
    DBT_libsec_errors_170,
    DBT_libsec_errors_171
};

#define NSCP_LIBSSL_ERROR_BASE 		(-12288)
#define NSCP_LIBSSL_MAX_ERROR           (NSCP_LIBSSL_ERROR_BASE + 112)

int nscp_libssl_errors[NSCP_LIBSSL_MAX_ERROR - NSCP_LIBSSL_ERROR_BASE + 1] = {
    DBT_libssl_errors_0,
    DBT_libssl_errors_1,
    DBT_libssl_errors_2,
    DBT_libssl_errors_3,
    DBT_libssl_errors_4,
    DBT_libssl_errors_5,
    DBT_libssl_errors_6,
    DBT_libssl_errors_7,
    DBT_libssl_errors_8,
    DBT_libssl_errors_9,
    DBT_libssl_errors_10,
    DBT_libssl_errors_11,
    DBT_libssl_errors_12,
    DBT_libssl_errors_13,
    DBT_libssl_errors_14,
    DBT_libssl_errors_15,
    DBT_libssl_errors_16,
    DBT_libssl_errors_17,
    DBT_libssl_errors_18,
    DBT_libssl_errors_19,
    DBT_libssl_errors_20,
    DBT_libssl_errors_21,
    DBT_libssl_errors_22,
    DBT_libssl_errors_23,
    DBT_libssl_errors_24,
    DBT_libssl_errors_25,
    DBT_libssl_errors_26,
    DBT_libssl_errors_27,
    DBT_libssl_errors_28,
    DBT_libssl_errors_29,
    DBT_libssl_errors_30,
    DBT_libssl_errors_31,
    DBT_libssl_errors_32,
    DBT_libssl_errors_33,
    DBT_libssl_errors_34,
    DBT_libssl_errors_35,
    DBT_libssl_errors_36,
    DBT_libssl_errors_37,
    DBT_libssl_errors_38,
    DBT_libssl_errors_39,
    DBT_libssl_errors_40,
    DBT_libssl_errors_41,
    DBT_libssl_errors_42,
    DBT_libssl_errors_43,
    DBT_libssl_errors_44,
    DBT_libssl_errors_45,
    DBT_libssl_errors_46,
    DBT_libssl_errors_47,
    DBT_libssl_errors_48,
    DBT_libssl_errors_49,
    DBT_libssl_errors_50,
    DBT_libssl_errors_51,
    DBT_libssl_errors_52,
    DBT_libssl_errors_53,
    DBT_libssl_errors_54,
    DBT_libssl_errors_55,
    DBT_libssl_errors_56,
    DBT_libssl_errors_57,
    DBT_libssl_errors_58,
    DBT_libssl_errors_59,
    DBT_libssl_errors_60,
    DBT_libssl_errors_61,
    DBT_libssl_errors_62,
    DBT_libssl_errors_63,
    DBT_libssl_errors_64,
    DBT_libssl_errors_65,
    DBT_libssl_errors_66,
    DBT_libssl_errors_67,
    DBT_libssl_errors_68,
    DBT_libssl_errors_69,
    DBT_libssl_errors_70,
    DBT_libssl_errors_71,
    DBT_libssl_errors_72,
    DBT_libssl_errors_73,
    DBT_libssl_errors_74,
    DBT_libssl_errors_75,
    DBT_libssl_errors_76,
    DBT_libssl_errors_77,
    DBT_libssl_errors_78,
    DBT_libssl_errors_79,
    DBT_libssl_errors_80,
    DBT_libssl_errors_81,
    DBT_libssl_errors_82,
    DBT_libssl_errors_83,
    DBT_libssl_errors_84,
    DBT_libssl_errors_85,
    DBT_libssl_errors_86,
    DBT_libssl_errors_87,
    DBT_libssl_errors_88,
    DBT_libssl_errors_89,
    DBT_libssl_errors_90,
    DBT_libssl_errors_91,
    DBT_libssl_errors_92,
    DBT_libssl_errors_93,
    DBT_libssl_errors_94,
    DBT_libssl_errors_95,
    DBT_libssl_errors_96,
    DBT_libssl_errors_97,
    DBT_libssl_errors_98,
    DBT_libssl_errors_99,
    DBT_libssl_errors_100,
    DBT_libssl_errors_101,
    DBT_libssl_errors_102,
    DBT_libssl_errors_103,
    DBT_libssl_errors_104,
    DBT_libssl_errors_105,
    DBT_libssl_errors_106,
    DBT_libssl_errors_107,
    DBT_libssl_errors_108,
    DBT_libssl_errors_109,
    DBT_libssl_errors_110,
    DBT_libssl_errors_111,
    DBT_libssl_errors_112
};

#define MAXINDEX(a)                     (sizeof(a) / sizeof(a[0]) - 1)
#define RANGE(a)                        (a[MAXINDEX(a)] - a[0])

const char *
nscperror_lookup(int error)
{
    const char *msg = NULL;
    SSLErrorCodes ssl_max_error = SSL_ERROR_END_OF_LIST - SSL_ERROR_BASE - 1;
    SECErrorCodes sec_max_error = SEC_ERROR_END_OF_LIST - SEC_ERROR_BASE -1;
        
    /* When NSS adds new error messages the asserts below will blow up as
     * a reminder. Do the following (or better yet, automate the whole mess):
     *
     * For libssl_errors mismatch:
     *    1. diff $NSS/include/sslerr.h to see what was added.
     *    2. Add libssl error entries to include/base/dbtbase.h
     *    3. Add corresponding entries to nscp_libssl_errors array, above.
     *    4. Update NSCP_LIBSSL_MAX_ERROR definition, above.
     *
     * For libsec_errors mismatch:
     *    1. diff $NSS/include/secerr.h to see what was added.
     *    2. Add libsec error entries to include/base/dbtbase.h
     *    3. Add corresponding entries to nscp_libsec_errors array, above.
     *    4. Update NSCP_LIBSSL_MAX_ERROR definition, above.
     */
    
#if 0
    PR_ASSERT(MAXINDEX(nscp_nspr_errors) == RANGE(nscp_nspr_errors));
    PR_ASSERT(MAXINDEX(nscp_libsec_errors) == RANGE(nscp_libsec_errors));
    PR_ASSERT(MAXINDEX(nscp_libsec_errors) >= sec_max_error);
    PR_ASSERT(MAXINDEX(nscp_libssl_errors) == RANGE(nscp_libssl_errors));
    PR_ASSERT(MAXINDEX(nscp_libssl_errors) >= ssl_max_error);
    
#endif

    if ((error >= NSCP_NSPR_ERROR_BASE) && (error <= NSCP_NSPR_MAX_ERROR)) {
        msg = XP_GetAdminStr(nscp_nspr_errors[error-NSCP_NSPR_ERROR_BASE]);
    } else if ((error >= NSCP_LIBSEC_ERROR_BASE) &&
        (error <= NSCP_LIBSEC_MAX_ERROR)) {
        msg = XP_GetAdminStr(nscp_libsec_errors[error-NSCP_LIBSEC_ERROR_BASE]);
    } else if ((error >= NSCP_LIBSSL_ERROR_BASE) &&
        (error <= NSCP_LIBSSL_MAX_ERROR)) {
        msg = XP_GetAdminStr(nscp_libssl_errors[error-NSCP_LIBSSL_ERROR_BASE]);
    } else {
        PRInt32 msglen = PR_GetErrorTextLength();
        if (msglen > 0) {
            msg = (char *)MALLOC(msglen);
            PR_GetErrorText(msg); 
        } else
            msg = STRDUP("Unknown Error");
    }

    return msg;
}
