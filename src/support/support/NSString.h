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

#ifndef __NSSTRING_H__
#define __NSSTRING_H__
#include "DynBuf.h"
#include <stdarg.h>
#include <string.h>
#include "nspool.h"

#ifdef WIN32
#ifdef BUILD_SUPPORT_DLL
#define NSSTRING_EXPORT __declspec(dllexport)
#else
#define NSSTRING_EXPORT __declspec(dllimport)
#endif
#else /* Unix */
#define NSSTRING_EXPORT
#endif

// Class: NSString
//  A high performance string class.
//  constraints:
//     (const char *) NSString and (char *) NSString are always NULL terminated
//       (note that they might have internal nulls as well.  Use length())
//
// This class provides ABSOLUTELY no locking.  Provide it in a higher layer.

class NSSTRING_EXPORT NSString {

public:
  /**
   * Specifies the growth increment for small strings.
   *
   * Use this for immutable strings.
   *
   * @see #setGrowthSize
   */
  static const unsigned long SMALL_STRING;

  /**
   * Specifies the growth increment for medium sized strings.
   *
   * Use this for strings such as error messages that are typically a line
   * or more (80 chars or more) in length.
   *
   * @see #setGrowthSize
   */
  static const unsigned long MEDIUM_STRING;

  /**
   * Specifies the growth increment for large strings.
   *
   * This is the default growth increment for <code>NSString</code> objects.
   *
   * @see #setGrowthSize
   */
  static const unsigned long LARGE_STRING;

  enum StripEnum {BOTH=0,LEADING=1,TRAILING=2};

  // CTors
  NSString();

  // copy a C string without allocating excess capacity
  NSString(const char *copy_me, unsigned long len = 0);

#ifdef USE_NSPOOL
  NSString(nspool_handle_t *pool);

  // allocates a dynamic buffer:
  NSString(nspool_handle_t *pool,
           unsigned long capacity, 
           unsigned long growth_size);

  // if you copy an NSString using a static buffer, a dynamic copy will
  // be allocated.
  NSString(nspool_handle_t *pool,
           const NSString &copy);

  // uses a dynamically allocated copy:
  NSString(nspool_handle_t *pool,
                        const char *copy_me, 
                        unsigned long len, 
                        unsigned long capacity, 
                        unsigned long growth_increment);
#endif

  NSString(const NSString &copy);


  // if you assign an NSString using a static buffer, a dynamic copy will
  // be allocated.
  NSString &operator=(const NSString &assign_me);

  // DTor
  ~NSString();


  // The following function causes an NSString to use a statically
  // allocated buffer.  If growth beyond the bounds of that buffer
  // is needed, it will be copied into a dynamically allocated buffer.
  // the caller is responsible for ensuring that the buffer remains
  // valid for the lifetime of the NSString, or for calling forceDynamic
  // BEFORE the buffer becomes invalid.
  // this function effectively resets the string if there are any current
  // contents.
  void useStatic(char *buffer, unsigned long capacity, unsigned long len);

  inline void append(const NSString& s) { append(s.data(), s.length()); }
  inline void append(const char *s) { append(s, strlen(s)); }
  inline void append(const char c) {
    ensureCapacity(_length+1+1);
    char *dest = (_useStatic ? _sBuf : (char *) _dBuf ) + _length;
    dest[0] = c;
    dest[1] = '\0';
    _length++;
  }
  inline void append(const char *s, unsigned long len) {
    ensureCapacity(_length+len+1);
    char *dest = (_useStatic ? _sBuf : (char *) _dBuf ) + _length;
    memcpy(dest, s, len);
    _length+= len;
    *(dest + len ) = '\0';
  }

  void prepend(const char *s, unsigned long len);
  inline void prepend(const char *s) { prepend(s, strlen(s)); }
  inline void prepend(const NSString& s) { prepend(s.data(), s.length()); }

  inline void chomp() {
    if (_length > 0) {
      _length--;
      (_useStatic ? _sBuf : _dBuf)[_length] = '\0';
    }
  }

  int last(char c) const; // index of last occurrence of char c or -1 on fail.

  inline unsigned long length() const { return _length; };

  inline unsigned long capacity() const 
    { return _useStatic ? _sCapacity : _dBuf.capacity(); };

  inline void ensureCapacity(unsigned long requiredCapacity) { 
    if (_useStatic) {
      if (requiredCapacity < _sCapacity)
        return;
      switchToDynamic(requiredCapacity);
    } else {
      _dBuf.ensureCapacity(requiredCapacity);
    }
  }

  void toUpper(void);
  void toLower(void);
  void mixCase(void); // upcase first character, downcase the rest.

  static unsigned int hash(const NSString &hash_me);
  unsigned int hashMe(void) const;

  // replace all instances of 'original' with 'replacement'.  Return number
  // of substitutions.
  unsigned int replace(char original, char replacement);

  inline void         clear() { _length = 0; 
                                if (_sBuf && (_sBuf != EmptyString))
                                  _sBuf[0] = '\0';
                                if ((char *)_dBuf && (char *)_dBuf != EmptyString)
                                  ((char *)_dBuf)[0] = '\0';
                              };

  inline const char * data() const 
    { if (_length)
        return (_useStatic ? _sBuf : (const char *)_dBuf);
      else
        return EmptyString;
    };

  inline char&         operator[](unsigned long i) 
    { 
      // do not use the following, MSVC4.2 barfs on it.  XXXJBS
      // return _useStatic ? _sBuf[i] : _dBuf[i];
      if (_length < i+1) _length = i+1;
      ensureCapacity(_length + 1);
      if (_useStatic) return _sBuf[i];
      return _dBuf[i];
    };

  inline operator const char *() const { return data(); };

  NSSTRING_EXPORT friend int operator==(const NSString &a, const NSString &b);

  // use forceDynamic before placing an NSString in the hands of another keeper
  // it will keep you from reading old stack data or old static buffers.
  inline void forceDynamic(void) { if (_useStatic) switchToDynamic(_length); }

  /**
   * Sets the growth increment step size to the specified value. If this
   * method is used before data is added to the string, then the value
   * specified by this method will also specify the initial size of the 
   * NSString object.
   *
   * By default, NSString allocates 512 byte strings. Use this method
   * to avoid overallocation of memory for simple strings like names,
   * error messages etc.
   */
  unsigned long setGrowthSize(const unsigned long newGrowthSize);

  void printv(const char *fmt, va_list args);

  void printf(const char *fmt, ...);

  void strip(StripEnum where, char toStrip);

private:
  nspool_handle_t *_pool;
  void switchToDynamic(unsigned long ensureCap);
  static const char *EmptyString;
  DynBuf _dBuf;
  char *_sBuf;
  unsigned long _length;
  unsigned long _sCapacity;
  int _useStatic;
};


#endif // __NSSTRING_H__

