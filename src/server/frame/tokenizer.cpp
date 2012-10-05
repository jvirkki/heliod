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

#include "netsite.h"
#include "frame/expr.h"
#include "expr_parse.h"
#include "tokenizer.h"

//-----------------------------------------------------------------------------
// TokenPosition::TokenPosition
//-----------------------------------------------------------------------------

TokenPosition::TokenPosition(int lineArg,
                             int colArg,
                             PRBool newlineArg)
: line(lineArg),
  col(colArg),
  newline(newlineArg)
{ }

//-----------------------------------------------------------------------------
// TokenContent::TokenContent
//-----------------------------------------------------------------------------

TokenContent::TokenContent(TokenType typeArg,
                           const char *valueArg)
: type(typeArg),
  value(valueArg)
{ }

//-----------------------------------------------------------------------------
// TokenContent::operator==
//-----------------------------------------------------------------------------

int TokenContent::operator==(const TokenContent& right) const
{
    return type == right.type && !strcmp(value, right.value);
}

//-----------------------------------------------------------------------------
// TokenContent::operator!=
//-----------------------------------------------------------------------------

int TokenContent::operator!=(const TokenContent& right) const
{
    return type != right.type || strcmp(value, right.value);
}

//-----------------------------------------------------------------------------
// Token::Token
//-----------------------------------------------------------------------------

Token::Token(TokenType typeArg,
             const char *valueArg,
             int lineArg,
             int colArg,
             PRBool newlineArg)
: TokenContent(typeArg, valueArg),
  TokenPosition(lineArg, colArg, newlineArg)
{ }

//-----------------------------------------------------------------------------
// TokenCopy::TokenCopy
//-----------------------------------------------------------------------------

TokenCopy::TokenCopy()
: Token(TOKEN_EOF, "", 1, 1, PR_TRUE),
  storage(NULL)
{ }

TokenCopy::TokenCopy(const Token& token)
{
    init(token);
}

TokenCopy::TokenCopy(TokenType typeArg,
                     const char *valueArg,
                     int lineArg,
                     int colArg,
                     PRBool newlineArg)
: Token(typeArg, valueArg, lineArg, colArg, newlineArg)
{
    storage = strdup(valueArg);
    value = storage;
}

//-----------------------------------------------------------------------------
// TokenCopy::~TokenCopy
//-----------------------------------------------------------------------------

TokenCopy::~TokenCopy()
{
    destroy();
}


//-----------------------------------------------------------------------------
// TokenCopy::operator=
//-----------------------------------------------------------------------------

TokenCopy& TokenCopy::operator=(const Token& right)
{
    if (&right != this) {
        destroy();
        init(right);
    }
    return *this;
}

//-----------------------------------------------------------------------------
// TokenCopy::init
//-----------------------------------------------------------------------------

inline void TokenCopy::init(const Token& token)
{
    type = token.type;
    line = token.line;
    col = token.col;
    newline = token.newline;
    storage = strdup(token.value);
    value = storage;
}

//-----------------------------------------------------------------------------
// TokenCopy::destroy
//-----------------------------------------------------------------------------

inline void TokenCopy::destroy()
{
    free(storage);
}

//-----------------------------------------------------------------------------
// TokenizerIOErrorException::TokenizerIOErrorException
//-----------------------------------------------------------------------------

TokenizerIOErrorException::TokenizerIOErrorException()
{
    error.save();
}

TokenizerIOErrorException::TokenizerIOErrorException(const NsprError& errorArg)
: error(errorArg)
{ }

//-----------------------------------------------------------------------------
// TokenizerUnclosedException::TokenizerUnclosedException
//-----------------------------------------------------------------------------

TokenizerUnclosedException::TokenizerUnclosedException(const TokenPosition& positionArg,
                                                       char openingArg,
                                                       char closingArg)
: position(positionArg),
  opening(openingArg),
  closing(closingArg)
{ }

//-----------------------------------------------------------------------------
// TokenizerCharException::TokenizerCharException
//-----------------------------------------------------------------------------

