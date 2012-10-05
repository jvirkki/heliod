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

#ifndef SUPPORT_URIMAPPER_H
#define SUPPORT_URIMAPPER_H

#include "support_common.h"
#include "prtypes.h"

//-----------------------------------------------------------------------------
// UriMapNodeImpl
//-----------------------------------------------------------------------------

struct SUPPORT_EXPORT UriMapNodeImpl {
    UriMapNodeImpl(const char *path, void *data, PRBool isInheritable);
    ~UriMapNodeImpl();

    char *fragment;          // Path fragment
    void *data;              // Data associated with URI
    PRBool isInheritable;    // Does data apply to child fragments?
    PRBool isTerminal;       // Does a mapped URI/URI space terminate here?
    UriMapNodeImpl *child;   // First child of this node
    UriMapNodeImpl *sibling; // Next sibling of this node
};

//-----------------------------------------------------------------------------
// UriMapImpl
//-----------------------------------------------------------------------------

class SUPPORT_EXPORT UriMapImpl {
public:
    UriMapImpl();
    ~UriMapImpl();
    void addUri(const char *path, void *data);
    void addUriSpace(const char *path, void *data);
    void *map(const char *path);

private:
    UriMapImpl(const UriMapImpl &);
    UriMapImpl& operator=(const UriMapImpl &);
    void add(const char *path, void *data, PRBool isInheritable);
    void *map(UriMapNodeImpl *node, void *dataUriSpace, const char *path);
    void deleteChildren(UriMapNodeImpl*);

    UriMapNodeImpl *_root;

friend class UriMapperImpl;
};

//-----------------------------------------------------------------------------
// UriMapperEntryImpl
//-----------------------------------------------------------------------------

struct UriMapperEntryImpl {
    void *dataUri;           // Data associated with URI that terminates here
    void *dataUriSpace;      // Data associated with path segments below us
    UriMapperEntryImpl **ht; // Hash table of entries below this entry
    int lenUriSpace;         // Length of the URI space's URI
    int hmask;               // ht[c & hmask] holds entries that begin with c
    char fragment[1];        // Variable length, nul-terminated, path fragment
};

//-----------------------------------------------------------------------------
// UriMapperImpl
//-----------------------------------------------------------------------------

class SUPPORT_EXPORT UriMapperImpl {
public:
    UriMapperImpl(const UriMapImpl &map);
    ~UriMapperImpl();
    void *map(const char *path, const char **suffix = NULL, const char **param = NULL);
    void *map(char *path, char **suffix = NULL, char **param = NULL);

private:
    UriMapperImpl(const UriMapperImpl &);
    UriMapperImpl& operator=(const UriMapperImpl &);
    UriMapperEntryImpl *add(UriMapNodeImpl *node, void *dataUriSpace, int lenUri, int lenUriSpace);
    UriMapperEntryImpl *allocate(UriMapNodeImpl *node, void *dataUriSpace, int lenUri, int lenUriSpace);
    static int getChildHashSize(UriMapNodeImpl *node);

    UriMapperEntryImpl *_root;
    void *_entries;
    int _used;
    int _size;
};

//-----------------------------------------------------------------------------
// UriMap
//-----------------------------------------------------------------------------

template <class UriData> class UriMapper;

/**
 * UriMap maintains mappings between a collection of URIs and URI spaces and
 * the associated UriData values.
 */
template <class UriData>
class SUPPORT_EXPORT UriMap : private UriMapImpl {
public:
    /**
     * Constructs an empty map.
     */
    UriMap();

    /**
     * Adds a URI and its associated UriData to the map. The caller retains
     * responsibility for managing memory associated with the UriData.
     */
    void addUri(const char *path, UriData data);

    /**
     * Adds a URI space and its associated UriData to the map. The caller
     * retains responsibility for managing memory associated with the UriData.
     */
    void addUriSpace(const char *path, UriData data);

    /**
     * Maps a given URI against the map and returns the associated UriData. If
     * there is no UriData associated with the given URI, (UriData)0 is
     * returned. This function does not skip ';'-delimited path parameters and
     * is intended for use when constructing the UriMap. After the UriMap has
     * been constructed, mapping operations should be performed using the
     * more efficient UriMapper::map().
     */
    UriData map(const char *path);

private:
    UriMap(const UriMap &);
    UriMap& operator=(const UriMap &);

friend class UriMapper<UriData>;
};

//-----------------------------------------------------------------------------
// UriMapper
//-----------------------------------------------------------------------------

