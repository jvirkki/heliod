#include <stdio.h>
#include <string.h>

#include <netsite.h>
#include <libaccess/aclproto.h>
#include <sechash.h>   /* PK11 based MD5 routines */
#include <ssl.h>
#include <nss.h>
#include <netsite.h>
#include <base/util.h>
#include <frame/conf.h> /* for conf_getglobals() */
#include <libaccess/digest.h>
#include <pk11func.h>    //resides in clientlibs
#include <base64.h>
//#include <libadmin/httpcode.h>  //SHA1
#include <libaccess/cryptwrapper.h> 


#include <libaccess/FileRealmUser.h>

#define PRIVATE_KEY_LEN  256

char* newSalt(int& saltSize);
char* addSalt(const char* clearPasswd, const char* salt, const int saltSize, int &length);
char* getSalt(const char*hashedText, int& saltSize);

/**
 * Constructor.
 *
 * @param name  User name.
 * @param hash1 served as user's password.
 * @param groups comma separated group memerships.
 */
FileRealmUser::FileRealmUser(const char* name1,const char* hash1,const char* groups1)
{
    if (name1) {
        name.append(name1);
        if (hash1) hash.append(hash1);
        if (groups1) groups.append(groups1);
    }
}


FileRealmUser::~FileRealmUser()
{
}


const NSString& FileRealmUser::getName() const
{
    return name;
}

const NSString& FileRealmUser::getHashedPwd() const
{
    return hash;
}

const NSString& FileRealmUser::getGroups() const
{
    return groups;
}


void FileRealmUser::setPassword(const char* pwd)
{
    hash.clear();
    if (pwd) hash.append(pwd);
}
    
void FileRealmUser::setGroups(const char* grps)
{
    groups.clear();
    if (grps) groups.append(grps);
}


void FileRealmUser::addGroup(const char* grp)
{
    if (grp==NULL) 
        return;
    if (groups.length()>0)
        groups.append(",");
    groups.append(grp);

}

/*
 * @param(IN)  passwd  password string in clear text
 * @return     PR_TRUE if matchs keyfile, PR_FALSE otherwise
 */
PRBool FileRealmUser::sshaVerify(const char* passwd)
{
    PRBool b = PR_FALSE;
    int saltSize=0;
    char *pSalt = getSalt(this->hash, saltSize);
    NSString pB64Hashed;
    sshaPasswd(pSalt,saltSize,passwd,pB64Hashed);
    if (pB64Hashed==hash)
        b = PR_TRUE;
    if (pSalt)
        FREE(pSalt);
    return b;
}

/*
 * @saltSize(OUT)  number of bytes of generated salt
 * @return   the salt in binary format
 */
char* newSalt(int& saltSize)
{
    static const int SALT_SIZE = 8;  //Number of bytes of salt for SSHA
    char tmpbuf[512]; 
    time_t current_time;
    time(&current_time);
    util_itoa(current_time,tmpbuf);    

    char *salt = (char*)MALLOC(SALT_SIZE); //salt = new char [SALT_SIZE]();
    memset(salt,0,SALT_SIZE);
    memcpy(salt,tmpbuf,SALT_SIZE); //salt generated
    saltSize = SALT_SIZE;
    return salt;
}


/*
 * @param(IN)   clearPasswd  password string in clear text
 * @param(IN)   salt the salt in binary format
 * @param(IN)   number of bytes
 * @length(OUT) the total byte of returned hashed password
 * @return      the hashed password with salt(in binary format)
 */
char* addSalt(const char* clearPasswd,const char* salt,const int saltSize,int&length)
{
    length = strlen(clearPasswd)+saltSize+1;
    char *pwdAndSalt = (char *)MALLOC(length);
    memset(pwdAndSalt,0,length);
    memcpy(pwdAndSalt,clearPasswd,strlen(clearPasswd) );
    if (salt==NULL || saltSize<=0)  { //handle no salt case
        length = strlen(clearPasswd);
    } else {
        memcpy(pwdAndSalt+strlen(clearPasswd),salt,saltSize);
        length = strlen(clearPasswd)+saltSize;
    }
    return pwdAndSalt;
}


/*
 * @param(IN)  sshaTagedHashedText  the password field from the keyfile line
 * @param(OUT) saltsize   number of bytes for the salt at end of password field
 * @return     the salt(in binary format). Callers of this function MUST free
 *             this salt. 
 */
