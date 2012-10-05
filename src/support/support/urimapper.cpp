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

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "nspr.h"
#include "urimapper.h"

//-----------------------------------------------------------------------------
// UriMapNodeImpl::UriMapNodeImpl
//-----------------------------------------------------------------------------

UriMapNodeImpl::UriMapNodeImpl(const char *_fragment, void *_data, PRBool _isInheritable)
: data(_data),
  isInheritable(_isInheritable),
  isTerminal(PR_TRUE),
  child(NULL),
  sibling(NULL)
{
    fragment = strdup(_fragment);
}

//-----------------------------------------------------------------------------
// UriMapNodeImpl::~UriMapNodeImpl
//-----------------------------------------------------------------------------

UriMapNodeImpl::~UriMapNodeImpl()
{
    free(fragment);
}

//-----------------------------------------------------------------------------
// UriMapImpl::UriMapImpl
//-----------------------------------------------------------------------------

UriMapImpl::UriMapImpl()
: _root(NULL)
{ }

//-----------------------------------------------------------------------------
// UriMapImpl::~UriMapImpl
//-----------------------------------------------------------------------------

UriMapImpl::~UriMapImpl()
{ 
    // Exhaustively delete the children and siblings
    deleteChildren(_root);
    delete _root;
}

//-----------------------------------------------------------------------------
// UriMapImpl::deleteNode
// Delete everything under the node. But, not the node itself.
//-----------------------------------------------------------------------------

void
UriMapImpl::deleteChildren(UriMapNodeImpl* node)
{
    if (node) {
        UriMapNodeImpl *child = node->child;
        while (child) {
            UriMapNodeImpl* sibling = child->sibling;
            deleteChildren(child);
            delete child;
            child = sibling;
        }
    }
}

//-----------------------------------------------------------------------------
// UriMapImpl::addUri
//-----------------------------------------------------------------------------

void UriMapImpl::addUri(const char *path, void *data)
{
    // Add a URI whose data is not inheritable by child path segments
    add(path, data, PR_FALSE);
}

//-----------------------------------------------------------------------------
// UriMapImpl::addUriSpace
//-----------------------------------------------------------------------------

void UriMapImpl::addUriSpace(const char *path, void *data)
{
    // Add a URI whose data is inheritable by child path segments
    int pathlen = strlen(path);
    if (pathlen > 0 && path[pathlen - 1] == '/') {
        // Add a slash-terminated URI whose data is inheritable
        add(path, data, PR_TRUE);

    } else {
        // Add a URI whose data is not inheritable
        add(path, data, PR_FALSE);

        char *pathslash = (char *)malloc(pathlen + 2);
        strcpy(pathslash, path);
        pathslash[pathlen++] = '/';
        pathslash[pathlen++] = '\0';

        // Add a slash-terminated URI whose data is inheritable
        add(pathslash, data, PR_TRUE);

        free(pathslash);
    }
}

//-----------------------------------------------------------------------------
// UriMapImpl::add
//-----------------------------------------------------------------------------

void UriMapImpl::add(const char *path, void *data, PRBool isInheritable)
{
    UriMapNodeImpl **phead = &_root;

    for (;;) {
        // Find a node at this level that begins with *path
        UriMapNodeImpl *node = *phead;
        while (node && *node->fragment != *path)
            node = node->sibling;

        // If no node at this level begins with *path...
        if (node == NULL) {
            // Add a new node at this level
            node = new UriMapNodeImpl(path, data, isInheritable);
            node->sibling = *phead;
            *phead = node;
            break;
        }

        // Compare the incoming path with this node's path fragment
        char *fragment = node->fragment;
        while (*path && *path == *fragment) {
            path++;
            fragment++;
        }

        // If this node extends beyond the incoming path...
        if (*fragment) {
            // child will adopt this node's existing children
            UriMapNodeImpl *child = new UriMapNodeImpl(fragment, node->data, node->isInheritable);
            child->isTerminal = node->isTerminal;
            child->child = node->child;

            // Split this node
            *fragment = '\0'; // modifies node->fragment
            node->data = 0;
            node->isInheritable = PR_FALSE;
            node->isTerminal = PR_FALSE;
            node->child = child;
        }

        // If the path terminates at the end of this fragment...
        if (*path == *fragment) {
            // Found the terminal node for this URI
            node->data = data;
            node->isInheritable = isInheritable;
            node->isTerminal = PR_TRUE;
            break;
        }

        // New node will be a child of this node
        phead = &node->child;
    }
}

//-----------------------------------------------------------------------------
// UriMapImpl::map
//-----------------------------------------------------------------------------

void *UriMapImpl::map(const char *path)
{
    if (_root)
        return map(_root, 0, path);
    else
        return 0;
}

void *UriMapImpl::map(UriMapNodeImpl *node, void *dataUriSpace, const char *path)
{
    while (node) {
        // Compare path with fragment
        const char *fragment = node->fragment;
        while (*path && *path == *fragment) {
            path++;
            fragment++;
        }

        // We're done if path matched exactly
        if (*path == *fragment)
            return node->isTerminal ? node->data : dataUriSpace;

        if (*fragment) {
            // Mismatch, or fragment extends beyond path
            break;

        } else if (*path) {
            // path extends beyond fragment
            UriMapNodeImpl *child = node->child;
            while (child && *child->fragment != *path)
                child = child->sibling;

            if (node->isInheritable && (*path == '/' ||
                node->fragment[strlen(node->fragment) - 1] == '/'))
                dataUriSpace = node->data;

            node = child;

        } else {
            // path matches exactly
            return node->data;
        }
    }

    // Mismatch, fragment extends beyond path, or path extends beyond tree
    return dataUriSpace;
}

