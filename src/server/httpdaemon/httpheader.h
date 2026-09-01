//
// HttpHeader.h
//

#ifndef _HTTPHEADER_H
#define _HTTPHEADER_H

#include "httpdaemon/libdaemon.h"        // HTTPDAEMON_DLL
#include "base/pool.h"
#include "base/keyword.h"
#include "frame/http.h"

/* Define character class mask bits */
#define HHCM_CHAR    0x1
#define HHCM_CTL     0x2
#define HHCM_EOL     0x4
#define HHCM_SPACE   0x8
#define HHCM_DIGIT   0x10
#define HHCM_ALPHA   0x20
#define HHCM_SEP     0x40

enum NSHttpHeader {
    NSHttpHeader_Unrecognized = 0,
    NSHttpHeader_Accept = 1,
    NSHttpHeader_Accept_Charset,
    NSHttpHeader_Accept_Encoding,
    NSHttpHeader_Accept_Language,
    NSHttpHeader_Accept_Ranges,
    NSHttpHeader_Age,
    NSHttpHeader_Allow,
    NSHttpHeader_Authorization,
    NSHttpHeader_Cache_Control,
    NSHttpHeader_Connection,
    NSHttpHeader_Content_Encoding,
    NSHttpHeader_Content_Language,
    NSHttpHeader_Content_Length,
    NSHttpHeader_Content_Location,
    NSHttpHeader_Content_MD5,
    NSHttpHeader_Content_Range,
    NSHttpHeader_Content_Type,
    NSHttpHeader_Cookie,
    NSHttpHeader_Date,
    NSHttpHeader_ETag,
    NSHttpHeader_Expect,
    NSHttpHeader_Expires,
    NSHttpHeader_From,
    NSHttpHeader_Host,
    NSHttpHeader_If_Match,
    NSHttpHeader_If_Modified_Since,
    NSHttpHeader_If_None_Match,
    NSHttpHeader_If_Range,
    NSHttpHeader_If_Unmodified_Since,
    NSHttpHeader_Last_Modified,
    NSHttpHeader_Location,
    NSHttpHeader_Max_Forwards,
    NSHttpHeader_Pragma,
    NSHttpHeader_Proxy_Authenticate,
    NSHttpHeader_Proxy_Authorization,
    NSHttpHeader_Range,
    NSHttpHeader_Referer,
    NSHttpHeader_Retry_After,
    NSHttpHeader_Server,
    NSHttpHeader_TE,
    NSHttpHeader_Trailer,
    NSHttpHeader_Transfer_Encoding,
    NSHttpHeader_Upgrade,
    NSHttpHeader_User_Agent,
    NSHttpHeader_Vary,
    NSHttpHeader_Via,
    NSHttpHeader_Warning,
    NSHttpHeader_WWW_Authenticate,
    /* Add new headers here */
    NSHttpHeaderMax
};

/* String descriptor */
typedef struct HHString HHString;
struct HHString {
    char *ptr;                      /* pointer to start of string */
    int len;                        /* length of string */
};

/* Header descriptor */
typedef struct HHHeader HHHeader;
struct HHHeader {
    HHString tag;                   /* header name string descriptor */
    HHString val;                   /* header value string descriptor */
    NSHttpHeader ix;                /* header index */
    int next;                       /* index in rqHeaders of next */
                                    /*   instance of same header */
};

/* HTTP parse status */
enum HHStatus {
    HHSTATUS_SUCCESS = 0,
    HHSTATUS_IO_ERROR = -1,
    HHSTATUS_IO_EOF = -2,
    HHSTATUS_IO_TIMEOUT = -3,
    HHSTATUS_BAD_REQUEST = -4,
    HHSTATUS_TOO_MANY_HEADERS = -5,
    HHSTATUS_TOO_LARGE = -6,
    HHSTATUS_VERSION_NOT_SUPPORTED = -7
};

class HTTPDAEMON_DLL HttpHeader {

 public:
    HttpHeader();
    ~HttpHeader();

    /* This should be called once prior to creating any instances */
    static PRBool       Initialize();

    /* Helper function - returns a terminated MALLOC copy of HHString */
    static char        *HHDup(pool_handle_t *pool, const HHString *hs);