char* getSalt(const char*sshaTagedHashedText,int& saltSize)
{
    const char* p = strstr(sshaTagedHashedText,SSHA_TAG);
    if (p==NULL)
        return NULL;
    p = sshaTagedHashedText + strlen(SSHA_TAG);

    int len=0;
    unsigned char *pHashed = ATOB_AsciiToData(p,(unsigned int*)&len);
    if (len<=SHA1_LENGTH || pHashed==NULL)
        return NULL;
    
    saltSize = len-SHA1_LENGTH;
    char *salt = (char *)MALLOC(saltSize +1);
    memcpy(salt, pHashed + SHA1_LENGTH, saltSize);
    salt[saltSize] = '\0';
    PORT_Free(pHashed);

    return salt;
}


/*
 * @param(IN) salt  salt in binary format
 * @param(IN) saltSize   number of bytes for the salt
 * @param(IN) clearPasswd clearPasswd
 * @param(OUT) the hashed password with salt in ascii b64 format
 *
 * #sample keyfile line
 * j2ee;{SSHA}WRWaz20fzw3zN9x5Uzyyk5Wfvrbe4m40rYTPrA==;staff,eng
 */
void sshaPasswd(const char* salt, const int saltSize,const char* clearPasswd,NSString& hashed)
{
    //PRBool workaround= PR_FALSE; //PR_TRUE;
    const int digestLen = SHA1_LENGTH;  //20
    unsigned char H[512];
    char* pwdAndSalt=NULL;
    
    hashed.clear();
    if (!clearPasswd) {
        return;
    }
    int tobeHashedLen = 0;
    pwdAndSalt = addSalt(clearPasswd, salt, saltSize,tobeHashedLen);

        //if (workaround==PR_TRUE) {
        //https_SHA1_Hash(H,pwdAndSalt); //refer lib/libadmin/password.cpp
        //} else {
        //const int keylen = PRIVATE_KEY_LEN;
        //unsigned char DigestPrivatekey[PRIVATE_KEY_LEN];
        //PK11_GenerateRandom(DigestPrivatekey, keylen);

        PK11Context * SHA1Ctx = PK11_CreateDigestContext(SEC_OID_SHA1);
        if (SHA1Ctx==NULL) {
            FREE(pwdAndSalt);
            return;            
        }
        PK11_DigestBegin(SHA1Ctx);
        //PK11_DigestOp(SHA1Ctx, (unsigned char*)pwdAndSalt, strlen(pwdAndSalt) );
        PK11_DigestOp(SHA1Ctx, (unsigned char*)pwdAndSalt, tobeHashedLen );
        PK11_DigestFinal(SHA1Ctx, (unsigned char *)H, (unsigned int*)&digestLen, sizeof(H));
        PK11_DestroyContext(SHA1Ctx, 1);
        PR_ASSERT(digestLen==20);
        //}

    char* base64Val=NULL;
    if (salt!=NULL && saltSize>0) {
        memcpy(H+20,salt,saltSize);   //append salt to hashed passwd
        base64Val = BTOA_DataToAscii(H,digestLen+saltSize);   //base64.h
    } else {
        base64Val = BTOA_DataToAscii(H,digestLen);   //base64.h
    }

    hashed.append(SSHA_TAG );
    hashed.append(base64Val);

    if (base64Val)
        PORT_Free(base64Val);
    base64Val = NULL;
    FREE(pwdAndSalt);
    return;
}

/*
 * @param passwd the clear password of the user
 * @return PR_TRUE is match, PR_FALSE otherwise
 *
 * #sample AuthUserFile line
 * j2ee:Cfnm6lBWVrf0k
*/
PRBool  FileRealmUser::htcryptVerify(const char* passwd)
{
    PRBool b = PR_FALSE;
    const char *hashpwd = (const char *)this->getHashedPwd();
    if (ACL_CryptCompare(passwd, hashpwd, hashpwd) == 0)
        b = PR_TRUE;
    return b;
}

/*
 * @param(IN)  name  user name
 * @param(IN)  realm user's realm
 * @param(IN)  cleartext user's password in clear text
 * @param(OUT) ha1 ha1 of digest auth RFC in ascii b64 format
 */
void ha1Passwd(const char*name,const char* realm,const char* cleartext,NSString& ha1)
{
    HASHHEX HA1;
    DigestCalcHA1((char*)"MD5", (char*)name, (char*)realm, (char*)cleartext,NULL,NULL,HA1);
    ha1.clear();
    ha1.append(HA1);
    return;
}




void FileRealmUser::toString(NSString& line)
{
    line.clear();
    line.append(name);
    line.append(";");
    line.append(hash);
    line.append(";");
    line.append(groups);
    return;
}

