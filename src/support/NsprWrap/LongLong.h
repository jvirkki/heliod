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

#ifndef __LONGLONG_H__
#define __LONGLONG_H__

#include "prlong.h"

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define PRLONGLONG_DLL_API _declspec(dllexport)
#else
#define PRLONGLONG_DLL_API _declspec(dllimport)
#endif
#else
#define PRLONGLONG_DLL_API 
#endif

// Wrapper around the ugly prlong interfaces.  Operates on 64 bit signed
// two's complement ints.

class PRLONGLONG_DLL_API PRLongLong {
public:
  // CONSTRUCTION
  inline PRLongLong() { copy(this->val, LL_ZERO); }

  // DESTRUCTION
  inline ~PRLongLong() {}; 

  // CONVERSION
  inline PRLongLong(const PRLongLong &copy_me) { copy(val, copy_me.val); }
  inline PRLongLong(const PRInt64 &rval) { copy(val, rval); };
  inline operator PRInt64() { return val; }
  void fromInt(int x);

  // ASSIGNMENT
  inline PRLongLong operator=(const PRLongLong &assignme) {
      if (&assignme != this) copy(val, assignme.val);
      return *this;
  };

  inline PRLongLong operator=(const PRInt64 &assignme) {
    copy(val, assignme);
    return *this;
  }

  // ASSIGNMENT ARITHMETIC
  inline PRLongLong operator+=(const PRLongLong &addme) {
    LL_ADD(val, val, addme.val);
    return *this;
  }

  inline PRLongLong operator-=(const PRLongLong &subme) {
    LL_SUB(val, val, subme.val);
    return *this;
  }

  inline PRLongLong operator*=(const PRLongLong &multme) {
    LL_MUL(val, val, multme.val);
    return *this;
  }

  inline PRLongLong operator/=(const PRLongLong &divme) {
    // we do not check for div by zero
    LL_DIV(val, val, divme.val);
    return *this;
  }

  inline PRLongLong operator%=(const PRLongLong &modme) {
    LL_MOD(val, val, modme.val);
    return *this;
  }

  // BITWISE ASSIGMENT
  inline PRLongLong operator^=(const PRLongLong &xorme) {
    LL_XOR(val, val, xorme.val);
    return *this;
  }

  inline PRLongLong operator&=(const PRLongLong &andme) {
    LL_AND(val, val, andme.val);
    return *this;
  }

  inline PRLongLong operator|=(const PRLongLong &orme) {
    LL_OR(val, val, orme.val);
    return *this;
  }

  inline PRLongLong operator<<=(int howfar) {
    LL_SHL(val, val, howfar);
    return *this;
  }

  inline PRLongLong operator>>=(int howfar) {
    LL_SHR(val, val, howfar);
    return *this;
  }

  // ARITHMETIC

  inline PRLongLong operator++(int) {
    // postfix
    PRLongLong rval = *this;
    *this += one();
    return rval;
  }

  inline PRLongLong operator++(void) {
    // prefix
    return *this += one();
  }

  inline PRLongLong operator--(void) {
    // prefix
    return *this -= one();
  }

  inline PRLongLong operator--(int) {
    // postfix
    PRLongLong rval = *this;
    *this -= one();
    return rval;
  }
  inline PRLongLong operator+(const PRLongLong &addme) {
    PRInt64 rval;
    LL_ADD(rval, val, addme.val);
    return PRLongLong(rval);
  };

  inline PRLongLong operator-(const PRLongLong &subme) {
    PRInt64 rval;
    LL_SUB(rval, val, subme.val);
    return PRLongLong(rval);
  };

  inline PRLongLong operator*(const PRLongLong &multme) {
    PRInt64 rval;
    LL_MUL(rval, val, multme.val);
    return PRLongLong(rval);
  };

  inline PRLongLong operator/(const PRLongLong &divme) {
    // we do not check for zero.
    PRInt64 rval;
    LL_DIV(rval, val, divme.val);
    return PRLongLong(rval);
  }
  
  inline PRLongLong operator%(const PRLongLong &modme) {
    PRInt64 rval;
    LL_MOD(rval, val, modme.val);
    return PRLongLong(rval);
  }

  // BITWISE OPERATORS

  inline PRLongLong operator^(const PRLongLong &xorme) {
    PRInt64 rval;
    LL_XOR(rval, val, xorme.val);
    return PRLongLong(rval);
  }

  inline  PRLongLong operator&(const PRLongLong &andme) {
    PRInt64 rval;
    LL_AND(rval, val, andme.val);
    return PRLongLong(rval);
  }

  inline PRLongLong operator|(const PRLongLong &orme) {
    PRInt64 rval;
    LL_OR(rval, val, orme.val);
    return PRLongLong(rval);
  }

  inline PRLongLong operator~() {
    PRInt64 rval;
    LL_NOT(rval, val);
    return PRLongLong(rval);
  }
  
  inline PRLongLong operator<<(int howfar) {
    PRInt64 rval;
    LL_SHL(rval, val, howfar);
    return PRLongLong(rval);
  }

  inline PRLongLong operator>>(int howfar) {
    // sign extended right shift
    PRInt64 rval;
    LL_SHR(rval, val, howfar);
    return PRLongLong(rval);
  }

  // LOGICAL OPERATORS

  inline int operator!() {
    return !LL_IS_ZERO(val);
  }
  
  inline int operator&&(const PRLongLong &logicalandme) {
    return !(LL_IS_ZERO(val) || LL_IS_ZERO(logicalandme.val));
  }

  inline int operator||(const PRLongLong &logicalorme) {
    return !(LL_IS_ZERO(val) && LL_IS_ZERO(logicalorme.val));
  }

  // COMPARATORS

  inline int operator==(const PRLongLong &x) {
    return LL_EQ(x.val, val);
  }
  
  inline int operator!=(const PRLongLong &x) {
    return LL_NE(x.val, val);
  }

  inline int operator<(const PRLongLong &x) {
    return LL_CMP(val, <, x.val);
  }

  inline int operator>(const PRLongLong &x) {
    return LL_CMP(val, >, x.val);
  }

  inline int operator<=(const PRLongLong &x) {
    return LL_CMP(val, <=, x.val);
  }

  inline int operator>=(const PRLongLong &x) {
    return LL_CMP(val, >=, x.val);
  }

  // CONSTANTS

  inline static PRLongLong zero(void) { return PRLongLong(LL_ZERO);   }
         static PRLongLong one(void);
  inline static PRLongLong max_value(void)  { return PRLongLong(LL_MAXINT); }
  inline static PRLongLong min_value(void)  { return PRLongLong(LL_MININT); }
private:
  void copy(PRInt64 &dest, const PRInt64 &src);
  PRInt64 val;
};

#endif // __LONGLONG_H__
