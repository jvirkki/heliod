#ifndef __FileRealm_H__
#define __FileRealm_H__

#include <string.h>
#include "support/NSString.h"
#include <libaccess/fileacl.h>
#include <libaccess/FileRealmUser.h>
#include "libaccess/WSRealm.h"

typedef void (*FileRealmListCB)(FileRealmUser*);

class FILEACL_PUBLIC FileRealm: public WSRealm {
public:
    FileRealm(const char* format);
    ~FileRealm();

public:
    PRBool supportDigest();
    PRBool isKeyfile();
    PRBool isHTAccess();
    PRBool isDigest();

public:
    int  load(const char *file,const char *fileformat);
    int  load(NSErr_t *errp,const char *file,const char *fileformat);
    int  save(const char* filename);
    void add(const FileRealmUser* u);
    FileRealmUser* find(const char* user);
    FileRealmUser* find(const char* user,const char* realm);

// listfn is user supplied callback function that will be called for
// every user in the file database
    void list(FileRealmListCB listfn);
    void remove(FileRealmUser *u);

private:
    FileRealmUser* parse_keyfile_line(char *buf);
    FileRealmUser* parse_user_grp_line(char *line,const char *format);


private:
    PList_t  users;
    NSString dbformat;
};


#endif