TokenizerCharException::TokenizerCharException(char cArg,
                                               int line,
                                               int col,
                                               PRBool newline)
: c(cArg),
  position(line, col, newline)
{ }

//-----------------------------------------------------------------------------
// Tokenizer::Tokenizer
//-----------------------------------------------------------------------------

Tokenizer::Tokenizer()
: pushedTokens(2),
  ownedTokens(256)
{ }

//-----------------------------------------------------------------------------
// Tokenizer::~Tokenizer
//-----------------------------------------------------------------------------

Tokenizer::~Tokenizer()
{
    for (int i = 0; i < ownedTokens.length(); i++)
        delete ownedTokens[i];
}

//-----------------------------------------------------------------------------
// Tokenizer::getToken
//-----------------------------------------------------------------------------

const Token& Tokenizer::getToken()
{
    if (Token *poppedToken = pushedTokens.pop())
        return *poppedToken;

    return tokenize();
}

//-----------------------------------------------------------------------------
// Tokenizer::pushToken
//-----------------------------------------------------------------------------

void Tokenizer::pushToken(const Token& token)
{
    pushedTokens.push(addOwnedToken(new TokenCopy(token)));
}

//-----------------------------------------------------------------------------
// Tokenizer::addOwnedToken
//-----------------------------------------------------------------------------

Token *Tokenizer::addOwnedToken(Token *token)
{
    ownedTokens.append(token);
    return token;
}

//-----------------------------------------------------------------------------
// Tokenizer::tokenize
//-----------------------------------------------------------------------------

const Token& Tokenizer::tokenize()
{
    // Tokenizer doesn't have any input source or tokenization logic, so it's
    // always at EOF
    return *addOwnedToken(new TokenCopy(TOKEN_EOF, "", 1, 1, PR_TRUE));
}

//-----------------------------------------------------------------------------
// TokenizerCharSource::TokenizerCharSource
//-----------------------------------------------------------------------------

TokenizerCharSource::TokenizerCharSource(int firstLine, int firstCol)
: line(firstLine),
  col(firstCol - 1),
  lf(PR_FALSE),
  eof(PR_FALSE),
  prevlen(0),
  pushedc(EOF)
{ }

//-----------------------------------------------------------------------------
// TokenizerCharSource::getc
//-----------------------------------------------------------------------------

inline int TokenizerCharSource::getc()
{
    int c;

    if (pushedc != EOF) {
        c = pushedc;
        pushedc = EOF;
    } else {
        c = readc();
    }

    if (lf) {
        prevlen = col;
        line++;
        col = 1;
    } else if (!eof) {
        col++;
    }

    lf = (c == '\n');
    eof = (c == EOF);

    return c;
}

//-----------------------------------------------------------------------------
// TokenizerCharSource::ungetc
//-----------------------------------------------------------------------------

inline void TokenizerCharSource::ungetc(int c)
{
    PR_ASSERT(pushedc == EOF);
    PR_ASSERT(c != EOF);
    pushedc = c;

    if (col == 1) {
        PR_ASSERT(line > 1);
        line--;
        col = prevlen;
        lf = PR_TRUE;
    } else {
        PR_ASSERT(col > 1);
        col--;
        lf = PR_FALSE;
    }
}

//-----------------------------------------------------------------------------
// TokenizerCharSource::getlf
//-----------------------------------------------------------------------------

inline PRBool TokenizerCharSource::getlf()
{
    int c = getc();
    if (c != EOF && c != '\n')
        ungetc(c);
    return (c == '\n');
}

//-----------------------------------------------------------------------------
// TokenizerStringCharSource::TokenizerStringCharSource
//-----------------------------------------------------------------------------

TokenizerStringCharSource::TokenizerStringCharSource(const char *sArg,
                                                     int len,
                                                     int firstLine,
                                                     int firstCol)
: TokenizerCharSource(firstLine, firstCol),
  s(sArg),
  p(sArg),
  end(sArg + len)
{ }

//-----------------------------------------------------------------------------
// TokenizerStringCharSource::getOffset
//-----------------------------------------------------------------------------

