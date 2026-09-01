#include <stdio.h>
#include <string.h>

#include <netsite.h>
#include <base/pblock.h>
#include <base/plist.h>
#include <base/buffer.h>
#include <base/file.h>
#include <base/util.h>
#include <base/pool.h>
#include <base/ereport.h>
#include <libaccess/nserror.h>
#include <libaccess/aclerror.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclproto.h>
#include <libaccess/fileacl.h>
#include <libaccess/FileRealmUser.h>
#include <libaccess/FileRealm.h>


/*
 * constructor.
 * format is one of {"keyfile","htaccess","digest"} as defined as
 * DBFORMAT_* in fileacl.h
 */
FileRealm::FileRealm(const char* format)
{
    users = PListNew(NULL);
    dbformat.append(format);
    setRealmType(ACL_DbTypeFile);
}

FileRealm::~FileRealm()
{
    PListDestroy(users);
}

/*
 * need to ACL_CritEnter/ACL_CritExit
 */
FileRealmUser* FileRealm::find(const char* name)
{
    FileRealmUser* filerealmUser = NULL;
    int idx = PListFindValue(users,name,(void**)&filerealmUser,0);
    return filerealmUser;
}

FileRealmUser* FileRealm::find(const char* name, const char* realm)
{
    if (!name || !*name)
        return NULL;
    if (!realm || !*realm)
        return find(name);
    
    NSString userWithRealm;
    userWithRealm.append(name);
    userWithRealm.append("@");
    userWithRealm.append(realm);
    return find(userWithRealm.data() );
}


/*
 * need to ACL_CritEnter/ACL_CritExit
 */
void FileRealm::add(const FileRealmUser* u)
{
    PListInitProp(users, 0, u->getName(), u, NULL);
}

void FileRealm::remove(FileRealmUser *u)
{
   PListDeleteProp(users, 0, u->getName());
}

PRBool FileRealm::supportDigest()
{
    return isDigest();
}


PRBool FileRealm::isKeyfile()
{
    if (strcmp(dbformat.data(),DBFORMAT_KEYFILE)==0)
        return PR_TRUE;
    return PR_FALSE;
}


PRBool FileRealm::isHTAccess()
{
    if ( strcmp(dbformat.data(),DBFORMAT_HTACCESS)==0)
        return PR_TRUE;
    return PR_FALSE;
}

PRBool FileRealm::isDigest()
{
    if ( strcmp(dbformat.data(),DBFORMAT_DIGEST)==0)
        return PR_TRUE;
    return PR_FALSE;
}

/*
 * read in authentication database (keyfile, htaccess's user/group file, digest file)
 * and store the information in FileRealm object
 * @param format is one of {"keyfile","userfile", "groupfile", "digestfile"}
 * it is the file format, NOT the dbformat.
 */
/*
int FileRealm::load(const char *file,const char *fileformat)
{
    FILE *fp;
    char buf[MAX_LINE_LEN];

    buf[0] = 0;

    if (file==NULL) {
        return -1;
    }
#ifdef XP_WIN32
    if ((fp = fopen(file, "rt")) == NULL)
#else
    if ((fp = fopen(file, "r")) == NULL)
#endif
    {
        return -1;
    }

    FileRealmUser* u= NULL;
    while (fgets(buf, sizeof(buf), fp) ) {
        u=NULL;
        if (buf[0] == '#' )
            continue;
        if ( strcmp(fileformat,FILEACL_ATTR_KEYFILE)==0 ||  strcmp(fileformat,FILEACL_ATTR_DIGESTFILE)==0)
            u = parse_keyfile_line(buf);
        else {
            u = parse_user_grp_line(buf,fileformat);
        }
        if (u!=NULL) {
            this->add(u);
        }
    }
    fclose(fp);
    return 0;
}
*/

/*
 this function is for admin CGI use.
 */
int FileRealm::load(const char *filename,const char *fileformat)
{
    return load(NULL,filename,fileformat);
}


/*
 * read in authentication database (keyfile, htaccess's user/group file, digest file)
 * and store the information in FileRealm object
 * @param format is one of {"keyfile","userfile", "groupfile", "digestfile"}
 * it is the file format, NOT the dbformat.
 * @return  -1 fail; 0 success
 */
int FileRealm::load(NSErr_t *errp,const char *filename,const char *fileformat)
{
    char line[MAX_LINE_LEN];
    FileRealmUser* u= NULL;
    filebuf_t* buf=NULL;
    SYS_FILE fd;
    PRFileInfo pr_info;

    fd = system_fopenRO((char*)filename); // Open the file
    if (fd == SYS_ERROR_FD) {
        if (errp) {
            nserrGenerate(errp,ACLERRFAIL, ACLERR6430, ACL_Program, 3, XP_GetAdminStr(DBT_fileaclErrorOpenFile),filename, system_errmsg() );
        }
        return -1;
    }
    if (PR_GetOpenFileInfo(fd, &pr_info) != -1){
        if (pr_info.size<=0) {
            system_fclose(fd);
            return 0;//ok if file is empty.filebuf_open will return NULL is file length=0.
        }
    }
    buf = filebuf_open(fd, FILE_BUFFERSIZE); // Create a file_buf for the file
    if (!buf) {
        system_fclose(fd);
        if (errp) {
            nserrGenerate(errp,ACLERRFAIL, ACLERR6430, ACL_Program, 3, XP_GetAdminStr(DBT_fileaclErrorOpenFile),filename, system_errmsg() );
        }
        return -1;
    }

    int counter=0;
    int rc=0;
    for (;;) {
        counter++;
        rc = util_getline(buf, counter, MAX_LINE_LEN, line);
        if (rc<0)  {  //ERROR(-1)
            if (errp) {
                nserrGenerate(errp,ACLERRFAIL, ACLERR6440, ACL_Program,2, XP_GetAdminStr(DBT_fileaclErrorReadingFile),filename );
            }
            break;
        }
        if (rc==1 && !*line) break;  //EOF(1) && nothing has read
        
        // Valid line
        if (!*line || (*line == '#')) continue;
        if ( strcmp(fileformat,FILEACL_ATTR_KEYFILE)==0 || strcmp(fileformat,FILEACL_ATTR_DIGESTFILE)==0)
            u = parse_keyfile_line(line);
        else {
            u = parse_user_grp_line(line,fileformat);
        }
        if (u!=NULL) {
            this->add(u);
        }
        if (rc==1) break;  //EOF
    }//end loop
    filebuf_close(buf);
    if (rc!=1)  return -1;
    return 0;
}


