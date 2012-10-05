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

#ifndef FRAME_TOKENIZER_H
#define FRAME_TOKENIZER_H

#include <string.h>

#include "base/buffer.h"
#include "support/NSString.h"
#include "support/GenericVector.h"
#include "NsprWrap/NsprError.h"

//-----------------------------------------------------------------------------
// TokenType
//-----------------------------------------------------------------------------

#ifdef XP_WIN32
#define TokenType TokenizerTokenType // Resolve conflict with Win32
#endif

/**
 * TokenType enumerates the different classes of tokens.
 */
enum TokenType {
    TOKEN_EOF = -1,
    TOKEN_COMMENT = 0,
    TOKEN_OPERATOR = 1,
    TOKEN_NUMBER = 2,
    TOKEN_IDENTIFIER = 3,
    TOKEN_SINGLE_QUOTE_STRING = 4,
    TOKEN_DOUBLE_QUOTE_STRING = 5
};

//-----------------------------------------------------------------------------
// TokenPosition
//-----------------------------------------------------------------------------

/**
 * TokenPosition describes a position within some tokenizable entity (e.g. the
 * location of a token within an obj.conf file).
 */
class TokenPosition {
public:
    /**
     * Construct a TokenPosition from a line number, column number, and newline
     * flag.
     */
    TokenPosition(int line, int col, PRBool newline);

    /**
     * A line number, starting from line 1.
     */
    int line;

    /**
     * A column number, starting from column 1.
     */
    int col;

    /**
     * Set when the position is that of the first non-whitespace character on
     * a line.
     */
    PRBool newline;

protected:
    TokenPosition() { }
};

//-----------------------------------------------------------------------------
// TokenContent
//-----------------------------------------------------------------------------

/**
 * TokenContent describes the type and value of a token.
 */
class TokenContent {
public:
    /**
     * Construct a TokenContent from a token type and constant value.  The
     * passed value is not copied; instead, the caller must ensure the passed
     * const char * remains valid for the lifetime of the TokenContent object.
     */
    TokenContent(TokenType type, const char *value);

    /**
     * The type of token this object represents.
     */
    TokenType type;

    /**
     * The token's value.
     */
    const char *value;    

    /**
     * Test two TokenContents for equality.
     */
    int operator==(const TokenContent& right) const;

    /**
     * Test two TokenContents for inequality.
     */
    int operator!=(const TokenContent& right) const;

protected:
    TokenContent() { }
};

//-----------------------------------------------------------------------------
// Token
//-----------------------------------------------------------------------------

/**
 * Token is the base class for tokens.
 */
class Token : public TokenContent, public TokenPosition {
public:
    /**
     * Destroy the token.  Any pointers to the token's value may become
     * invalid.
     */
    virtual ~Token() { };

protected:
    Token() { }
    Token(TokenType type,
          const char *value,
          int line,
          int col,
          PRBool newline);

private:
    Token(const Token& token);
    Token& operator=(const Token& right);
};

//-----------------------------------------------------------------------------
// TokenCopy
//-----------------------------------------------------------------------------

/**
 * TokenCopy is a copy of a token returned from a tokenizer.
 */
class TokenCopy : public Token {
public:
    /**
     * Construct an empty token.  The token is invalid until it is assigned a
     * value.
     */
    TokenCopy();

    /**
     * Construct a TokenCopy from the given token, creating a copy of its
     * value.
     */
    TokenCopy(const Token& token);

    /**
     * Construct a TokenCopy from the given token type, value, line number,
     * column number, and newline flag.  A copy of the passed value is created.
     */
    TokenCopy(TokenType type,
              const char *value,
              int line,
              int col,
              PRBool newline);

    /**
     * Destroy the token.  Any pointers to the token's value will become
     * invalid.
     */
    ~TokenCopy();

    /**
     * Assign one token to another, creating a copy of its value.
     */
    TokenCopy& operator=(const Token& right);

private:
    inline void init(const Token& token);
    inline void destroy();

    char *storage;
};

//-----------------------------------------------------------------------------
// TokenizerException
//-----------------------------------------------------------------------------

/**
 * TokenizerException is thrown when an error or end of file condition is
 * encountered.
 */
class TokenizerException { };

//-----------------------------------------------------------------------------
// TokenizerIOErrorException
//-----------------------------------------------------------------------------

/**
 * TokenizerIOErrorException is thrown when an IO error is encountered.
 */
class TokenizerIOErrorException : public TokenizerException {
public:
    /**
     * Construct a TokenizerIOErrorException, capturing the thread's current
     * error state.
     */
    TokenizerIOErrorException();

