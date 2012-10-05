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


/* A slighly unconventional ifdef here.
 * The minissl.h file is intended to avoid including all of the ssl.h
 * include file.  So if the real ssl.h has been included already (__ssl_h_),
 * then don't include this one because we'll just get redefinitions.
 */
#if !defined(_MINISSL_H_) && !defined(__ssl_h_)
#define _MINISSL_H_

typedef int (*SSLAcceptFunc)(int fd, struct sockaddr *a, int *ap);

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/* Prototypes for SSL I/O functions */
EXTERN int SSL_Connect(int, const void *, int);
EXTERN int SSL_Ioctl(int, int, void*);
EXTERN int SSL_Close(int);
EXTERN int SSL_Socket(int, int, int);
EXTERN int SSL_GetSockOpt(int, int, int, void *, int *);
EXTERN int SSL_SetSockOpt(int, int, int, const void *, int);
EXTERN int SSL_Bind(int, const void *, int);
EXTERN int SSL_Listen(int, int);
EXTERN int SSL_Accept(int, void *, int *);
EXTERN int SSL_Read(int, void *, int);
EXTERN int SSL_Write(int, const void *, int);
EXTERN int SSL_GetPeerName(int, void *, int *);
EXTERN void SSL_AcceptHook(SSLAcceptFunc func);
EXTERN int SSL_DataPendingHack(int);
EXTERN int SSL_ForceHandshake(SYS_NETFD);
EXTERN int SSL_ForceHandshakeWithTimeout(SYS_NETFD, PRUint32);
EXTERN int SSL_ReHandshakeWithTimeout(SYS_NETFD, PRBool, PRUint32);
EXTERN int SSL_SecurityStatus(SYS_NETFD, int *, char **, int *, int *,
                char **, char **);
EXTERN int SSL_Import(SYS_NETFD);

#endif /* _MINISSL_H_ */
