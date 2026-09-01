/*
 * filedb.cpp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_WIN32
#include "wingetopt.h"
#endif

#include "libaccess/fileacl.h"
#include "libaccess/FileRealm.h"
#include "libaccess/FileRealmUser.h"
#include "nss.h"

int generateUser(int argc, char *argv[])
{
    const char* filename = "filedatabase.txt";
        //const char* delim  = " /\\%&";
    char* dbformat = (char*)DBFORMAT_KEYFILE;
    char* username = NULL;
    char* realm  = NULL;
    char* passwd = NULL;
    char* grps   = NULL;

    int c;
    while  ((c = getopt(argc, argv, "t:u:p:r:g:")) != -1) {
        switch (c) {
            case 't':
                dbformat = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 'r':
                realm = optarg;
                break;
            case 'p':
                passwd = optarg;
                break;
            case 'g':
                grps = optarg;
                break;
            default:
                break;
        }
    }

    if ( strcmp(dbformat,DBFORMAT_KEYFILE)!=0 && strcmp(dbformat,DBFORMAT_DIGEST)!=0 ) {
        fprintf(stdout, "usage: invalid file auth db type\n");
        fprintf(stdout, "usage: filedb -t digest -u username -r realm -p passwd -g groups\n");
        return -1;
    }
    if ( strcmp(dbformat,DBFORMAT_DIGEST)==0 && realm==NULL ) {
        fprintf(stdout, "usage: missing realm name\n");
        fprintf(stdout, "usage: filedb -t digest -u username -r realm -p passwd -g groups\n");
        return -1;
    }    
    if (username==NULL||passwd==NULL) {
        fprintf(stdout, "usage: missing user name or user password\n");
        fprintf(stdout, "usage:  filedb -t digest -u username -r realm -p passwd -g groups\n");
        fprintf(stdout, "example: filedb -t digest  -u j2ee -r filerealm  -p j2eepwd -g 'staff,engineer'\n");
        fprintf(stdout, "example: filedb -t keyfile -u j2ee -p j2eepwd -g 'staff,engineer'\n");
        return -1;
    }
    

    //FileRealm *fRealm = new FileRealm(DBFORMAT_DIGEST);
    //fRealm->load(filename, FILEACL_ATTR_DIGESTFILE);
    FileRealm *fRealm = new FileRealm(dbformat);
    FileRealmUser *pUser = NULL;
    NSString userNew;
    NSString hashed;
    userNew.append(username);
    userNew.strip(NSString::BOTH,' ');
    if (userNew.length()!=strlen(username)) {
        printf("err: there is illegal character(s) in user name.\n");
        return -1;
    }
    /*
    const char* p = userNew.data();
    while (*p) {
        if (!isalnum(*p) && !(*p=='_') && !(*p=='-')) 
            break;
        p++;
    }
    if (*p!=NULL) { 
        printf("err: there is illegal character(s) in user name.\n");
        return -1;
    }
    */
    if ( strcmp(dbformat,DBFORMAT_DIGEST)==0 ) {
        fRealm->load(filename, FILEACL_ATTR_DIGESTFILE);
        ha1Passwd(username,realm,passwd,hashed);
        userNew.append("@");
        userNew.append(realm);
    } else {
        fRealm->load(filename, FILEACL_ATTR_KEYFILE);
        sshaPasswd(username,8,passwd,hashed);  //8 bytes of salt
    }

    pUser = fRealm->find(userNew.data() );
    if (pUser==NULL) {
        pUser = new FileRealmUser(userNew.data(), (char*)hashed.data(),grps);
        fRealm->add(pUser);
    } else {
        pUser->setPassword(hashed.data() );
        pUser->setGroups(grps);
    }

    int errCode = fRealm->save(filename);
    if (errCode==ERR_LINE_TOO_LONG) {   //saving failed
        printf("err: your user name/realm name/group name is too long.\n");
    }
    delete fRealm;
    return 0;
}


int main(int argc,char*argv[])
{
    NSS_NoDB_Init(".");
    return generateUser(argc,argv);
}