    /* Parse a comma-separated list of tokens in an HHString */
    static int          ParseCSList(HHString *outstr, const HHString *instr,
                                    int startpos, PRBool &wasQuoted);

    /* Get MaxRqHeaders, the maximum number of headers in a request */
    static int          GetMaxRqHeaders();

    /* Set MaxRqHeaders, the maximum number of headers in a request */
    static void         SetMaxRqHeaders(int n);

    /* Parse an HTTP request */
    HHStatus            ParseRequest(netbuf *buf, PRIntervalTime timeout);

    /* Returns the request line */
    const HHString&     GetRequestLine()               const;

    /* Returns the method from the HTTP request line */
    const HHString&     GetMethod()                    const;

    /* Returns the method number (METHOD_xxxx in nsapi.h) */
    int                 GetMethodNumber()              const;

    /* Returns the request URI as specified in the request line */
    const HHString&     GetRequestURI()                const;

    /* Returns the abs_path part of the URI from the request line */
    const HHString&     GetRequestAbsPath()            const;

    /* Returns the protocol as specified in the request line */
    const HHString     *GetProtocol()                  const;

    /* Return HTTP version number requested by client (in NSAPI format) */
    int                 GetClientProtocolVersion()     const;

    /* Return HTTP version number negotiated with client (in NSAPI format) */
    int                 GetNegotiatedProtocolVersion() const;

    /* Returns host from request URI or Host: header */
    const HHString     *GetHost()                      const;

    /* Returns query part of request URI */
    const HHString     *GetQuery()                     const;

    /* Returns a descriptor for the nth request header */
    const HHHeader     *GetHeader(int n)               const;

    const HHHeader     *GetCacheControl()              const;
    const HHString     *GetConnection()                const;
    const HHHeader     *GetContentLength()             const;
    const HHHeader     *GetIfMatch()                   const;
    const HHString     *GetIfModifiedSince()           const;
    const HHString     *GetIfNoneMatch()               const;
    const HHHeader     *GetIfRange()                   const;
    const HHHeader     *GetIfUnmodifiedSince()         const;
    const HHHeader     *GetPragma()                    const;
    const HHHeader     *GetRange()                     const;
    const HHString     *GetReferer()                   const;
    const HHHeader     *GetTransferEncoding()          const;
    const HHString     *GetUserAgent()                 const;

    PRBool              IsAbsoluteURI()                const;
    PRBool              IsKeepAliveRequested()         const;
    
 private:
    void                AppendHeader(HHHeader *hh, int inew);
    int                 NetbufAppend(netbuf *buf);
    HHStatus            ScanRequestLine(netbuf *buf);
    HHStatus            ScanHeaders(netbuf *buf);
    void                ParseConnectionHeader();

 private:   

    HHString            sRequest;   /* request line */
    HHString            sMethod;    /* request method */
    HHString            sReqURI;    /* request URI */
    HHString            sProtocol;  /* request protocol */
    HHString            sHost;      /* host string */
    HHString            sAbsPath;   /* abs_path part of URI */
    HHString            sQuery;     /* query string */
    int                 iMethod;    /* method index or -1 */
    int                 nClientProtocolVersion;

    PRBool              ScanVersion(int& i, unsigned char*& cp, char*& str, netbuf*& buf);
                                    /* parse protocol version */

    /* Maximum number of HTTP header lines */
    static int    maxRqHeaders;

    /* Array of descriptors for request headers */
    int                 nRqHeaders;
    HHHeader*           rqHeaders;

    /* Pointers to header entries of particular interest */
    HHHeader           *hCacheControl;
    HHHeader           *hConnection;
    HHHeader           *hContentLength;
    HHHeader           *hHost;
    HHHeader           *hIfMatch;
    HHHeader           *hIfModifiedSince;
    HHHeader           *hIfNoneMatch;
    HHHeader           *hIfUnmodifiedSince;
    HHHeader           *hIfRange;
    HHHeader           *hPragma;
    HHHeader           *hRange;
    HHHeader           *hReferer;
    HHHeader           *hTransferEncoding;
    HHHeader           *hUserAgent;

    PRBool              fAbsoluteURI;
    PRBool              fKeepAliveRequested;

    PRIntervalTime      timeRemaining;

    /* character class table */
    static char         httpCharMask[256];

    /* namespace for HTTP method keywords */
    static NSKWSpace   *httpMethods;