/*
 * #syntax in keyfile (user;pwd-info;group[,group]*)
 * #sample keyfile
 * #List of users for simple file realm. Empty by default.
 * ias;{SSHA}fHIc7qnoMSmgDnkARmM5HnzzCZ4ij4XegpNLvQ==;staff
 * j2ee;{SSHA}WRWaz20fzw3zN9x5Uzyyk5Wfvrbe4m40rYTPrA==;staff,eng
 */
FileRealmUser* FileRealm::parse_keyfile_line(char *buf)
{
    static const char delimeter_char = ';';
    char  *lastchar;

    lastchar = strrchr(buf, '\n');    
    if (lastchar) *lastchar = '\0'; // remove the last char if it is newline
    
    char *tmp;
    char *start=buf;
    tmp=strchr(start,delimeter_char);
    if (tmp==NULL)
        return NULL;

    *tmp++ = NULL;
    char *userName = NULL;
    char *hashedPwd = NULL;
    char *groups = NULL;

    userName = start;
    start=tmp;
    tmp=(*start)?strchr(start,delimeter_char):NULL;
    if (tmp) {
        *tmp++ = NULL;
        hashedPwd = start;
        start = tmp;
        if (*start) {
           groups = start;
        }
    }
    FileRealmUser* u =  new FileRealmUser(userName,hashedPwd,groups);
    return u;
}

/*
 * #syntax in AuthUserFile (user:pwd-info)
 * #sample AuthUserFile (user only)
 * ias:rezK5Hb3FagOg
 * j2ee:Cfnm6lBWVrf0k
 *
 * #syntax in AuthGroupFile (group:username[ username]*)
 * #sample AuthGroupFile (group only)
 * staff:j2ee ias
 * engineering:j2ee
 *
 * #if AuthUserFile==AuthGroupFile, the synatx is (user:pwd-info:group[,group]*)
 * #sample user/group mixed
 * j2ee:Cfnm6lBWVrf0k:staff,engineering
 * ias:rezK5Hb3FagOg:staff
 */
FileRealmUser* FileRealm::parse_user_grp_line(char *line, const char *format)
{
    static const char delimeter_char = ':';

    register char *start=line;
    register char *tmp;

    if(start==NULL || *start==NULL) {
        return NULL;
    }
    tmp = strrchr(start, '\n');    
    if (tmp) *tmp = NULL;

    if(!(tmp = strchr(start, delimeter_char)))
        return NULL;

    *tmp++ = NULL;
    FileRealmUser* u = NULL;
    if ( strcmp(format,FILEACL_ATTR_GROUPFILE)==0 ) {
        char *saveGrp = start;
        start=tmp;
        while (start!=NULL) {
            tmp = strchr(start,' ');
            if (tmp) *tmp++ = NULL;
            u=this->find(start);
            if (u) u->addGroup(saveGrp);
            start = tmp;
        }
        return NULL;
    } else {
        u = new FileRealmUser(start,NULL,NULL);
        start = tmp;
        if(!(tmp = strchr(start,delimeter_char))) {
            u->setPassword(start);
        } else {
            *tmp++ = NULL;
            u->setPassword(start);
            u->setGroups(tmp);
        }
        return u;
    }
    return NULL;
}

static void validate(char* name, const void* val,void* user_data) 
{
    int* errCode = (int*)(user_data);
    FileRealmUser* user = (FileRealmUser*)val;
    NSString line;
    user->toString(line);
    if (line.length() >= MAX_LINE_LEN)
        *errCode=ERR_LINE_TOO_LONG;
    return;
}


static void saveLine(char* name, const void* val,void* user_data )
{
    FILE *fp = (FILE*)user_data;
    FileRealmUser* user = (FileRealmUser*)val;
    NSString line;
    user->toString(line);
    fprintf(fp,"%s\n",line.data());
    return;
}



int FileRealm::save(const char* filename)
{
    int errCode=0;
    PListEnumerate(users,validate,&errCode);  //validate each user
    if (errCode!=0)
        return errCode;
    FILE *fp;
    if (!(fp = fopen(filename, "w"))) {
        fprintf(stderr, "Could not open password file %s.\n",filename);
        perror("fopen");
        exit(1);
    }
    PListEnumerate(users,saveLine,fp);
    fclose(fp);
    return 0;
}

static void FileRealmListUsers(char* name, const void* val, void* user_data)
{
    FileRealmUser* user = (FileRealmUser*)val;
    FileRealmListCB list_cb = (FileRealmListCB)user_data;
    (*list_cb)(user);
}

void FileRealm::list(FileRealmListCB list_cb)
{
    PListEnumerate(users, FileRealmListUsers, (void*)list_cb);
}