int TokenizerStringCharSource::getOffset()
{
    int offset = p - s;
    if (pushedc != EOF)
        offset--;
    return offset;
}

//-----------------------------------------------------------------------------
// TokenizerFilebufCharSource::TokenizerFilebufCharSource
//-----------------------------------------------------------------------------

TokenizerFilebufCharSource::TokenizerFilebufCharSource(filebuf_t *bufArg,
                                                       int firstLine,
                                                       int firstCol)
: TokenizerCharSource(firstLine, firstCol),
  buf(bufArg)
{ }

//-----------------------------------------------------------------------------
// throwUnexpectedCharException
//-----------------------------------------------------------------------------

static void throwUnexpectedCharException(TokenizerCharSource& source,
                                         const NSString& value,
                                         TokenPosition& pos)
{
    // Couldn't find a valid token.  Choose a character to blame.
    int c = source.getc();
    if (c == EOF) {
        PR_ASSERT(value.length() > 0);
        c = value[0];
        throw TokenizerCharException(c, pos.line, pos.col, pos.newline);
    } else {
        int line = source.getLine();
        int col = source.getCol();
        PRBool newline = (pos.newline &&
                          line == pos.line &&
                          col == pos.col);
        throw TokenizerCharException(c, line, col, newline);
    }
}

//-----------------------------------------------------------------------------
// ObjTokenizer::ObjTokenizer
//-----------------------------------------------------------------------------

ObjTokenizer::ObjTokenizer(TokenizerCharSource& sourceArg)
: source(sourceArg),
  prevLine(-1)
{ }

//-----------------------------------------------------------------------------
// ObjTokenizer::tokenize
//-----------------------------------------------------------------------------

