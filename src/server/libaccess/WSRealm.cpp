#include <netsite.h>
#include <frame/conf.h>
#include <libaccess/WSRealm.h>
#include <libaccess/aclerror.h>
#include <libaccess/dbtlibaccess.h>


void free_Vector_pb_param(GenericVector* pVector)
{
    for (int i=0;pVector!=NULL && i<pVector->length();i++) {
      pb_param* pb = (pb_param*)( (*pVector)[i]);
      FREE(pb->name);
      FREE(pb->value);
      FREE(pb);
    }
    delete pVector;
}

void free_Vector_char(GenericVector* pVector)
{
    for (int i=0;pVector!=NULL && i<pVector->length();i++) {
      FREE( (*pVector)[i]);
    }
    delete pVector;
}


WSRealm* WSRealm::getWSRealm(NSErr_t *errp, PList_t resource, PList_t auth_info)
{
    int rv;
    char *dbname;
    ACLMethod_t method;
    ACLDbType_t dbtype;
    void        *pAnyDB=NULL;

    // get hold of the authentication DB name
    if ((rv = ACL_AuthInfoGetDbname(auth_info, &dbname)) < 0) {
        char rv_str[16];
        sprintf(rv_str, "%d", rv);
        nserrGenerate(errp, ACLERRFAIL, ACLERR5830, ACL_Program, 2,
        XP_GetAdminStr(DBT_ldapaclUnableToGetDatabaseName), rv_str);
        return NULL;
    }

    // get the VS's idea of the user database
    rv = ACL_DatabaseFind(errp, dbname, &dbtype, &pAnyDB);
    if (rv != LAS_EVAL_TRUE || ACL_DbTypeLdap == ACL_DBTYPE_INVALID) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR5840, ACL_Program, 2,
                XP_GetAdminStr(DBT_ldapaclUnableToGetParsedDatabaseName), dbname);
        return NULL;
    }
    // XXX elving it's not safe to arbitrarily cast to (WSRealm *)
    return (WSRealm*)pAnyDB;
}