    /**
     * Construct a TokenizerIOErrorException using the specified error state.
     */
    TokenizerIOErrorException(const NsprError& error);

    /**
     * Description of the IO error.
     */
    NsprError error;
};

//-----------------------------------------------------------------------------
// TokenizerUnclosedException
//-----------------------------------------------------------------------------

/**
 * TokenizerUnclosedException is thrown when a closing character (e.g. '"')
 * is expected but not found.
 */
class TokenizerUnclosedException : public TokenizerException {
public:
    /**
     * Construct a TokenizerUnclosedException from the position at which the
     * opening character appeared, the opening character, and the expected
     * closing character.
     */
    TokenizerUnclosedException(const TokenPosition& position,
                               char opening,
                               char closing);

    /**
     * The position at which the opening character appeared.
     */
    TokenPosition position;

    /**
     * The opening character.
     */
    char opening;

    /**
     * The expected closing character.
     */
    char closing;
};

//-----------------------------------------------------------------------------
// TokenizerCharException
//-----------------------------------------------------------------------------

/**
 * TokenizerCharException is thrown when an invalid or unexpected character is
 * is encountered.
 */
class TokenizerCharException : public TokenizerException {
public:
    /**
     * Construct a TokenizerCharException from a character and the position at
     * which it appeared.
     */
    TokenizerCharException(char c, int line, int pos, PRBool newline);

    /**
     * The invalid character.
     */
    char c;

    /**
     * The position at which the character appeared.
     */
    TokenPosition position;
};

//-----------------------------------------------------------------------------
// Tokenizer
//-----------------------------------------------------------------------------

/**
 * Tokenizer is the base class for token sources and sinks.
 */
class Tokenizer {
public:
    /**
     * Construct a tokenization source/sink with no pushed tokens or builtin
     * tokenization rules.
     */
    Tokenizer();

    /**
     * Destroy the tokenizer.
     */
    virtual ~Tokenizer();

    /**
     * Return a reference to the next token.  Throws TokenizerException if no
     * more tokens are available.  The token is owned by the tokenizer and will
     * be destroyed when the tokenizer is destroyed.
     */
    const Token& getToken();

    /**
     * Indicate that a copy of the passed token should be returned by the next
     * call to getToken().  If pushToken() is called multiple times before
     * a getToken() call, the pushed tokens are returned in LIFO order.
     */
    void pushToken(const Token& token);

protected:
    /**
     * Derived classes override tokenize() to implement tokenization rules.
     */
    virtual const Token& tokenize();

    /**
     * Derived classes may call addOwnedToken() to indicate that the given
     * Token * should be deleted when the tokenizer is destroyed.  Returns
     * its argument.
     */
    Token *addOwnedToken(Token *token);

private:
    Tokenizer(const Tokenizer& tokenizer);
    Tokenizer& operator=(const Tokenizer& tokenizer);

    PtrVector<Token> pushedTokens;
    PtrVector<Token> ownedTokens;
};

//-----------------------------------------------------------------------------
// TokenizerCharSource
//-----------------------------------------------------------------------------

/**
 * TokenizerCharSource is the abstract base class for tokenizable character
 * sources.
 */
class TokenizerCharSource {
public:
    /**
     * Destroy the TokenizerCharSource.
     */
    virtual ~TokenizerCharSource() { }

    /**
     * Return the next char.  Throws TokenizerIOErrorException on IO errors.
     * Returns EOF if no further chars are available.
     */
    inline int getc();

    /**
     * Push the char most recently returned by getc(), c, back into the
     * TokenizerCharSource.  It is an error to call ungetc() unless the most
     * recent operation on the TokenizerCharSource was a call to getc() that
     * returned the char c.
     */
    void ungetc(int c);

    /**
     * Return the line number, starting from line 1, on which the char most
     * recently returned from getc() appeared.  It is an error to call
     * getLine() nless the most recent operation on the TokenizerCharSource was
     * a call to getc() that returned a char.
     */
    int getLine() const { return line; }

    /**
     * Return the column number, starting from column 1, in which the char most
     * recently returned from getc() appeared.  It is an error to call
     * getLine() unless the most recent operation on the TokenizerCharSource
     * was a call to getc() that returned a char.
     */
    int getCol() const { return col; }

    /**
     * Return PR_TRUE and consume the next char if and only if it is '\n'.
     */
    inline PRBool getlf();

protected:
    TokenizerCharSource(int firstLine, int firstCol);

    /**
     * Called by getc() to read a char.  Throws TokenizerIOErrorException on IO
     * errors.  Returns EOF if no further chars are available.
     */
    virtual inline int readc() = 0;