const Token& ObjTokenizer::tokenize()
{
    int c;

    // Get the first non-whitespace character
    while (isspace(c = source.getc()));

    // Token begins at this character
    TokenPosition pos(source.getLine(),
                      source.getCol(),
                      source.getLine() != prevLine);
    prevLine = source.getLine();
    PR_ASSERT(pos.line > 0);
    PR_ASSERT(pos.col > 0);

    // Determine the token type and value
    TokenType type;
    NSString value;
    if (c == EOF) {
        // EOF
        type = TOKEN_EOF;

    } else if (c == '#') {
        // Comments end at LF, CRLF, or EOF
        type = TOKEN_COMMENT;
        for (;;) {
            value.append(c);
            c = source.getc();
            if (c == EOF || c == '\n' || (c == '\r' && source.getlf()))
                break;
        }

    } else if (isdigit(c) && !isLeadingIdentifierChar(c)) {
        // The value is a number
        type = TOKEN_NUMBER;
        value.append(c);
        PRBool hex = PR_FALSE;
        if (c == '0') {
            int nextc = source.getc();
            if (nextc == 'x') {
                value.append('x');
                hex = PR_TRUE;
            } else {
                if (nextc != EOF)
                    source.ungetc(nextc);
            }
        }
        for (;;) {
            c = source.getc();
            if (c == EOF)
                break;
            if ((hex && !isxdigit(c)) || (!hex && !isdigit(c))) {
                source.ungetc(c);
                break;
            }
            value.append(c);
        }

    } else if (c == '\'') {
        // Single quote strings end at the first non-escaped "'".  We unescape
        // escaped character sequences as we go.
        type = TOKEN_SINGLE_QUOTE_STRING;
        for (;;) {
            c = source.getc();
            if (c == EOF)
                throw TokenizerUnclosedException(pos, '\'', '\'');
            if (c == '\'')
                break;
            if (c == '\\') {
                // Escaped LF and CRLF are skipped, escaped single quotes
                // are unescaped
                c = source.getc();
                if (c == '\n' || (c == '\r' && source.getlf()))
                    continue;
                if (c != '\'')
                    value.append('\\');
            } else if (c == '\n') {
                // Unescaped LF (e.g. a missing trailing "'") is forbidden
                throw TokenizerUnclosedException(pos, '\'', '\'');
            }
            value.append(c);
        }

    } else if (c == '"') {
        // Double quote strings end at the first non-escaped '"'.  We don't
        // unescape escaped character sequences to avoid losing information
        // required during interpolation (e.g. "\$foo" vs "$foo").
        type = TOKEN_DOUBLE_QUOTE_STRING;
        for (;;) {
            c = source.getc();
            if (c == EOF)
                throw TokenizerUnclosedException(pos, '"', '"');
            if (c == '"')
                break;
            if (c == '\\') {
                // Escaped LF and CRLF are skipped
                c = source.getc();
                if (c == '\n' || (c == '\r' && source.getlf()))
                    continue;
                value.append('\\');
            } else if (c == '\n') {
                // Unescaped LF (e.g. a missing trailing '"') is forbidden
                throw TokenizerUnclosedException(pos, '"', '"');
            }
            value.append(c);
        }

    } else {
        // The value is either an operator, identifier, or invalid
        PRBool checkOperator = PR_TRUE;
        PRBool checkIdentifier = PR_TRUE;
        for (;;) {
            value.append(c);

            PRBool potentialOperator = checkOperator;
            if (potentialOperator) {
                if (!couldBeOperator(value))
                    potentialOperator = PR_FALSE;
            }

            PRBool potentialIdentifier = checkIdentifier;
            if (potentialIdentifier) {
                if (value.length() == 1) {
                    potentialIdentifier = isLeadingIdentifierChar(c);
                } else {
                    potentialIdentifier = isNonleadingIdentifierChar(c);
                }
            }

            if (!potentialOperator && !potentialIdentifier) {
                value.chomp();
                source.ungetc(c);
                break;
            }

            checkOperator = potentialOperator;
            checkIdentifier = potentialIdentifier;

            c = source.getc();
            if (c == EOF)
                break;
        }

        // What type of value was it?
        if (checkOperator && isOperator(value)) {
            type = TOKEN_OPERATOR;
        } else if (checkIdentifier &&
                   value.length() > 0 &&
                   isLeadingIdentifierChar(value.data()[0]))
        {
            type = TOKEN_IDENTIFIER;
        } else {
            throwUnexpectedCharException(source, value, pos);
        }
    }

    // Create a new token object, including a copy of the token value
    Token *token = new TokenCopy(type, value, pos.line, pos.col, pos.newline);

    // The token should be deleted when this tokenizer is destroyed
    addOwnedToken(token);

    return *token;
}

//-----------------------------------------------------------------------------
// DirectiveTokenizer::DirectiveTokenizer
//-----------------------------------------------------------------------------

DirectiveTokenizer::DirectiveTokenizer(TokenizerCharSource& source)
: ObjTokenizer(source)
{ }

//-----------------------------------------------------------------------------
// ExpressionTokenizer::ExpressionTokenizer
//-----------------------------------------------------------------------------

ExpressionTokenizer::ExpressionTokenizer(TokenizerCharSource& source)
: ObjTokenizer(source)
{ }

//-----------------------------------------------------------------------------
// ExpressionTokenizer::isOperator
//-----------------------------------------------------------------------------

PRBool ExpressionTokenizer::isOperator(const NSString& value) const
{
    return expr_is_operator(value, value.length());
}

//-----------------------------------------------------------------------------
// ExpressionTokenizer::couldBeOperator
//-----------------------------------------------------------------------------

PRBool ExpressionTokenizer::couldBeOperator(const NSString& value) const
{
    return expr_could_be_operator(value, value.length());
}

//-----------------------------------------------------------------------------
// ExpressionTokenizer::isLeadingIdentifierChar
//-----------------------------------------------------------------------------

PRBool ExpressionTokenizer::isLeadingIdentifierChar(char c) const
{
    return expr_leading_identifier_char(c);
}

//-----------------------------------------------------------------------------
// ExpressionTokenizer::isNonleadingIdentifierChar
//-----------------------------------------------------------------------------

PRBool ExpressionTokenizer::isNonleadingIdentifierChar(char c) const
{
    return expr_nonleading_identifier_char(c);
}