/**
 * UriMapper is used to map specific URIs against a predefined map.
 */
template <class UriData>
class SUPPORT_EXPORT UriMapper : private UriMapperImpl {
public:
    /**
     * Constructs a UriMapper from a given UriMap. The caller retains
     * responsibility for managing the UriMap. Since the UriMapper constructor
     * creates a copy of the data in the UriMap, the caller can choose to
     * destroy the UriMap as soon as the UriMapper constructor returns. 
     */
    UriMapper(const UriMap<UriData> &map);

    /**
     * Maps a given URI against the UriMap, skipping ';'-delimited path
     * parameters, and returns the associated UriData. A pointer to the first
     * character within the matched URI space and a pointer to the first
     * ';'-delimited parameter encountered are saved for inspection by the
     * caller. If there is no UriData associated with the given URI, (UriData)0
     * is returned.
     */
    UriData map(const char *path, const char **suffix = NULL, const char **param = NULL);
    UriData map(char *path, char **suffix = NULL, char **param = NULL);

private:
    UriMapper(const UriMapper &);
    UriMapper& operator=(const UriMapper &);
};

//-----------------------------------------------------------------------------
// UriMapperImpl::map
//-----------------------------------------------------------------------------

inline void *UriMapperImpl::map(char *path, char **suffix, char **param)
{
    return map(path, (const char **)(void *)suffix, (const char **)(void *)param);
}

inline void *UriMapperImpl::map(const char *path, const char **suffix, const char **param)
{
    const char *ppath = path;
    void *dataUriSpace = 0;
    int lenUriSpace = 0;

    if (param != NULL)
        *param = NULL;

    UriMapperEntryImpl *entry = _root;
    while (entry) {
        // Compare ppath with fragment
        const char *fragment = entry->fragment;
        for (;;) {
            while (*ppath && *ppath == *fragment) {
                ppath++;
                fragment++;
            }

            if (*ppath != ';')
                break;

            // Remember where ;param was
            if (param != NULL) {
                *param = ppath;
                param = NULL;
            }

            // Skip past ;param
            while (*ppath && *ppath != '/')
                ppath++;
        }

        if (*fragment) {
            // Mismatch, or fragment extends beyond path
            break;

        } else if (*ppath) {
            // path extends beyond fragment
            dataUriSpace = entry->dataUriSpace;
            lenUriSpace = entry->lenUriSpace;
            entry = entry->ht[((unsigned char)*ppath) & entry->hmask];

        } else {
            // path matches exactly
            if (suffix != NULL)
                *suffix = ppath;
            return entry->dataUri;
        }
    }

    if (suffix != NULL)
        *suffix = path + lenUriSpace;

    return dataUriSpace;
}

//-----------------------------------------------------------------------------
// UriMap<UriData>::UriMap
//-----------------------------------------------------------------------------

template <class UriData>
inline UriMap<UriData>::UriMap()
{ }

//-----------------------------------------------------------------------------
// UriMap<UriData>::addUri
//-----------------------------------------------------------------------------

template <class UriData>
inline void UriMap<UriData>::addUri(const char *path, UriData data)
{
    UriMapImpl::addUri(path, (void *)data);
}

//-----------------------------------------------------------------------------
// UriMap<UriData>::addUriSpace
//-----------------------------------------------------------------------------

template <class UriData>
inline void UriMap<UriData>::addUriSpace(const char *path, UriData data)
{
    UriMapImpl::addUriSpace(path, (void *)data);
}

//-----------------------------------------------------------------------------
// UriMap<UriData>::map
//-----------------------------------------------------------------------------

template <class UriData>
inline UriData UriMap<UriData>::map(const char *path)
{
    return (UriData)UriMapImpl::map(path);
}

//-----------------------------------------------------------------------------
// UriMapper<UriData>::UriMapper
//-----------------------------------------------------------------------------

template <class UriData>
inline UriMapper<UriData>::UriMapper(const UriMap<UriData> &map)
: UriMapperImpl(map)
{ }

//-----------------------------------------------------------------------------
// UriMapper<UriData>::map
//-----------------------------------------------------------------------------

template <class UriData>
inline UriData UriMapper<UriData>::map(const char *path, const char **suffix, const char **param)
{
    return (UriData)UriMapperImpl::map(path, suffix, param);
}

template <class UriData>
inline UriData UriMapper<UriData>::map(char *path, char **suffix, char **param)
{
    return (UriData)UriMapperImpl::map(path, suffix, param);
}

#endif // SUPPORT_URIMAPPER_H