//-----------------------------------------------------------------------------
// UriMapperImpl::UriMapperImpl
//-----------------------------------------------------------------------------

UriMapperImpl::UriMapperImpl(const UriMapImpl &map)
: _root(NULL),
  _entries(NULL),
  _used(0),
  _size(0)
{
    UriMapNodeImpl *root = map._root;

    if (root) {
        // Create a stub node that can act as the tree root if necessary
        UriMapNodeImpl stub("", 0, PR_TRUE);
        if (root->sibling != NULL) {
            stub.child = root;
            root = &stub;
        }

        // Do a dry run to find out how big to make the _entries buffer
        add(root, 0, 0, 0);
        _entries = (char *)malloc(_size);

        // Once more with feeling
        _root = add(root, 0, 0, 0);
    }
}

//-----------------------------------------------------------------------------
// UriMapperImpl::~UriMapperImpl
//-----------------------------------------------------------------------------

UriMapperImpl::~UriMapperImpl()
{
    free(_entries);
}

//-----------------------------------------------------------------------------
// UriMapperImpl::add
//-----------------------------------------------------------------------------

UriMapperEntryImpl *UriMapperImpl::add(UriMapNodeImpl *node, void *dataUriSpace, int lenUri, int lenUriSpace)
{
    lenUri += strlen(node->fragment);

    // Create an entry for this node
    UriMapperEntryImpl *entry = allocate(node, dataUriSpace, lenUri, lenUriSpace);

    if (entry) {
        // If this is the end of a path segment...
        if (node->fragment[strlen(node->fragment) - 1] == '/') {
            // All child fragments will be inside our URI space
            dataUriSpace = entry->dataUriSpace;
            lenUriSpace = lenUri;
        }
    }

    // For each of this node's children...
    UriMapNodeImpl *child = node->child;
    while (child) {
        void *dataChildUriSpace = dataUriSpace;
        int lenChildUriSpace = lenUriSpace;

        if (entry) {
            // If this is the beginning of a path segment...
            if (*child->fragment == '/') {
                // This child's fragment will be inside our URI space
                dataChildUriSpace = entry->dataUriSpace;
                lenChildUriSpace = lenUri;
            }

            // Add this child's entry to the hash table
            int i = ((unsigned char)*child->fragment) & entry->hmask;
            entry->ht[i] = add(child, dataChildUriSpace, lenUri, lenChildUriSpace);

        } else {
            // We're just checking how big the _entries buffer should be
            add(child, dataChildUriSpace, lenUri, lenChildUriSpace);
        }

        child = child->sibling;
    }

    return entry;
}

//-----------------------------------------------------------------------------
// UriMapperImpl::allocate
//-----------------------------------------------------------------------------

UriMapperEntryImpl *UriMapperImpl::allocate(UriMapNodeImpl *node, void *dataUriSpace, int lenUri, int lenUriSpace)
{
    // Get length required for path fragment
    int lenFragment = strlen(node->fragment);

    // Get optimal hash table size
    int hsize = getChildHashSize(node);

    // Increase lenFragment so the allocation is a multiple of sizeof(void *)
    int align = (sizeof(UriMapperEntryImpl) + lenFragment) & (sizeof(void *) - 1);
    if (align)
        lenFragment += (sizeof(void *) - align);

    // Get length required for this entry
    int needed = sizeof(UriMapperEntryImpl) + lenFragment + hsize * sizeof(UriMapperEntryImpl *);
    if (!_entries) {
        // We're just checking how big the _entries buffer should be
        _size += needed;
        return NULL;
    }

    // Allocate an entry from the _entries buffer
    char *base = (char *)_entries + _used;
    _used += needed;
    PR_ASSERT(_used <= _size);

    // Initialize the entry
    UriMapperEntryImpl *entry = (UriMapperEntryImpl *)base;
    entry->dataUri = node->isTerminal ? node->data : dataUriSpace;
    entry->dataUriSpace = node->isInheritable ? node->data : dataUriSpace;
    entry->lenUriSpace = node->isInheritable ? lenUri : lenUriSpace;
    entry->ht = (UriMapperEntryImpl **)(base + sizeof(UriMapperEntryImpl) + lenFragment);
    for (int i = 0; i < hsize; i++)
        entry->ht[i] = NULL;
    entry->hmask = (hsize - 1);
    strcpy(entry->fragment, node->fragment);

    return entry;
}

//-----------------------------------------------------------------------------
// UriMapperImpl::getChildHashSize
//-----------------------------------------------------------------------------

int UriMapperImpl::getChildHashSize(UriMapNodeImpl *node)
{
    int numCollisionsMin = INT_MAX;
    int hsizeNumCollisionsMin = 256;

    for (int hsize = 256; hsize > 0; hsize >>= 2) {
        char ht[256];
        int hmask = (hsize - 1);
        int numCollisions = 0;

        memset(ht, 0, sizeof(ht));

        UriMapNodeImpl *child = node->child;
        while (child) {
            if (ht[((unsigned char)*child->fragment) & hmask])
                numCollisions++;
            ht[((unsigned char)*child->fragment) & hmask] = 1;
            child = child->sibling;
        }

        if (numCollisions <= numCollisionsMin) {
            numCollisionsMin = numCollisions;
            hsizeNumCollisionsMin = hsize;
        }
    }

    // We can't handle collisions
    PR_ASSERT(numCollisionsMin == 0);

    return hsizeNumCollisionsMin;
}