    /* namespace for HTTP header keywords */
    static NSKWSpace   *httpHeaders;

    /* Flag for relaxed HTTP compliance */
    static PRBool relaxedHttpCompliance;

};

inline char *HttpHeader::HHDup(pool_handle_t *pool, const HHString *hs)
{
    char *cp = NULL;

    if (hs && hs->ptr) {
        cp = (char *)pool_malloc(pool, hs->len + 1);
        if (cp) {
            memcpy(cp, hs->ptr, hs->len);
            cp[hs->len] = '\0';
        }
    }
    return cp;
}

inline int HttpHeader::GetMaxRqHeaders()
{
    return maxRqHeaders;
}

inline void HttpHeader::AppendHeader(HHHeader *hh, int inew)
{
    int ilast = hh->next;
    /* Last entry on list has negative "next" field */
    if (ilast < 0) {
        hh->next = inew;
    }
    else {
        while (rqHeaders[ilast].next >= 0) {
            ilast = rqHeaders[ilast].next;
        }
        rqHeaders[ilast].next = inew;
    }
}

inline const HHString& HttpHeader::GetRequestLine() const
{
    return sRequest;
}

inline const HHString& HttpHeader::GetMethod() const
{
    return sMethod;
}

inline int HttpHeader::GetMethodNumber() const
{
    return iMethod;
}

inline const HHString& HttpHeader::GetRequestURI() const
{
    return sReqURI;
}

inline const HHString& HttpHeader::GetRequestAbsPath() const
{
    return sAbsPath;
}

inline const HHString *HttpHeader::GetProtocol() const
{
    return (sProtocol.len) ? &sProtocol : NULL;
}

inline int HttpHeader::GetClientProtocolVersion() const
{
    return nClientProtocolVersion;
}

inline int HttpHeader::GetNegotiatedProtocolVersion() const
{
    if (nClientProtocolVersion > HTTPprotv_num) {
        return HTTPprotv_num;
    } else {
        return nClientProtocolVersion;
    }
}

inline const HHString *HttpHeader::GetHost() const
{
    return (sHost.len) ? &sHost : ((hHost) ? &hHost->val : NULL);
}

inline const HHString *HttpHeader::GetQuery() const
{
    return (sQuery.ptr) ? &sQuery : NULL;
}

inline const HHHeader *HttpHeader::GetHeader(int n) const 
{ 
    return (n < nRqHeaders) ? &rqHeaders[n] : NULL;
}

inline const HHHeader *HttpHeader::GetCacheControl() const
{
    return hCacheControl;
}

inline const HHString *HttpHeader::GetConnection() const
{
    return (hConnection) ? &hConnection->val : NULL;
}

inline const HHHeader *HttpHeader::GetContentLength() const
{
    return hContentLength;
}

inline const HHHeader *HttpHeader::GetIfMatch() const
{
    return hIfMatch;
}

inline const HHString *HttpHeader::GetIfModifiedSince() const
{
    return (hIfModifiedSince) ? &hIfModifiedSince->val : NULL;
}

inline const HHString *HttpHeader::GetIfNoneMatch() const
{
    return (hIfNoneMatch) ? &hIfNoneMatch->val : NULL;
}

inline const HHHeader *HttpHeader::GetIfRange() const
{
    return hIfRange;
}

inline const HHHeader *HttpHeader::GetIfUnmodifiedSince() const
{
    return hIfUnmodifiedSince;
}

inline const HHHeader *HttpHeader::GetPragma() const
{
    return hPragma;
}

inline const HHHeader *HttpHeader::GetRange() const
{
    return hRange;
}

inline const HHString *HttpHeader::GetReferer() const
{
    return (hReferer) ? &hReferer->val : NULL;
}

inline const HHHeader *HttpHeader::GetTransferEncoding() const
{
    return hTransferEncoding;
}

inline const HHString *HttpHeader::GetUserAgent() const
{
    return (hUserAgent) ? &hUserAgent->val : NULL;
}

inline PRBool HttpHeader::IsAbsoluteURI() const
{
    return fAbsoluteURI;
}

inline PRBool HttpHeader::IsKeepAliveRequested() const
{
    return fKeepAliveRequested;
}

#endif /* _HTTPHEADER_H */