    int line;
    int col;
    PRBool lf;
    PRBool eof;
    int prevlen;
    int pushedc;
};

//-----------------------------------------------------------------------------
// TokenizerStringCharSource
//-----------------------------------------------------------------------------

/**
 * TokenizerStringCharSource is a TokenizerCharSource wrapper for a string.
 */
class TokenizerStringCharSource : public TokenizerCharSource {
public:
    /**
     * Construct a TokenizerCharSource that reads from the specified string.
     */
    TokenizerStringCharSource(const char *s,
                              int len,
                              int firstLine = 1,
                              int firstCol = 1);
    /**
     * Return the number of characters that have been read from the string.
     */
    int getOffset();

protected:
    inline int readc() {
        if (p == end)
            return EOF;
        return *p++;
    }
private:
    const char *s;
    const char *p;
    const char *end;
};

//-----------------------------------------------------------------------------
// TokenizerFilebufCharSource
//-----------------------------------------------------------------------------

/**
 * TokenizerFilebufCharSource is a TokenizerCharSource wrapper for the legacy
 * NSAPI filebuf_t.
 */
class TokenizerFilebufCharSource : public TokenizerCharSource {
public:
    /**
     * Construct a TokenizerCharSource that reads from the specified filebuf_t.
     * Note that the caller retains ownership of the passed filebuf_t; it will
     * not be destroyed when the TokenizerFilebufCharSource is destroyed.
     */
    TokenizerFilebufCharSource(filebuf_t *buf,
                               int firstLine = 1,
                               int firstCol = 1);

protected:
    inline int readc() {
        int c = filebuf_getc(buf);
        if (c == IO_ERROR)
            throw TokenizerIOErrorException();
        if (c == IO_EOF)
            c = EOF;
        return c;
    }
private:
    filebuf_t *buf;
};

//-----------------------------------------------------------------------------
// ObjTokenizer
//-----------------------------------------------------------------------------

/**
 * ObjTokenizer is the abstract base class for obj.conf tokenization.
 */
class ObjTokenizer : public Tokenizer {
public:
    TokenizerCharSource& getSource() { return source; }

protected:
    ObjTokenizer(TokenizerCharSource& source);
    const Token& tokenize();
    virtual inline PRBool isOperator(const NSString& value) const = 0;
    virtual inline PRBool couldBeOperator(const NSString& value) const = 0;
    virtual inline PRBool isLeadingIdentifierChar(char c) const = 0;
    virtual inline PRBool isNonleadingIdentifierChar(char c) const = 0;

private:
    TokenizerCharSource& source;
    int prevLine;
};

//-----------------------------------------------------------------------------
// DirectiveTokenizer
//-----------------------------------------------------------------------------

/**
 * DirectiveTokenizer tokenizes obj.conf directives.
 */
class DirectiveTokenizer : public ObjTokenizer {
public:
    DirectiveTokenizer(TokenizerCharSource& source);

protected:

    inline PRBool isOperator(const NSString& value) const {
        // Directive operators are "<", "=", ">", "{", "}", and "</"
        switch (value.length()) {
        case 1:
            switch (value[0]) {
            case '<': return PR_TRUE;
            case '=': return PR_TRUE;
            case '>': return PR_TRUE;
            case '{': return PR_TRUE;
            case '}': return PR_TRUE;
            }
            return PR_FALSE;
        case 2:
            return value[0] == '<' && value[1] == '/';
        default:
            return PR_FALSE;
        }
    }
    inline PRBool couldBeOperator(const NSString& value) const {
        // The only 2-character operator begins with the same character as a
        // 1-character operator, so couldBeOperator is equivalent to isOperator
        return isOperator(value);
    }
    inline PRBool isLeadingIdentifierChar(char c) const {
        return !isspace(c) &&
               c != '\\' &&
               c != '\'' &&
               c != '"' &&
               c != '{' &&
               c != '}' &&
               c != '<' &&
               c != '=' &&
               c != '>';
    }
    inline PRBool isNonleadingIdentifierChar(char c) const {
        return isLeadingIdentifierChar(c);
    }
};

//-----------------------------------------------------------------------------
// ExpressionTokenizer
//-----------------------------------------------------------------------------

/**
 * ExpressionTokenizer tokenizes expressions.
 */
class ExpressionTokenizer : public ObjTokenizer {
public:
    ExpressionTokenizer(TokenizerCharSource& source);

protected:
    PRBool isOperator(const NSString& value) const;
    PRBool couldBeOperator(const NSString& value) const;
    PRBool isLeadingIdentifierChar(char c) const;
    PRBool isNonleadingIdentifierChar(char c) const;
};

#endif // FRAME_TOKENIZER_H
