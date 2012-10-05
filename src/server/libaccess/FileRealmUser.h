#ifndef __FileRealmUser_H__
#define __FileRealmUser_H__

#include <string.h>
#include "support/NSString.h"
#include <libaccess/fileacl.h>

#define SSHA_TAG    "{SSHA}"
#define DIGEST_TAG  "{DIGEST}"

FILEACL_PUBLIC void sshaPasswd(const char* salt,const int saltSize, const char* clearPasswd,NSString& hashed);
FILEACL_PUBLIC void ha1Passwd(const char*name,const char* realm,const char* cleartext,NSString& ha1);

/*
 * class to store file based user information.
 * ths objects of this class are stored in FileRealm object
 */
class FILEACL_PUBLIC FileRealmUser {
public:
    FileRealmUser(const char* name,const char* hash,const char* groups);
    ~FileRealmUser();

public:
    const NSString& getName() const;
    const NSString& getHashedPwd() const;
    const NSString& getGroups() const;

    void setPassword(const char* pwd);
    void setGroups(const char* grps);
    void addGroup(const char* grp);

public:
    PRBool sshaVerify(const char* passwd);
    PRBool htcryptVerify(const char* passwd);
    void   toString(NSString& line);

private:


private:
    NSString name;    //ex. "j2ee"
    NSString hash;    //ex. "{SSHA}fHIc7qnoMSmgDnkARmM5HnzzCZ4ij4XegpNLvQ=="
    NSString groups;  //ex. "staff,engineering"
};


#endif
