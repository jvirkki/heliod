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

/*
 * Thunk layer to hook old nspr lib name to new name.
 * Written by Robin J. Maxwell 02-18-99
 */
#include <windows.h>

#define UP_MODULE_NAME "libnspr4.dll"

HINSTANCE hDll = 0;

//
// Pointers to symbols in the good library
void *_GetExecutionEnvironment;
void *_LL_MaxInt;
void *_LL_MinInt;
void *_LL_Zero;
void *_PRP_DestroyNakedCondVar;
void *_PRP_NakedBroadcast;
void *_PRP_NakedNotify;
void *_PRP_NakedWait;
void *_PRP_NewNakedCondVar;
void *_PRP_TryLock;
void *_PR_Abort;
void *_PR_Accept;
void *_PR_AcceptRead;
void *_PR_Access;
void *_PR_AddToCounter;
void *_PR_AddWaitFileDesc;
void *_PR_AllocFileDesc;
void *_PR_Assert;
void *_PR_AtomicAdd;
void *_PR_AtomicDecrement;
void *_PR_AtomicIncrement;
void *_PR_AtomicSet;
void *_PR_AttachThread;
void *_PR_AttachThreadGCAble;
void *_PR_Available;
void *_PR_Available64;
void *_PR_Bind;
void *_PR_BlockClockInterrupts;
void *_PR_CEnterMonitor;
void *_PR_CExitMonitor;
void *_PR_CNotify;
void *_PR_CNotifyAll;
void *_PR_CWait;
void *_PR_CallOnce;
void *_PR_Calloc;
void *_PR_CancelWaitFileDesc;
void *_PR_CancelWaitGroup;
void *_PR_CeilingLog2;
void *_PR_ChangeFileDescNativeHandle;
void *_PR_Cleanup;
void *_PR_ClearInterrupt;
void *_PR_ClearThreadGCAble;
void *_PR_Close;
void *_PR_CloseDir;
void *_PR_CloseFileMap;
void *_PR_Connect;
void *_PR_CreateAlarm;
void *_PR_CreateCounter;
void *_PR_CreateFileMap;
void *_PR_CreateIOLayerStub;
void *_PR_CreateMWaitEnumerator;
#ifndef USING_NSPR40
void *_PR_CreateNetAddr;
#endif
void *_PR_CreateOrderedLock;
void *_PR_CreatePipe;
void *_PR_CreateProcess;
void *_PR_CreateProcessDetached;
void *_PR_CreateStack;
void *__PR_CreateThread;
void *_PR_CreateThreadGCAble;
void *_PR_CreateTrace;
void *_PR_CreateWaitGroup;
void *_PR_DecrementCounter;
void *_PR_Delete;
void *_PR_DestroyAlarm;
void *_PR_DestroyCondVar;
void *_PR_DestroyCounter;
void *_PR_DestroyLock;
void *_PR_DestroyMWaitEnumerator;
void *_PR_DestroyMonitor;
#ifndef USING_NSPR40
void *_PR_DestroyNetAddr;
#endif
void *_PR_DestroyOrderedLock;
void *_PR_DestroyPollableEvent;
void *_PR_DestroyProcessAttr;
void *_PR_DestroySem;
void *_PR_DestroyStack;
void *_PR_DestroyTrace;
void *_PR_DestroyWaitGroup;
void *_PR_DetachProcess;
void *_PR_DetachThread;
void *_PR_DisableClockInterrupts;
void *_PR_EnableClockInterrupts;
void *_PR_EnterMonitor;
void *_PR_EnumerateHostEnt;
void *_PR_EnumerateThreads;
void *_PR_EnumerateWaitGroup;
void *_PR_ExitMonitor;
void *_PR_ExplodeTime;
void *_PR_FD_CLR;
void *_PR_FD_ISSET;
void *_PR_FD_NCLR;
void *_PR_FD_NISSET;
void *_PR_FD_NSET;
void *_PR_FD_SET;
void *_PR_FD_ZERO;
#ifndef USING_NSPR40
void *_PR_FamilyInet;
#endif
void *_PR_FileDesc2NativeHandle;
void *_PR_FindLibrary;
void *_PR_FindNextCounterQname;
void *_PR_FindNextCounterRname;
void *_PR_FindNextTraceQname;
void *_PR_FindNextTraceRname;
void *_PR_FindSymbol;
void *_PR_FindSymbolAndLibrary;
void *_PR_FloorLog2;
void *_PR_FormatTime;
void *_PR_FormatTimeUSEnglish;
void *_PR_Free;
void *_PR_FreeFileDesc;
void *_PR_FreeLibraryName;
void *_PR_GMTParameters;
void *_PR_GetConnectStatus;
void *_PR_GetCounter;
void *_PR_GetCounterHandleFromName;
void *_PR_GetCounterNameFromHandle;
void *_PR_GetCurrentThread;
void *_PR_GetDefaultIOMethods;
void *_PR_GetDescType;
void *_PR_GetDirectorySeparator;
void *_PR_GetDirectorySepartor;
void *_PR_GetEnv;
void *_PR_GetError;
#ifndef USING_NSPR40
void *_PR_GetErrorString;
#endif
void *_PR_GetErrorText;
void *_PR_GetErrorTextLength;
void *_PR_GetFileInfo;
void *_PR_GetFileInfo64;
void *_PR_GetFileMethods;
void *_PR_GetGCRegisters;
void *_PR_GetHostByAddr;
void *_PR_GetHostByName;
#ifndef USING_NSPR40
void *_PR_GetHostName;
#endif
void *_PR_GetIdentitiesLayer;
void *_PR_GetLayersIdentity;
void *_PR_GetLibraryName;
void *_PR_GetLibraryPath;
void *_PR_GetMonitorEntryCount;
void *_PR_GetNameForIdentity;
void *_PR_GetOSError;
void *_PR_GetOpenFileInfo;
void *_PR_GetOpenFileInfo64;
void *_PR_GetPageShift;
void *_PR_GetPageSize;
void *_PR_GetPeerName;
void *_PR_GetProtoByName;
void *_PR_GetProtoByNumber;
void *_PR_GetSP;
void *_PR_GetSockName;
#ifndef USING_NSPR40
void *_PR_GetSockOpt;
#endif
void *_PR_GetSocketOption;
void *_PR_GetSpecialFD;
void *_PR_GetStackSpaceLeft;
void *_PR_GetSystemInfo;
void *_PR_GetTCPMethods;
void *_PR_GetThreadAffinityMask;
void *_PR_GetThreadID;
void *_PR_GetThreadPriority;
void *_PR_GetThreadPrivate;
void *_PR_GetThreadScope;
void *_PR_GetThreadState;
void *_PR_GetThreadType;
void *_PR_GetTraceEntries;
void *_PR_GetTraceHandleFromName;
void *_PR_GetTraceNameFromHandle;
void *_PR_GetTraceOption;
void *_PR_GetUDPMethods;
void *_PR_GetUniqueIdentity;
void *_PR_GetValueSem;
void *_PR_ImplodeTime;
void *_PR_ImportFile;
void *_PR_ImportTCPSocket;
void *_PR_ImportUDPSocket;
void *_PR_IncrementCounter;
void *_PR_Init;
void *_PR_Initialize;
void *_PR_InitializeNetAddr;
void *_PR_Initialized;
void *_PR_Interrupt;
void *_PR_IntervalNow;
void *_PR_IntervalToMicroseconds;
void *_PR_IntervalToMilliseconds;
void *_PR_IntervalToSeconds;
void *_PR_JoinThread;
void *_PR_KillProcess;
void *_PR_Listen;
void *_PR_LoadLibrary;
void *_PR_LoadStaticLibrary;
void *_PR_LocalTimeParameters;
void *_PR_Lock;
void *_PR_LockFile;
void *_PR_LockOrderedLock;
void *_PR_LogFlush;
void *_PR_LogPrint;
void *_PR_Malloc;
void *_PR_MemMap;
void *_PR_MemUnmap;
void *_PR_MicrosecondsToInterval;
void *_PR_MillisecondsToInterval;
void *_PR_MkDir;
void *_PR_NTFast_Accept;
void *_PR_NTFast_AcceptRead;
void *_PR_NTFast_AcceptRead_WithTimeoutCallback;
void *_PR_NTFast_UpdateAcceptContext;
#ifndef USING_NSPR40
void *_PR_NT_UseNonblock;
void *_PR_NetAddrSize;
#endif
void *_PR_NetAddrToString;
void *_PR_NewCondVar;
void *_PR_NewLock;
void *_PR_NewLogModule;
void *_PR_NewMonitor;
void *_PR_NewNamedMonitor;
void *_PR_NewPollableEvent;
void *_PR_NewProcessAttr;
void *_PR_NewSem;
void *_PR_NewTCPSocket;
void *_PR_NewTCPSocketPair;
void *_PR_NewThreadPrivateIndex;
void *_PR_NewUDPSocket;
void *_PR_NormalizeTime;
void *_PR_Notify;
void *_PR_NotifyAll;
void *_PR_NotifyAllCondVar;
void *_PR_NotifyCondVar;
void *_PR_Now;
void *_PR_Open;
void *_PR_OpenDir;
void *_PR_ParseTimeString;
void *_PR_Poll;
void *_PR_PopIOLayer;
void *_PR_PostSem;
void *_PR_ProcessAttrSetCurrentDirectory;
void *_PR_ProcessAttrSetStdioRedirect;
void *_PR_ProcessExit;
void *_PR_PushIOLayer;
void *_PR_Read;
void *_PR_ReadDir;
void *_PR_Realloc;
void *_PR_RecordTraceEntries;
void *_PR_Recv;
void *_PR_RecvFrom;
void *_PR_Rename;
void *_PR_ResetAlarm;
void *_PR_ResetProcessAttr;
void *_PR_ResumeAll;
void *_PR_RmDir;
void *_PR_ScanStackPointers;
void *_PR_SecondsToInterval;
void *_PR_Seek;
void *_PR_Seek64;
void *_PR_Select;
void *_PR_Send;
void *_PR_SendTo;
void *_PR_SetAlarm;
void *_PR_SetCPUAffinityMask;
void *_PR_SetConcurrency;
void *_PR_SetCounter;
void *_PR_SetError;
void *_PR_SetErrorText;
void *_PR_SetFDCacheSize;
#ifndef USING_NSPR40
void *_PR_SetIPv6Enable;
#endif
void *_PR_SetLibraryPath;
void *_PR_SetLogBuffering;
void *_PR_SetLogFile;
void *_PR_SetPollableEvent;
#ifndef USING_NSPR40
void *_PR_SetSockOpt;
#endif
void *_PR_SetSocketOption;
void *_PR_SetStdioRedirect;
void *_PR_SetThreadAffinityMask;
void *_PR_SetThreadDumpProc;
void *_PR_SetThreadGCAble;
void *_PR_SetThreadPriority;
void *_PR_SetThreadPrivate;
void *_PR_SetThreadRecycleMode;
void *_PR_SetTraceOption;
void *_PR_ShowStatus;
void *_PR_Shutdown;
void *_PR_Sleep;
void *_PR_Socket;
void *_PR_StackPop;
void *_PR_StackPush;
void *_PR_Stat;
void *_PR_StringToNetAddr;
void *_PR_SubtractFromCounter;
void *_PR_SuspendAll;
void *_PR_Sync;
void *_PR_TLockFile;
void *_PR_TestAndEnterMonitor;
void *_PR_TestAndLock;
void *_PR_ThreadScanStackPointers;
void *_PR_TicksPerSecond;
void *_PR_Trace;
void *_PR_TransmitFile;
void *_PR_USPacificTimeParameters;
void *_PR_UnblockClockInterrupts;
void *_PR_UnloadLibrary;
void *_PR_Unlock;
void *_PR_UnlockFile;
void *_PR_UnlockOrderedLock;
void *_PR_VersionCheck;
void *_PR_Wait;
void *_PR_WaitCondVar;
void *_PR_WaitForPollableEvent;
void *_PR_WaitProcess;
void *_PR_WaitRecvReady;
void *_PR_WaitSem;
void *_PR_Write;
void *_PR_Writev;
void *_PR_Yield;
void *_PR_cnvtf;
void *_PR_dtoa;
void *_PR_fprintf;
void *_PR_htonl;
void *_PR_htonll;
void *_PR_htons;
void *_PR_ntohl;
void *_PR_ntohll;
void *_PR_ntohs;
void *_PR_smprintf;
void *_PR_smprintf_free;
void *_PR_snprintf;
void *_PR_sprintf_append;
void *_PR_sscanf;
void *_PR_strtod;
void *_PR_sxprintf;
void *_PR_vfprintf;
void *_PR_vsmprintf;
void *_PR_vsnprintf;
void *_PR_vsprintf_append;
void *_PR_vsxprintf;
void *_SetExecutionEnvironment;
void *__PR_AddSleepQ;
void *___PR_CreateThread;
void *__PR_DelSleepQ;
void *__PR_GetPrimordialCPU;
void *__PR_NativeCreateThread;
void *_libVersionPoint;

extern "C" {
__declspec( naked ) __declspec(dllexport) void GetExecutionEnvironment(void)
{
_asm jmp DWORD PTR _GetExecutionEnvironment
}

__declspec( naked ) __declspec(dllexport) void LL_MaxInt(void)
{
_asm jmp DWORD PTR _LL_MaxInt
}

__declspec( naked ) __declspec(dllexport) void LL_MinInt(void)
{
_asm jmp DWORD PTR _LL_MinInt
}

__declspec( naked ) __declspec(dllexport) void LL_Zero(void)
{
_asm jmp DWORD PTR _LL_Zero
}

__declspec( naked ) __declspec(dllexport) void PRP_DestroyNakedCondVar(void)
{
_asm jmp DWORD PTR _PRP_DestroyNakedCondVar
}

__declspec( naked ) __declspec(dllexport) void PRP_NakedBroadcast(void)
{
_asm jmp DWORD PTR _PRP_NakedBroadcast
}

__declspec( naked ) __declspec(dllexport) void PRP_NakedNotify(void)
{
_asm jmp DWORD PTR _PRP_NakedNotify

}

__declspec( naked ) __declspec(dllexport) void PRP_NakedWait(void)
{
_asm jmp DWORD PTR _PRP_NakedWait
}

__declspec( naked ) __declspec(dllexport) void PRP_NewNakedCondVar(void)
{
_asm jmp DWORD PTR _PRP_NewNakedCondVar
}

__declspec( naked ) __declspec(dllexport) void PRP_TryLock(void)
{
_asm jmp DWORD PTR _PRP_TryLock
}

__declspec( naked ) __declspec(dllexport) void PR_Abort(void)
{
_asm jmp DWORD PTR _PR_Abort
}

__declspec( naked ) __declspec(dllexport) void PR_Accept(void)
{
_asm jmp DWORD PTR _PR_Accept
}

__declspec( naked ) __declspec(dllexport) void PR_AcceptRead(void)
{
_asm jmp DWORD PTR _PR_AcceptRead
}

__declspec( naked ) __declspec(dllexport) void PR_Access(void)
{
_asm jmp DWORD PTR _PR_Access
}

__declspec( naked ) __declspec(dllexport) void PR_AddToCounter(void)
{
_asm jmp DWORD PTR _PR_AddToCounter
}

__declspec( naked ) __declspec(dllexport) void PR_AddWaitFileDesc(void)
{
_asm jmp DWORD PTR _PR_AddWaitFileDesc
}

__declspec( naked ) __declspec(dllexport) void PR_AllocFileDesc(void)
{
_asm jmp DWORD PTR _PR_AllocFileDesc
}

__declspec( naked ) __declspec(dllexport) void PR_Assert(void)
{

_asm jmp DWORD PTR _PR_Assert
}

__declspec( naked ) __declspec(dllexport) void PR_AtomicAdd(void)
{
_asm jmp DWORD PTR _PR_AtomicAdd
}

__declspec( naked ) __declspec(dllexport) void PR_AtomicDecrement(void)
{
_asm jmp DWORD PTR _PR_AtomicDecrement
}

__declspec( naked ) __declspec(dllexport) void PR_AtomicIncrement(void)
{
_asm jmp DWORD PTR _PR_AtomicIncrement
}

__declspec( naked ) __declspec(dllexport) void PR_AtomicSet(void)
{
_asm jmp DWORD PTR _PR_AtomicSet
}

__declspec( naked ) __declspec(dllexport) void PR_AttachThread(void)
{
_asm jmp DWORD PTR _PR_AttachThread
}

__declspec( naked ) __declspec(dllexport) void PR_AttachThreadGCAble(void)
{
_asm jmp DWORD PTR _PR_AttachThreadGCAble
}

__declspec( naked ) __declspec(dllexport) void PR_Available(void)
{
_asm jmp DWORD PTR _PR_Available
}

__declspec( naked ) __declspec(dllexport) void PR_Available64(void)
{
   _asm jmp DWORD PTR _PR_Available64
}

__declspec( naked ) __declspec(dllexport) void PR_Bind(void)
{
   _asm jmp DWORD PTR _PR_Bind
}

__declspec( naked ) __declspec(dllexport) void PR_BlockClockInterrupts(void)
{
_asm jmp DWORD PTR _PR_BlockClockInterrupts
}

__declspec( naked ) __declspec(dllexport) void PR_CEnterMonitor(void)
{
   _asm jmp DWORD PTR _PR_CEnterMonitor
}

__declspec( naked ) __declspec(dllexport) void PR_CExitMonitor(void)
{
_asm jmp DWORD PTR _PR_CExitMonitor
}

__declspec( naked ) __declspec(dllexport) void PR_CNotify(void)
{
_asm jmp DWORD PTR _PR_CNotify
}

__declspec( naked ) __declspec(dllexport) void PR_CNotifyAll(void)
{
_asm jmp DWORD PTR _PR_CNotifyAll
}

__declspec( naked ) __declspec(dllexport) void PR_CWait(void)
{
_asm jmp DWORD PTR _PR_CWait
}

__declspec( naked ) __declspec(dllexport) void PR_CallOnce(void)
{
_asm jmp DWORD PTR _PR_CallOnce
}

__declspec( naked ) __declspec(dllexport) void PR_Calloc(void)
{
_asm jmp DWORD PTR _PR_Calloc
}

__declspec( naked ) __declspec(dllexport) void PR_CancelWaitFileDesc(void)
{
_asm jmp DWORD PTR _PR_CancelWaitFileDesc
}

__declspec( naked ) __declspec(dllexport) void PR_CancelWaitGroup(void)
{
_asm jmp DWORD PTR _PR_CancelWaitGroup
}

__declspec( naked ) __declspec(dllexport) void PR_CeilingLog2(void)
{
_asm jmp DWORD PTR _PR_CeilingLog2
}

__declspec( naked ) __declspec(dllexport) void PR_ChangeFileDescNativeHandle(void)
{
_asm jmp DWORD PTR _PR_ChangeFileDescNativeHandle
}

__declspec( naked ) __declspec(dllexport) void PR_Cleanup(void)
{
_asm jmp DWORD PTR _PR_Cleanup
}

__declspec( naked ) __declspec(dllexport) void PR_ClearInterrupt(void)
{
_asm jmp DWORD PTR _PR_ClearInterrupt
}

__declspec( naked ) __declspec(dllexport) void PR_ClearThreadGCAble(void)
{
_asm jmp DWORD PTR _PR_ClearThreadGCAble
}

__declspec( naked ) __declspec(dllexport) void PR_Close(void)
{
_asm jmp DWORD PTR _PR_Close
}

__declspec( naked ) __declspec(dllexport) void PR_CloseDir(void)
{
_asm jmp DWORD PTR _PR_CloseDir
}

__declspec( naked ) __declspec(dllexport) void PR_CloseFileMap(void)
{
_asm jmp DWORD PTR _PR_CloseFileMap
}

__declspec( naked ) __declspec(dllexport) void PR_Connect(void)
{
_asm jmp DWORD PTR _PR_Connect
}

__declspec( naked ) __declspec(dllexport) void PR_CreateAlarm(void)
{
_asm jmp DWORD PTR _PR_CreateAlarm
}

__declspec( naked ) __declspec(dllexport) void PR_CreateCounter(void)
{
_asm jmp DWORD PTR _PR_CreateCounter
}

__declspec( naked ) __declspec(dllexport) void PR_CreateFileMap(void)
{
_asm jmp DWORD PTR _PR_CreateFileMap
}

__declspec( naked ) __declspec(dllexport) void PR_CreateIOLayerStub(void)
{
_asm jmp DWORD PTR _PR_CreateIOLayerStub
}

__declspec( naked ) __declspec(dllexport) void PR_CreateMWaitEnumerator(void)
{
_asm jmp DWORD PTR _PR_CreateMWaitEnumerator
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_CreateNetAddr(void)
{
_asm jmp DWORD PTR _PR_CreateNetAddr
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_CreateOrderedLock(void)
{
_asm jmp DWORD PTR _PR_CreateOrderedLock
}

__declspec( naked ) __declspec(dllexport) void PR_CreatePipe(void)
{
_asm jmp DWORD PTR _PR_CreatePipe
}

__declspec( naked ) __declspec(dllexport) void PR_CreateProcess(void)
{
_asm jmp DWORD PTR _PR_CreateProcess
}

__declspec( naked ) __declspec(dllexport) void PR_CreateProcessDetached(void)
{
_asm jmp DWORD PTR _PR_CreateProcessDetached
}

__declspec( naked ) __declspec(dllexport) void PR_CreateStack(void)
{
_asm jmp DWORD PTR _PR_CreateStack
}

__declspec( naked ) __declspec(dllexport) void PR_CreateThread(void)
{
_asm jmp DWORD PTR __PR_CreateThread
}

__declspec( naked ) __declspec(dllexport) void PR_CreateThreadGCAble(void)
{
_asm jmp DWORD PTR _PR_CreateThreadGCAble
}

__declspec( naked ) __declspec(dllexport) void PR_CreateTrace(void)
{
_asm jmp DWORD PTR _PR_CreateTrace
}

__declspec( naked ) __declspec(dllexport) void PR_CreateWaitGroup(void)
{
_asm jmp DWORD PTR _PR_CreateWaitGroup
}

__declspec( naked ) __declspec(dllexport) void PR_DecrementCounter(void)
{
_asm jmp DWORD PTR _PR_DecrementCounter
}

__declspec( naked ) __declspec(dllexport) void PR_Delete(void)
{
_asm jmp DWORD PTR _PR_Delete
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyAlarm(void)
{
_asm jmp DWORD PTR _PR_DestroyAlarm
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyCondVar(void)
{
_asm jmp DWORD PTR _PR_DestroyCondVar
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyCounter(void)
{
_asm jmp DWORD PTR _PR_DestroyCounter
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyLock(void)
{
_asm jmp DWORD PTR _PR_DestroyLock
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyMWaitEnumerator(void)
{
_asm jmp DWORD PTR _PR_DestroyMWaitEnumerator
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyMonitor(void)
{
_asm jmp DWORD PTR _PR_DestroyMonitor
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_DestroyNetAddr(void)
{
_asm jmp DWORD PTR _PR_DestroyNetAddr
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_DestroyOrderedLock(void)
{
_asm jmp DWORD PTR _PR_DestroyOrderedLock
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyPollableEvent(void)
{
_asm jmp DWORD PTR _PR_DestroyPollableEvent
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyProcessAttr(void)
{
_asm jmp DWORD PTR _PR_DestroyProcessAttr
}

__declspec( naked ) __declspec(dllexport) void PR_DestroySem(void)
{
_asm jmp DWORD PTR _PR_DestroySem
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyStack(void)
{
_asm jmp DWORD PTR _PR_DestroyStack
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyTrace(void)
{
_asm jmp DWORD PTR _PR_DestroyTrace
}

__declspec( naked ) __declspec(dllexport) void PR_DestroyWaitGroup(void)
{
_asm jmp DWORD PTR _PR_DestroyWaitGroup
}

__declspec( naked ) __declspec(dllexport) void PR_DetachProcess(void)
{
_asm jmp DWORD PTR _PR_DetachProcess
}

__declspec( naked ) __declspec(dllexport) void PR_DetachThread(void)
{
_asm jmp DWORD PTR _PR_DetachThread
}

__declspec( naked ) __declspec(dllexport) void PR_DisableClockInterrupts(void)
{
_asm jmp DWORD PTR _PR_DisableClockInterrupts
}

__declspec( naked ) __declspec(dllexport) void PR_EnableClockInterrupts(void)
{
_asm jmp DWORD PTR _PR_EnableClockInterrupts
}

__declspec( naked ) __declspec(dllexport) void PR_EnterMonitor(void)
{
_asm jmp DWORD PTR _PR_EnterMonitor
}

__declspec( naked ) __declspec(dllexport) void PR_EnumerateHostEnt(void)
{
_asm jmp DWORD PTR _PR_EnumerateHostEnt
}

__declspec( naked ) __declspec(dllexport) void PR_EnumerateThreads(void)
{
_asm jmp DWORD PTR _PR_EnumerateThreads
}

__declspec( naked ) __declspec(dllexport) void PR_EnumerateWaitGroup(void)
{
_asm jmp DWORD PTR _PR_EnumerateWaitGroup
}

__declspec( naked ) __declspec(dllexport) void PR_ExitMonitor(void)
{
_asm jmp DWORD PTR _PR_ExitMonitor
}

__declspec( naked ) __declspec(dllexport) void PR_ExplodeTime(void)
{
_asm jmp DWORD PTR _PR_ExplodeTime
}

__declspec( naked ) __declspec(dllexport) void PR_FD_CLR(void)
{
_asm jmp DWORD PTR _PR_FD_CLR
}

__declspec( naked ) __declspec(dllexport) void PR_FD_ISSET(void)
{
_asm jmp DWORD PTR _PR_FD_ISSET
}

__declspec( naked ) __declspec(dllexport) void PR_FD_NCLR(void)
{
_asm jmp DWORD PTR _PR_FD_NCLR
}

__declspec( naked ) __declspec(dllexport) void PR_FD_NISSET(void)
{
_asm jmp DWORD PTR _PR_FD_NISSET
}

__declspec( naked ) __declspec(dllexport) void PR_FD_NSET(void)
{
_asm jmp DWORD PTR _PR_FD_NSET
}

__declspec( naked ) __declspec(dllexport) void PR_FD_SET(void)
{
_asm jmp DWORD PTR _PR_FD_SET
}

__declspec( naked ) __declspec(dllexport) void PR_FD_ZERO(void)
{
_asm jmp DWORD PTR _PR_FD_ZERO
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_FamilyInet(void)
{
_asm jmp DWORD PTR _PR_FamilyInet
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_FileDesc2NativeHandle(void)
{
_asm jmp DWORD PTR _PR_FileDesc2NativeHandle
}

__declspec( naked ) __declspec(dllexport) void PR_FindLibrary(void)
{
_asm jmp DWORD PTR _PR_FindLibrary
}

__declspec( naked ) __declspec(dllexport) void PR_FindNextCounterQname(void)
{
_asm jmp DWORD PTR _PR_FindNextCounterQname
}

__declspec( naked ) __declspec(dllexport) void PR_FindNextCounterRname(void)
{
_asm jmp DWORD PTR _PR_FindNextCounterRname
}

__declspec( naked ) __declspec(dllexport) void PR_FindNextTraceQname(void)
{
_asm jmp DWORD PTR _PR_FindNextTraceQname
}

__declspec( naked ) __declspec(dllexport) void PR_FindNextTraceRname(void)
{
_asm jmp DWORD PTR _PR_FindNextTraceRname
}

__declspec( naked ) __declspec(dllexport) void PR_FindSymbol(void)
{
_asm jmp DWORD PTR _PR_FindSymbol
}

__declspec( naked ) __declspec(dllexport) void PR_FindSymbolAndLibrary(void)
{
_asm jmp DWORD PTR _PR_FindSymbolAndLibrary
}

__declspec( naked ) __declspec(dllexport) void PR_FloorLog2(void)
{
_asm jmp DWORD PTR _PR_FloorLog2
}

__declspec( naked ) __declspec(dllexport) void PR_FormatTime(void)
{
_asm jmp DWORD PTR _PR_FormatTime
}

__declspec( naked ) __declspec(dllexport) void PR_FormatTimeUSEnglish(void)
{
_asm jmp DWORD PTR _PR_FormatTimeUSEnglish
}

__declspec( naked ) __declspec(dllexport) void PR_Free(void)
{
_asm jmp DWORD PTR _PR_Free
}

__declspec( naked ) __declspec(dllexport) void PR_FreeFileDesc(void)
{
_asm jmp DWORD PTR _PR_FreeFileDesc
}

__declspec( naked ) __declspec(dllexport) void PR_FreeLibraryName(void)
{
_asm jmp DWORD PTR _PR_FreeLibraryName
}

__declspec( naked ) __declspec(dllexport) void PR_GMTParameters(void)
{
_asm jmp DWORD PTR _PR_GMTParameters
}

__declspec( naked ) __declspec(dllexport) void PR_GetConnectStatus(void)
{
_asm jmp DWORD PTR _PR_GetConnectStatus
}

__declspec( naked ) __declspec(dllexport) void PR_GetCounter(void)
{
_asm jmp DWORD PTR _PR_GetCounter
}

__declspec( naked ) __declspec(dllexport) void PR_GetCounterHandleFromName(void)
{
_asm jmp DWORD PTR _PR_GetCounterHandleFromName
}

__declspec( naked ) __declspec(dllexport) void PR_GetCounterNameFromHandle(void)
{
_asm jmp DWORD PTR _PR_GetCounterNameFromHandle
}

__declspec( naked ) __declspec(dllexport) void PR_GetCurrentThread(void)
{
_asm jmp DWORD PTR _PR_GetCurrentThread
}

__declspec( naked ) __declspec(dllexport) void PR_GetDefaultIOMethods(void)
{
_asm jmp DWORD PTR _PR_GetDefaultIOMethods
}

__declspec( naked ) __declspec(dllexport) void PR_GetDescType(void)
{
_asm jmp DWORD PTR _PR_GetDescType
}

__declspec( naked ) __declspec(dllexport) void PR_GetDirectorySeparator(void)
{
_asm jmp DWORD PTR _PR_GetDirectorySeparator
}

__declspec( naked ) __declspec(dllexport) void PR_GetDirectorySepartor(void)
{
_asm jmp DWORD PTR _PR_GetDirectorySepartor
}

__declspec( naked ) __declspec(dllexport) void PR_GetEnv(void)
{
_asm jmp DWORD PTR _PR_GetEnv
}

__declspec( naked ) __declspec(dllexport) void PR_GetError(void)
{
_asm jmp DWORD PTR _PR_GetError
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_GetErrorString(void)
{
_asm jmp DWORD PTR _PR_GetErrorString
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_GetErrorText(void)
{
_asm jmp DWORD PTR _PR_GetErrorText
}

__declspec( naked ) __declspec(dllexport) void PR_GetErrorTextLength(void)
{
_asm jmp DWORD PTR _PR_GetErrorTextLength
}

__declspec( naked ) __declspec(dllexport) void PR_GetFileInfo(void)
{
_asm jmp DWORD PTR _PR_GetFileInfo
}

__declspec( naked ) __declspec(dllexport) void PR_GetFileInfo64(void)
{
_asm jmp DWORD PTR _PR_GetFileInfo64
}

__declspec( naked ) __declspec(dllexport) void PR_GetFileMethods(void)
{
_asm jmp DWORD PTR _PR_GetFileMethods
}

__declspec( naked ) __declspec(dllexport) void PR_GetGCRegisters(void)
{
_asm jmp DWORD PTR _PR_GetGCRegisters
}

__declspec( naked ) __declspec(dllexport) void PR_GetHostByAddr(void)
{
_asm jmp DWORD PTR _PR_GetHostByAddr
}

__declspec( naked ) __declspec(dllexport) void PR_GetHostByName(void)
{
_asm jmp DWORD PTR _PR_GetHostByName
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_GetHostName(void)
{
_asm jmp DWORD PTR _PR_GetHostName
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_GetIdentitiesLayer(void)
{
_asm jmp DWORD PTR _PR_GetIdentitiesLayer
}

__declspec( naked ) __declspec(dllexport) void PR_GetLayersIdentity(void)
{
_asm jmp DWORD PTR _PR_GetLayersIdentity
}

__declspec( naked ) __declspec(dllexport) void PR_GetLibraryName(void)
{
_asm jmp DWORD PTR _PR_GetLibraryName
}

__declspec( naked ) __declspec(dllexport) void PR_GetLibraryPath(void)
{
_asm jmp DWORD PTR _PR_GetLibraryPath
}

__declspec( naked ) __declspec(dllexport) void PR_GetMonitorEntryCount(void)
{
_asm jmp DWORD PTR _PR_GetMonitorEntryCount
}

__declspec( naked ) __declspec(dllexport) void PR_GetNameForIdentity(void)
{
_asm jmp DWORD PTR _PR_GetNameForIdentity
}

__declspec( naked ) __declspec(dllexport) void PR_GetOSError(void)
{
_asm jmp DWORD PTR _PR_GetOSError
}

__declspec( naked ) __declspec(dllexport) void PR_GetOpenFileInfo(void)
{
_asm jmp DWORD PTR _PR_GetOpenFileInfo
}

__declspec( naked ) __declspec(dllexport) void PR_GetOpenFileInfo64(void)
{
_asm jmp DWORD PTR _PR_GetOpenFileInfo64
}

__declspec( naked ) __declspec(dllexport) void PR_GetPageShift(void)
{
_asm jmp DWORD PTR _PR_GetPageShift
}

__declspec( naked ) __declspec(dllexport) void PR_GetPageSize(void)
{
_asm jmp DWORD PTR _PR_GetPageSize
}

__declspec( naked ) __declspec(dllexport) void PR_GetPeerName(void)
{
_asm jmp DWORD PTR _PR_GetPeerName
}

__declspec( naked ) __declspec(dllexport) void PR_GetProtoByName(void)
{
_asm jmp DWORD PTR _PR_GetProtoByName
}

__declspec( naked ) __declspec(dllexport) void PR_GetProtoByNumber(void)
{
_asm jmp DWORD PTR _PR_GetProtoByNumber
}

__declspec( naked ) __declspec(dllexport) void PR_GetSP(void)
{
_asm jmp DWORD PTR _PR_GetSP
}

__declspec( naked ) __declspec(dllexport) void PR_GetSockName(void)
{
_asm jmp DWORD PTR _PR_GetSockName
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_GetSockOpt(void)
{
_asm jmp DWORD PTR _PR_GetSockOpt
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_GetSocketOption(void)
{
_asm jmp DWORD PTR _PR_GetSocketOption
}

__declspec( naked ) __declspec(dllexport) void PR_GetSpecialFD(void)
{
_asm jmp DWORD PTR _PR_GetSpecialFD
}

__declspec( naked ) __declspec(dllexport) void PR_GetStackSpaceLeft(void)
{
_asm jmp DWORD PTR _PR_GetStackSpaceLeft
}

__declspec( naked ) __declspec(dllexport) void PR_GetSystemInfo(void)
{
_asm jmp DWORD PTR _PR_GetSystemInfo
}

__declspec( naked ) __declspec(dllexport) void PR_GetTCPMethods(void)
{
_asm jmp DWORD PTR _PR_GetTCPMethods
}

__declspec( naked ) __declspec(dllexport) void PR_GetThreadAffinityMask(void)
{
_asm jmp DWORD PTR _PR_GetThreadAffinityMask
}

__declspec( naked ) __declspec(dllexport) void PR_GetThreadID(void)
{
_asm jmp DWORD PTR _PR_GetThreadID
}

__declspec( naked ) __declspec(dllexport) void PR_GetThreadPriority(void)
{
_asm jmp DWORD PTR _PR_GetThreadPriority
}

__declspec( naked ) __declspec(dllexport) void PR_GetThreadPrivate(void)
{
_asm jmp DWORD PTR _PR_GetThreadPrivate
}

__declspec( naked ) __declspec(dllexport) void PR_GetThreadScope(void)
{
_asm jmp DWORD PTR _PR_GetThreadScope
}

__declspec( naked ) __declspec(dllexport) void PR_GetThreadState(void)
{
_asm jmp DWORD PTR _PR_GetThreadState
}

__declspec( naked ) __declspec(dllexport) void PR_GetThreadType(void)
{
_asm jmp DWORD PTR _PR_GetThreadType
}

__declspec( naked ) __declspec(dllexport) void PR_GetTraceEntries(void)
{
_asm jmp DWORD PTR _PR_GetTraceEntries
}

__declspec( naked ) __declspec(dllexport) void PR_GetTraceHandleFromName(void)
{
_asm jmp DWORD PTR _PR_GetTraceHandleFromName
}

__declspec( naked ) __declspec(dllexport) void PR_GetTraceNameFromHandle(void)
{
_asm jmp DWORD PTR _PR_GetTraceNameFromHandle
}

__declspec( naked ) __declspec(dllexport) void PR_GetTraceOption(void)
{
_asm jmp DWORD PTR _PR_GetTraceOption
}

__declspec( naked ) __declspec(dllexport) void PR_GetUDPMethods(void)
{
_asm jmp DWORD PTR _PR_GetUDPMethods
}

__declspec( naked ) __declspec(dllexport) void PR_GetUniqueIdentity(void)
{
_asm jmp DWORD PTR _PR_GetUniqueIdentity
}

__declspec( naked ) __declspec(dllexport) void PR_GetValueSem(void)
{
_asm jmp DWORD PTR _PR_GetValueSem
}

__declspec( naked ) __declspec(dllexport) void PR_ImplodeTime(void)
{
_asm jmp DWORD PTR _PR_ImplodeTime
}

__declspec( naked ) __declspec(dllexport) void PR_ImportFile(void)
{
_asm jmp DWORD PTR _PR_ImportFile
}

__declspec( naked ) __declspec(dllexport) void PR_ImportTCPSocket(void)
{
_asm jmp DWORD PTR _PR_ImportTCPSocket
}

__declspec( naked ) __declspec(dllexport) void PR_ImportUDPSocket(void)
{
_asm jmp DWORD PTR _PR_ImportUDPSocket
}

__declspec( naked ) __declspec(dllexport) void PR_IncrementCounter(void)
{
_asm jmp DWORD PTR _PR_IncrementCounter
}

__declspec( naked ) __declspec(dllexport) void PR_Init(void)
{
_asm jmp DWORD PTR _PR_Init
}

__declspec( naked ) __declspec(dllexport) void PR_Initialize(void)
{
_asm jmp DWORD PTR _PR_Initialize
}

__declspec( naked ) __declspec(dllexport) void PR_InitializeNetAddr(void)
{
_asm jmp DWORD PTR _PR_InitializeNetAddr
}

__declspec( naked ) __declspec(dllexport) void PR_Initialized(void)
{
_asm jmp DWORD PTR _PR_Initialized
}

__declspec( naked ) __declspec(dllexport) void PR_Interrupt(void)
{
_asm jmp DWORD PTR _PR_Interrupt
}

__declspec( naked ) __declspec(dllexport) void PR_IntervalNow(void)
{
_asm jmp DWORD PTR _PR_IntervalNow
}

__declspec( naked ) __declspec(dllexport) void PR_IntervalToMicroseconds(void)
{
_asm jmp DWORD PTR _PR_IntervalToMicroseconds
}

__declspec( naked ) __declspec(dllexport) void PR_IntervalToMilliseconds(void)
{
_asm jmp DWORD PTR _PR_IntervalToMilliseconds
}

__declspec( naked ) __declspec(dllexport) void PR_IntervalToSeconds(void)
{
_asm jmp DWORD PTR _PR_IntervalToSeconds
}

__declspec( naked ) __declspec(dllexport) void PR_JoinThread(void)
{
_asm jmp DWORD PTR _PR_JoinThread
}

__declspec( naked ) __declspec(dllexport) void PR_KillProcess(void)
{
_asm jmp DWORD PTR _PR_KillProcess
}

__declspec( naked ) __declspec(dllexport) void PR_Listen(void)
{
_asm jmp DWORD PTR _PR_Listen
}

__declspec( naked ) __declspec(dllexport) void PR_LoadLibrary(void)
{
_asm jmp DWORD PTR _PR_LoadLibrary
}

__declspec( naked ) __declspec(dllexport) void PR_LoadStaticLibrary(void)
{
_asm jmp DWORD PTR _PR_LoadStaticLibrary
}

__declspec( naked ) __declspec(dllexport) void PR_LocalTimeParameters(void)
{
_asm jmp DWORD PTR _PR_LocalTimeParameters
}

__declspec( naked ) __declspec(dllexport) void PR_Lock(void)
{
_asm jmp DWORD PTR _PR_Lock
}

__declspec( naked ) __declspec(dllexport) void PR_LockFile(void)
{
_asm jmp DWORD PTR _PR_LockFile
}

__declspec( naked ) __declspec(dllexport) void PR_LockOrderedLock(void)
{
_asm jmp DWORD PTR _PR_LockOrderedLock
}

__declspec( naked ) __declspec(dllexport) void PR_LogFlush(void)
{
_asm jmp DWORD PTR _PR_LogFlush
}

__declspec( naked ) __declspec(dllexport) void PR_LogPrint(void)
{
_asm jmp DWORD PTR _PR_LogPrint
}

__declspec( naked ) __declspec(dllexport) void PR_Malloc(void)
{
_asm jmp DWORD PTR _PR_Malloc
}

__declspec( naked ) __declspec(dllexport) void PR_MemMap(void)
{
_asm jmp DWORD PTR _PR_MemMap
}

__declspec( naked ) __declspec(dllexport) void PR_MemUnmap(void)
{
_asm jmp DWORD PTR _PR_MemUnmap
}

__declspec( naked ) __declspec(dllexport) void PR_MicrosecondsToInterval(void)
{
_asm jmp DWORD PTR _PR_MicrosecondsToInterval
}

__declspec( naked ) __declspec(dllexport) void PR_MillisecondsToInterval(void)
{
_asm jmp DWORD PTR _PR_MillisecondsToInterval
}

__declspec( naked ) __declspec(dllexport) void PR_MkDir(void)
{
_asm jmp DWORD PTR _PR_MkDir
}

__declspec( naked ) __declspec(dllexport) void PR_NTFast_Accept(void)
{
_asm jmp DWORD PTR _PR_NTFast_Accept
}

__declspec( naked ) __declspec(dllexport) void PR_NTFast_AcceptRead(void)
{
_asm jmp DWORD PTR _PR_NTFast_AcceptRead
}

__declspec( naked ) __declspec(dllexport) void PR_NTFast_AcceptRead_WithTimeoutCallback(void)
{
_asm jmp DWORD PTR _PR_NTFast_AcceptRead_WithTimeoutCallback
}

__declspec( naked ) __declspec(dllexport) void PR_NTFast_UpdateAcceptContext(void)
{
_asm jmp DWORD PTR _PR_NTFast_UpdateAcceptContext
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_NT_UseNonblock(void)
{
_asm jmp DWORD PTR _PR_NT_UseNonblock
}
#endif

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_NetAddrSize(void)
{
_asm jmp DWORD PTR _PR_NetAddrSize
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_NetAddrToString(void)
{
_asm jmp DWORD PTR _PR_NetAddrToString
}

__declspec( naked ) __declspec(dllexport) void PR_NewCondVar(void)
{
_asm jmp DWORD PTR _PR_NewCondVar
}

__declspec( naked ) __declspec(dllexport) void PR_NewLock(void)
{
_asm jmp DWORD PTR _PR_NewLock
}

__declspec( naked ) __declspec(dllexport) void PR_NewLogModule(void)
{
_asm jmp DWORD PTR _PR_NewLogModule
}

__declspec( naked ) __declspec(dllexport) void PR_NewMonitor(void)
{
_asm jmp DWORD PTR _PR_NewMonitor
}

__declspec( naked ) __declspec(dllexport) void PR_NewNamedMonitor(void)
{
_asm jmp DWORD PTR _PR_NewNamedMonitor
}

__declspec( naked ) __declspec(dllexport) void PR_NewPollableEvent(void)
{
_asm jmp DWORD PTR _PR_NewPollableEvent
}

__declspec( naked ) __declspec(dllexport) void PR_NewProcessAttr(void)
{
_asm jmp DWORD PTR _PR_NewProcessAttr
}

__declspec( naked ) __declspec(dllexport) void PR_NewSem(void)
{
_asm jmp DWORD PTR _PR_NewSem
}

__declspec( naked ) __declspec(dllexport) void PR_NewTCPSocket(void)
{
_asm jmp DWORD PTR _PR_NewTCPSocket
}

__declspec( naked ) __declspec(dllexport) void PR_NewTCPSocketPair(void)
{
_asm jmp DWORD PTR _PR_NewTCPSocketPair
}

__declspec( naked ) __declspec(dllexport) void PR_NewThreadPrivateIndex(void)
{
_asm jmp DWORD PTR _PR_NewThreadPrivateIndex
}

__declspec( naked ) __declspec(dllexport) void PR_NewUDPSocket(void)
{
_asm jmp DWORD PTR _PR_NewUDPSocket
}

__declspec( naked ) __declspec(dllexport) void PR_NormalizeTime(void)
{
_asm jmp DWORD PTR _PR_NormalizeTime
}

__declspec( naked ) __declspec(dllexport) void PR_Notify(void)
{
_asm jmp DWORD PTR _PR_Notify
}

__declspec( naked ) __declspec(dllexport) void PR_NotifyAll(void)
{
_asm jmp DWORD PTR _PR_NotifyAll
}

__declspec( naked ) __declspec(dllexport) void PR_NotifyAllCondVar(void)
{
_asm jmp DWORD PTR _PR_NotifyAllCondVar
}

__declspec( naked ) __declspec(dllexport) void PR_NotifyCondVar(void)
{
_asm jmp DWORD PTR _PR_NotifyCondVar
}

__declspec( naked ) __declspec(dllexport) void PR_Now(void)
{
_asm jmp DWORD PTR _PR_Now
}

__declspec( naked ) __declspec(dllexport) void PR_Open(void)
{
_asm jmp DWORD PTR _PR_Open
}

__declspec( naked ) __declspec(dllexport) void PR_OpenDir(void)
{
_asm jmp DWORD PTR _PR_OpenDir
}

__declspec( naked ) __declspec(dllexport) void PR_ParseTimeString(void)
{
_asm jmp DWORD PTR _PR_ParseTimeString
}

__declspec( naked ) __declspec(dllexport) void PR_Poll(void)
{
_asm jmp DWORD PTR _PR_Poll
}

__declspec( naked ) __declspec(dllexport) void PR_PopIOLayer(void)
{
_asm jmp DWORD PTR _PR_PopIOLayer
}

__declspec( naked ) __declspec(dllexport) void PR_PostSem(void)
{
_asm jmp DWORD PTR _PR_PostSem
}

__declspec( naked ) __declspec(dllexport) void PR_ProcessAttrSetCurrentDirectory(void)
{
_asm jmp DWORD PTR _PR_ProcessAttrSetCurrentDirectory
}

__declspec( naked ) __declspec(dllexport) void PR_ProcessAttrSetStdioRedirect(void)
{
_asm jmp DWORD PTR _PR_ProcessAttrSetStdioRedirect
}

__declspec( naked ) __declspec(dllexport) void PR_ProcessExit(void)
{
_asm jmp DWORD PTR _PR_ProcessExit
}

__declspec( naked ) __declspec(dllexport) void PR_PushIOLayer(void)
{
_asm jmp DWORD PTR _PR_PushIOLayer
}

__declspec( naked ) __declspec(dllexport) void PR_Read(void)
{
_asm jmp DWORD PTR _PR_Read
}

__declspec( naked ) __declspec(dllexport) void PR_ReadDir(void)
{
_asm jmp DWORD PTR _PR_ReadDir
}

__declspec( naked ) __declspec(dllexport) void PR_Realloc(void)
{
_asm jmp DWORD PTR _PR_Realloc
}

__declspec( naked ) __declspec(dllexport) void PR_RecordTraceEntries(void)
{
_asm jmp DWORD PTR _PR_RecordTraceEntries
}

__declspec( naked ) __declspec(dllexport) void PR_Recv(void)
{
_asm jmp DWORD PTR _PR_Recv
}

__declspec( naked ) __declspec(dllexport) void PR_RecvFrom(void)
{
_asm jmp DWORD PTR _PR_RecvFrom
}

__declspec( naked ) __declspec(dllexport) void PR_Rename(void)
{
_asm jmp DWORD PTR _PR_Rename
}

__declspec( naked ) __declspec(dllexport) void PR_ResetAlarm(void)
{
_asm jmp DWORD PTR _PR_ResetAlarm
}

__declspec( naked ) __declspec(dllexport) void PR_ResetProcessAttr(void)
{
_asm jmp DWORD PTR _PR_ResetProcessAttr
}

__declspec( naked ) __declspec(dllexport) void PR_ResumeAll(void)
{
_asm jmp DWORD PTR _PR_ResumeAll
}

__declspec( naked ) __declspec(dllexport) void PR_RmDir(void)
{
_asm jmp DWORD PTR _PR_RmDir
}

__declspec( naked ) __declspec(dllexport) void PR_ScanStackPointers(void)
{
_asm jmp DWORD PTR _PR_ScanStackPointers
}

__declspec( naked ) __declspec(dllexport) void PR_SecondsToInterval(void)
{
_asm jmp DWORD PTR _PR_SecondsToInterval
}

__declspec( naked ) __declspec(dllexport) void PR_Seek(void)
{
_asm jmp DWORD PTR _PR_Seek
}

__declspec( naked ) __declspec(dllexport) void PR_Seek64(void)
{
_asm jmp DWORD PTR _PR_Seek64
}

__declspec( naked ) __declspec(dllexport) void PR_Select(void)
{
_asm jmp DWORD PTR _PR_Select
}

__declspec( naked ) __declspec(dllexport) void PR_Send(void)
{
_asm jmp DWORD PTR _PR_Send
}

__declspec( naked ) __declspec(dllexport) void PR_SendTo(void)
{
_asm jmp DWORD PTR _PR_SendTo
}

__declspec( naked ) __declspec(dllexport) void PR_SetAlarm(void)
{
_asm jmp DWORD PTR _PR_SetAlarm
}

__declspec( naked ) __declspec(dllexport) void PR_SetCPUAffinityMask(void)
{
_asm jmp DWORD PTR _PR_SetCPUAffinityMask
}

__declspec( naked ) __declspec(dllexport) void PR_SetConcurrency(void)
{
_asm jmp DWORD PTR _PR_SetConcurrency
}

__declspec( naked ) __declspec(dllexport) void PR_SetCounter(void)
{
_asm jmp DWORD PTR _PR_SetCounter
}

__declspec( naked ) __declspec(dllexport) void PR_SetError(void)
{
_asm jmp DWORD PTR _PR_SetError
}

__declspec( naked ) __declspec(dllexport) void PR_SetErrorText(void)
{
_asm jmp DWORD PTR _PR_SetErrorText
}

__declspec( naked ) __declspec(dllexport) void PR_SetFDCacheSize(void)
{
_asm jmp DWORD PTR _PR_SetFDCacheSize
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_SetIPv6Enable(void)
{
_asm jmp DWORD PTR _PR_SetIPv6Enable
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_SetLibraryPath(void)
{
_asm jmp DWORD PTR _PR_SetLibraryPath
}

__declspec( naked ) __declspec(dllexport) void PR_SetLogBuffering(void)
{
_asm jmp DWORD PTR _PR_SetLogBuffering
}

__declspec( naked ) __declspec(dllexport) void PR_SetLogFile(void)
{
_asm jmp DWORD PTR _PR_SetLogFile
}

__declspec( naked ) __declspec(dllexport) void PR_SetPollableEvent(void)
{
_asm jmp DWORD PTR _PR_SetPollableEvent
}

#ifndef USING_NSPR40
__declspec( naked ) __declspec(dllexport) void PR_SetSockOpt(void)
{
_asm jmp DWORD PTR _PR_SetSockOpt
}
#endif

__declspec( naked ) __declspec(dllexport) void PR_SetSocketOption(void)
{
_asm jmp DWORD PTR _PR_SetSocketOption
}

__declspec( naked ) __declspec(dllexport) void PR_SetStdioRedirect(void)
{
_asm jmp DWORD PTR _PR_SetStdioRedirect
}

__declspec( naked ) __declspec(dllexport) void PR_SetThreadAffinityMask(void)
{
_asm jmp DWORD PTR _PR_SetThreadAffinityMask
}

__declspec( naked ) __declspec(dllexport) void PR_SetThreadDumpProc(void)
{
_asm jmp DWORD PTR _PR_SetThreadDumpProc
}

__declspec( naked ) __declspec(dllexport) void PR_SetThreadGCAble(void)
{
_asm jmp DWORD PTR _PR_SetThreadGCAble
}

__declspec( naked ) __declspec(dllexport) void PR_SetThreadPriority(void)
{
_asm jmp DWORD PTR _PR_SetThreadPriority
}

__declspec( naked ) __declspec(dllexport) void PR_SetThreadPrivate(void)
{
_asm jmp DWORD PTR _PR_SetThreadPrivate
}

__declspec( naked ) __declspec(dllexport) void PR_SetThreadRecycleMode(void)
{
_asm jmp DWORD PTR _PR_SetThreadRecycleMode
}

__declspec( naked ) __declspec(dllexport) void PR_SetTraceOption(void)
{
_asm jmp DWORD PTR _PR_SetTraceOption
}

__declspec( naked ) __declspec(dllexport) void PR_ShowStatus(void)
{
_asm jmp DWORD PTR _PR_ShowStatus
}

__declspec( naked ) __declspec(dllexport) void PR_Shutdown(void)
{
_asm jmp DWORD PTR _PR_Shutdown
}

__declspec( naked ) __declspec(dllexport) void PR_Sleep(void)
{
_asm jmp DWORD PTR _PR_Sleep
}

__declspec( naked ) __declspec(dllexport) void PR_Socket(void)
{
_asm jmp DWORD PTR _PR_Socket
}

__declspec( naked ) __declspec(dllexport) void PR_StackPop(void)
{
_asm jmp DWORD PTR _PR_StackPop
}

__declspec( naked ) __declspec(dllexport) void PR_StackPush(void)
{
_asm jmp DWORD PTR _PR_StackPush
}

__declspec( naked ) __declspec(dllexport) void PR_Stat(void)
{
_asm jmp DWORD PTR _PR_Stat
}

__declspec( naked ) __declspec(dllexport) void PR_StringToNetAddr(void)
{
_asm jmp DWORD PTR _PR_StringToNetAddr
}

__declspec( naked ) __declspec(dllexport) void PR_SubtractFromCounter(void)
{
_asm jmp DWORD PTR _PR_SubtractFromCounter
}

__declspec( naked ) __declspec(dllexport) void PR_SuspendAll(void)
{
_asm jmp DWORD PTR _PR_SuspendAll
}

__declspec( naked ) __declspec(dllexport) void PR_Sync(void)
{
_asm jmp DWORD PTR _PR_Sync
}

__declspec( naked ) __declspec(dllexport) void PR_TLockFile(void)
{
_asm jmp DWORD PTR _PR_TLockFile
}

__declspec( naked ) __declspec(dllexport) void PR_TestAndEnterMonitor(void)
{
_asm jmp DWORD PTR _PR_TestAndEnterMonitor
}

__declspec( naked ) __declspec(dllexport) void PR_TestAndLock(void)
{
_asm jmp DWORD PTR _PR_TestAndLock
}

__declspec( naked ) __declspec(dllexport) void PR_ThreadScanStackPointers(void)
{
_asm jmp DWORD PTR _PR_ThreadScanStackPointers
}

__declspec( naked ) __declspec(dllexport) void PR_TicksPerSecond(void)
{
_asm jmp DWORD PTR _PR_TicksPerSecond
}

__declspec( naked ) __declspec(dllexport) void PR_Trace(void)
{
_asm jmp DWORD PTR _PR_Trace
}

__declspec( naked ) __declspec(dllexport) void PR_TransmitFile(void)
{
_asm jmp DWORD PTR _PR_TransmitFile
}

__declspec( naked ) __declspec(dllexport) void PR_USPacificTimeParameters(void)
{
_asm jmp DWORD PTR _PR_USPacificTimeParameters
}

__declspec( naked ) __declspec(dllexport) void PR_UnblockClockInterrupts(void)
{
_asm jmp DWORD PTR _PR_UnblockClockInterrupts
}

__declspec( naked ) __declspec(dllexport) void PR_UnloadLibrary(void)
{
_asm jmp DWORD PTR _PR_UnloadLibrary
}

__declspec( naked ) __declspec(dllexport) void PR_Unlock(void)
{
_asm jmp DWORD PTR _PR_Unlock
}

__declspec( naked ) __declspec(dllexport) void PR_UnlockFile(void)
{
_asm jmp DWORD PTR _PR_UnlockFile
}

__declspec( naked ) __declspec(dllexport) void PR_UnlockOrderedLock(void)
{
_asm jmp DWORD PTR _PR_UnlockOrderedLock
}

__declspec( naked ) __declspec(dllexport) void PR_VersionCheck(void)
{
_asm jmp DWORD PTR _PR_VersionCheck
}

__declspec( naked ) __declspec(dllexport) void PR_Wait(void)
{
_asm jmp DWORD PTR _PR_Wait
}

__declspec( naked ) __declspec(dllexport) void PR_WaitCondVar(void)
{
_asm jmp DWORD PTR _PR_WaitCondVar
}

__declspec( naked ) __declspec(dllexport) void PR_WaitForPollableEvent(void)
{
_asm jmp DWORD PTR _PR_WaitForPollableEvent
}

__declspec( naked ) __declspec(dllexport) void PR_WaitProcess(void)
{
_asm jmp DWORD PTR _PR_WaitProcess
}

__declspec( naked ) __declspec(dllexport) void PR_WaitRecvReady(void)
{
_asm jmp DWORD PTR _PR_WaitRecvReady
}

__declspec( naked ) __declspec(dllexport) void PR_WaitSem(void)
{
_asm jmp DWORD PTR _PR_WaitSem
}

__declspec( naked ) __declspec(dllexport) void PR_Write(void)
{
_asm jmp DWORD PTR _PR_Write
}

__declspec( naked ) __declspec(dllexport) void PR_Writev(void)
{
_asm jmp DWORD PTR _PR_Writev
}

__declspec( naked ) __declspec(dllexport) void PR_Yield(void)
{
_asm jmp DWORD PTR _PR_Yield
}

__declspec( naked ) __declspec(dllexport) void PR_cnvtf(void)
{
_asm jmp DWORD PTR _PR_cnvtf
}

__declspec( naked ) __declspec(dllexport) void PR_dtoa(void)
{
_asm jmp DWORD PTR _PR_dtoa
}

__declspec( naked ) __declspec(dllexport) void PR_fprintf(void)
{
_asm jmp DWORD PTR _PR_fprintf
}

__declspec( naked ) __declspec(dllexport) void PR_htonl(void)
{
_asm jmp DWORD PTR _PR_htonl
}

__declspec( naked ) __declspec(dllexport) void PR_htonll(void)
{
_asm jmp DWORD PTR _PR_htonll
}

__declspec( naked ) __declspec(dllexport) void PR_htons(void)
{
_asm jmp DWORD PTR _PR_htons
}

__declspec( naked ) __declspec(dllexport) void PR_ntohl(void)
{
_asm jmp DWORD PTR _PR_ntohl
}

__declspec( naked ) __declspec(dllexport) void PR_ntohll(void)
{
_asm jmp DWORD PTR _PR_ntohll
}

__declspec( naked ) __declspec(dllexport) void PR_ntohs(void)
{
_asm jmp DWORD PTR _PR_ntohs
}

__declspec( naked ) __declspec(dllexport) void PR_smprintf(void)
{
_asm jmp DWORD PTR _PR_smprintf
}

__declspec( naked ) __declspec(dllexport) void PR_smprintf_free(void)
{
_asm jmp DWORD PTR _PR_smprintf_free
}

__declspec( naked ) __declspec(dllexport) void PR_snprintf(void)
{
_asm jmp DWORD PTR _PR_snprintf
}

__declspec( naked ) __declspec(dllexport) void PR_sprintf_append(void)
{
_asm jmp DWORD PTR _PR_sprintf_append
}

__declspec( naked ) __declspec(dllexport) void PR_sscanf(void)
{
_asm jmp DWORD PTR _PR_sscanf
}

__declspec( naked ) __declspec(dllexport) void PR_strtod(void)
{
_asm jmp DWORD PTR _PR_strtod
}

__declspec( naked ) __declspec(dllexport) void PR_sxprintf(void)
{
_asm jmp DWORD PTR _PR_sxprintf
}

__declspec( naked ) __declspec(dllexport) void PR_vfprintf(void)
{
_asm jmp DWORD PTR _PR_vfprintf
}

__declspec( naked ) __declspec(dllexport) void PR_vsmprintf(void)
{
_asm jmp DWORD PTR _PR_vsmprintf
}

__declspec( naked ) __declspec(dllexport) void PR_vsnprintf(void)
{
_asm jmp DWORD PTR _PR_vsnprintf
}

__declspec( naked ) __declspec(dllexport) void PR_vsprintf_append(void)
{
_asm jmp DWORD PTR _PR_vsprintf_append
}

__declspec( naked ) __declspec(dllexport) void PR_vsxprintf(void)
{
_asm jmp DWORD PTR _PR_vsxprintf
}

__declspec( naked ) __declspec(dllexport) void SetExecutionEnvironment(void)
{
_asm jmp DWORD PTR _SetExecutionEnvironment
}

__declspec( naked ) __declspec(dllexport) void _PR_AddSleepQ(void)
{
_asm jmp DWORD PTR __PR_AddSleepQ
}

__declspec( naked ) __declspec(dllexport) void _PR_CreateThread(void)
{
_asm jmp DWORD PTR ___PR_CreateThread
}

__declspec( naked ) __declspec(dllexport) void _PR_DelSleepQ(void)
{
_asm jmp DWORD PTR __PR_DelSleepQ
}

__declspec( naked ) __declspec(dllexport) void _PR_GetPrimordialCPU(void)
{
_asm jmp DWORD PTR __PR_GetPrimordialCPU
}

__declspec( naked ) __declspec(dllexport) void _PR_NativeCreateThread(void)
{
_asm jmp DWORD PTR __PR_NativeCreateThread
}

__declspec( naked ) __declspec(dllexport) void libVersionPoint(void)
{
_asm jmp DWORD PTR _libVersionPoint
}

} // Extern C
int load_nspr_table()
{
    if (!(hDll = GetModuleHandle(UP_MODULE_NAME)))
        hDll = LoadLibrary(UP_MODULE_NAME);
	if (hDll){
        _GetExecutionEnvironment = (void (*)())GetProcAddress(hDll, "GetExecutionEnvironment");
        _LL_MaxInt = GetProcAddress(hDll, "LL_MaxInt");
        _LL_MinInt = GetProcAddress(hDll, "LL_MinInt");
        _LL_Zero = GetProcAddress(hDll, "LL_Zero");
        _PRP_DestroyNakedCondVar = GetProcAddress(hDll, "PRP_DestroyNakedCondVar");
        _PRP_NakedBroadcast = GetProcAddress(hDll, "PRP_NakedBroadcast");
        _PRP_NakedNotify = GetProcAddress(hDll, "PRP_NakedNotify");
        _PRP_NakedWait = GetProcAddress(hDll, "PRP_NakedWait");
        _PRP_NewNakedCondVar = GetProcAddress(hDll, "PRP_NewNakedCondVar");
        _PRP_TryLock = GetProcAddress(hDll, "PRP_TryLock");
        _PR_Abort = GetProcAddress(hDll, "PR_Abort");
        _PR_Accept = GetProcAddress(hDll, "PR_Accept");
        _PR_AcceptRead = GetProcAddress(hDll, "PR_AcceptRead");
        _PR_Access = GetProcAddress(hDll, "PR_Access");
        _PR_AddToCounter = GetProcAddress(hDll, "PR_AddToCounter");
        _PR_AddWaitFileDesc = GetProcAddress(hDll, "PR_AddWaitFileDesc");
        _PR_AllocFileDesc = GetProcAddress(hDll, "PR_AllocFileDesc");
        _PR_Assert = GetProcAddress(hDll, "PR_Assert");
        _PR_AtomicAdd = GetProcAddress(hDll, "PR_AtomicAdd");
        _PR_AtomicDecrement = GetProcAddress(hDll, "PR_AtomicDecrement");
        _PR_AtomicIncrement = GetProcAddress(hDll, "PR_AtomicIncrement");
        _PR_AtomicSet = GetProcAddress(hDll, "PR_AtomicSet");
        _PR_AttachThread = GetProcAddress(hDll, "PR_AttachThread");
        _PR_AttachThreadGCAble = GetProcAddress(hDll, "PR_AttachThreadGCAble");
        _PR_Available = GetProcAddress(hDll, "PR_Available");
        _PR_Available64 = GetProcAddress(hDll, "PR_Available64");
        _PR_Bind = GetProcAddress(hDll, "PR_Bind");
        _PR_BlockClockInterrupts = GetProcAddress(hDll, "PR_BlockClockInterrupts");
        _PR_CEnterMonitor = GetProcAddress(hDll, "PR_CEnterMonitor");
        _PR_CExitMonitor = GetProcAddress(hDll, "PR_CExitMonitor");
        _PR_CNotify = GetProcAddress(hDll, "PR_CNotify");
        _PR_CNotifyAll = GetProcAddress(hDll, "PR_CNotifyAll");
        _PR_CWait = GetProcAddress(hDll, "PR_CWait");
        _PR_CallOnce = GetProcAddress(hDll, "PR_CallOnce");
        _PR_Calloc = GetProcAddress(hDll, "PR_Calloc");
        _PR_CancelWaitFileDesc = GetProcAddress(hDll, "PR_CancelWaitFileDesc");
        _PR_CancelWaitGroup = GetProcAddress(hDll, "PR_CancelWaitGroup");
        _PR_CeilingLog2 = GetProcAddress(hDll, "PR_CeilingLog2");
        _PR_ChangeFileDescNativeHandle = GetProcAddress(hDll, "PR_ChangeFileDescNativeHandle");
        _PR_Cleanup = GetProcAddress(hDll, "PR_Cleanup");
        _PR_ClearInterrupt = GetProcAddress(hDll, "PR_ClearInterrupt");
        _PR_ClearThreadGCAble = GetProcAddress(hDll, "PR_ClearThreadGCAble");
        _PR_Close = GetProcAddress(hDll, "PR_Close");
        _PR_CloseDir = GetProcAddress(hDll, "PR_CloseDir");
        _PR_CloseFileMap = GetProcAddress(hDll, "PR_CloseFileMap");
        _PR_Connect = GetProcAddress(hDll, "PR_Connect");
        _PR_CreateAlarm = GetProcAddress(hDll, "PR_CreateAlarm");
        _PR_CreateCounter = GetProcAddress(hDll, "PR_CreateCounter");
        _PR_CreateFileMap = GetProcAddress(hDll, "PR_CreateFileMap");
        _PR_CreateIOLayerStub = GetProcAddress(hDll, "PR_CreateIOLayerStub");
        _PR_CreateMWaitEnumerator = GetProcAddress(hDll, "PR_CreateMWaitEnumerator");
#ifndef USING_NSPR40
        _PR_CreateNetAddr = GetProcAddress(hDll, "PR_CreateNetAddr");
#endif
        _PR_CreateOrderedLock = GetProcAddress(hDll, "PR_CreateOrderedLock");
        _PR_CreatePipe = GetProcAddress(hDll, "PR_CreatePipe");
        _PR_CreateProcess = GetProcAddress(hDll, "PR_CreateProcess");
        _PR_CreateProcessDetached = GetProcAddress(hDll, "PR_CreateProcessDetached");
        _PR_CreateStack = GetProcAddress(hDll, "PR_CreateStack");
        __PR_CreateThread = GetProcAddress(hDll, "PR_CreateThread");
        _PR_CreateThreadGCAble = GetProcAddress(hDll, "PR_CreateThreadGCAble");
        _PR_CreateTrace = GetProcAddress(hDll, "PR_CreateTrace");
        _PR_CreateWaitGroup = GetProcAddress(hDll, "PR_CreateWaitGroup");
        _PR_DecrementCounter = GetProcAddress(hDll, "PR_DecrementCounter");
        _PR_Delete = GetProcAddress(hDll, "PR_Delete");
        _PR_DestroyAlarm = GetProcAddress(hDll, "PR_DestroyAlarm");
        _PR_DestroyCondVar = GetProcAddress(hDll, "PR_DestroyCondVar");
        _PR_DestroyCounter = GetProcAddress(hDll, "PR_DestroyCounter");
        _PR_DestroyLock = GetProcAddress(hDll, "PR_DestroyLock");
        _PR_DestroyMWaitEnumerator = GetProcAddress(hDll, "PR_DestroyMWaitEnumerator");
        _PR_DestroyMonitor = GetProcAddress(hDll, "PR_DestroyMonitor");
#ifndef USING_NSPR40
        _PR_DestroyNetAddr = GetProcAddress(hDll, "PR_DestroyNetAddr");
#endif
        _PR_DestroyOrderedLock = GetProcAddress(hDll, "PR_DestroyOrderedLock");
        _PR_DestroyPollableEvent = GetProcAddress(hDll, "PR_DestroyPollableEvent");
        _PR_DestroyProcessAttr = GetProcAddress(hDll, "PR_DestroyProcessAttr");
        _PR_DestroySem = GetProcAddress(hDll, "PR_DestroySem");
        _PR_DestroyStack = GetProcAddress(hDll, "PR_DestroyStack");
        _PR_DestroyTrace = GetProcAddress(hDll, "PR_DestroyTrace");
        _PR_DestroyWaitGroup = GetProcAddress(hDll, "PR_DestroyWaitGroup");
        _PR_DetachProcess = GetProcAddress(hDll, "PR_DetachProcess");
        _PR_DetachThread = GetProcAddress(hDll, "PR_DetachThread");
        _PR_DisableClockInterrupts = GetProcAddress(hDll, "PR_DisableClockInterrupts");
        _PR_EnableClockInterrupts = GetProcAddress(hDll, "PR_EnableClockInterrupts");
        _PR_EnterMonitor = GetProcAddress(hDll, "PR_EnterMonitor");
        _PR_EnumerateHostEnt = GetProcAddress(hDll, "PR_EnumerateHostEnt");
        _PR_EnumerateThreads = GetProcAddress(hDll, "PR_EnumerateThreads");
        _PR_EnumerateWaitGroup = GetProcAddress(hDll, "PR_EnumerateWaitGroup");
        _PR_ExitMonitor = GetProcAddress(hDll, "PR_ExitMonitor");
        _PR_ExplodeTime = GetProcAddress(hDll, "PR_ExplodeTime");
        _PR_FD_CLR = GetProcAddress(hDll, "PR_FD_CLR");
        _PR_FD_ISSET = GetProcAddress(hDll, "PR_FD_ISSET");
        _PR_FD_NCLR = GetProcAddress(hDll, "PR_FD_NCLR");
        _PR_FD_NISSET = GetProcAddress(hDll, "PR_FD_NISSET");
        _PR_FD_NSET = GetProcAddress(hDll, "PR_FD_NSET");
        _PR_FD_SET = GetProcAddress(hDll, "PR_FD_SET");
        _PR_FD_ZERO = GetProcAddress(hDll, "PR_FD_ZERO");
#ifndef USING_NSPR40
        _PR_FamilyInet = GetProcAddress(hDll, "PR_FamilyInet");
#endif
        _PR_FileDesc2NativeHandle = GetProcAddress(hDll, "PR_FileDesc2NativeHandle");
        _PR_FindLibrary = GetProcAddress(hDll, "PR_FindLibrary");
        _PR_FindNextCounterQname = GetProcAddress(hDll, "PR_FindNextCounterQname");
        _PR_FindNextCounterRname = GetProcAddress(hDll, "PR_FindNextCounterRname");
        _PR_FindNextTraceQname = GetProcAddress(hDll, "PR_FindNextTraceQname");
        _PR_FindNextTraceRname = GetProcAddress(hDll, "PR_FindNextTraceRname");
        _PR_FindSymbol = GetProcAddress(hDll, "PR_FindSymbol");
        _PR_FindSymbolAndLibrary = GetProcAddress(hDll, "PR_FindSymbolAndLibrary");
        _PR_FloorLog2 = GetProcAddress(hDll, "PR_FloorLog2");
        _PR_FormatTime = GetProcAddress(hDll, "PR_FormatTime");
        _PR_FormatTimeUSEnglish = GetProcAddress(hDll, "PR_FormatTimeUSEnglish");
        _PR_Free = GetProcAddress(hDll, "PR_Free");
        _PR_FreeFileDesc = GetProcAddress(hDll, "PR_FreeFileDesc");
        _PR_FreeLibraryName = GetProcAddress(hDll, "PR_FreeLibraryName");
        _PR_GMTParameters = GetProcAddress(hDll, "PR_GMTParameters");
        _PR_GetConnectStatus = GetProcAddress(hDll, "PR_GetConnectStatus");
        _PR_GetCounter = GetProcAddress(hDll, "PR_GetCounter");
        _PR_GetCounterHandleFromName = GetProcAddress(hDll, "PR_GetCounterHandleFromName");
        _PR_GetCounterNameFromHandle = GetProcAddress(hDll, "PR_GetCounterNameFromHandle");
        _PR_GetCurrentThread = GetProcAddress(hDll, "PR_GetCurrentThread");
        _PR_GetDefaultIOMethods = GetProcAddress(hDll, "PR_GetDefaultIOMethods");
        _PR_GetDescType = GetProcAddress(hDll, "PR_GetDescType");
        _PR_GetDirectorySeparator = GetProcAddress(hDll, "PR_GetDirectorySeparator");
        _PR_GetDirectorySepartor = GetProcAddress(hDll, "PR_GetDirectorySepartor");
        _PR_GetEnv = GetProcAddress(hDll, "PR_GetEnv");
        _PR_GetError = GetProcAddress(hDll, "PR_GetError");
#ifndef USING_NSPR40
        _PR_GetErrorString = GetProcAddress(hDll, "PR_GetErrorString");
#endif
        _PR_GetErrorText = GetProcAddress(hDll, "PR_GetErrorText");
        _PR_GetErrorTextLength = GetProcAddress(hDll, "PR_GetErrorTextLength");
        _PR_GetFileInfo = GetProcAddress(hDll, "PR_GetFileInfo");
        _PR_GetFileInfo64 = GetProcAddress(hDll, "PR_GetFileInfo64");
        _PR_GetFileMethods = GetProcAddress(hDll, "PR_GetFileMethods");
        _PR_GetGCRegisters = GetProcAddress(hDll, "PR_GetGCRegisters");
        _PR_GetHostByAddr = GetProcAddress(hDll, "PR_GetHostByAddr");
        _PR_GetHostByName = GetProcAddress(hDll, "PR_GetHostByName");
#ifndef USING_NSPR40
        _PR_GetHostName = GetProcAddress(hDll, "PR_GetHostName");
#endif
        _PR_GetIdentitiesLayer = GetProcAddress(hDll, "PR_GetIdentitiesLayer");
        _PR_GetLayersIdentity = GetProcAddress(hDll, "PR_GetLayersIdentity");
        _PR_GetLibraryName = GetProcAddress(hDll, "PR_GetLibraryName");
        _PR_GetLibraryPath = GetProcAddress(hDll, "PR_GetLibraryPath");
        _PR_GetMonitorEntryCount = GetProcAddress(hDll, "PR_GetMonitorEntryCount");
        _PR_GetNameForIdentity = GetProcAddress(hDll, "PR_GetNameForIdentity");
        _PR_GetOSError = GetProcAddress(hDll, "PR_GetOSError");
        _PR_GetOpenFileInfo = GetProcAddress(hDll, "PR_GetOpenFileInfo");
        _PR_GetOpenFileInfo64 = GetProcAddress(hDll, "PR_GetOpenFileInfo64");
        _PR_GetPageShift = GetProcAddress(hDll, "PR_GetPageShift");
        _PR_GetPageSize = GetProcAddress(hDll, "PR_GetPageSize");
        _PR_GetPeerName = GetProcAddress(hDll, "PR_GetPeerName");
        _PR_GetProtoByName = GetProcAddress(hDll, "PR_GetProtoByName");
        _PR_GetProtoByNumber = GetProcAddress(hDll, "PR_GetProtoByNumber");
        _PR_GetSP = GetProcAddress(hDll, "PR_GetSP");
        _PR_GetSockName = GetProcAddress(hDll, "PR_GetSockName");
#ifndef USING_NSPR40
        _PR_GetSockOpt = GetProcAddress(hDll, "PR_GetSockOpt");
#endif
        _PR_GetSocketOption = GetProcAddress(hDll, "PR_GetSocketOption");
        _PR_GetSpecialFD = GetProcAddress(hDll, "PR_GetSpecialFD");
        _PR_GetStackSpaceLeft = GetProcAddress(hDll, "PR_GetStackSpaceLeft");
        _PR_GetSystemInfo = GetProcAddress(hDll, "PR_GetSystemInfo");
        _PR_GetTCPMethods = GetProcAddress(hDll, "PR_GetTCPMethods");
        _PR_GetThreadAffinityMask = GetProcAddress(hDll, "PR_GetThreadAffinityMask");
        _PR_GetThreadID = GetProcAddress(hDll, "PR_GetThreadID");
        _PR_GetThreadPriority = GetProcAddress(hDll, "PR_GetThreadPriority");
        _PR_GetThreadPrivate = GetProcAddress(hDll, "PR_GetThreadPrivate");
        _PR_GetThreadScope = GetProcAddress(hDll, "PR_GetThreadScope");
        _PR_GetThreadState = GetProcAddress(hDll, "PR_GetThreadState");
        _PR_GetThreadType = GetProcAddress(hDll, "PR_GetThreadType");
        _PR_GetTraceEntries = GetProcAddress(hDll, "PR_GetTraceEntries");
        _PR_GetTraceHandleFromName = GetProcAddress(hDll, "PR_GetTraceHandleFromName");
        _PR_GetTraceNameFromHandle = GetProcAddress(hDll, "PR_GetTraceNameFromHandle");
        _PR_GetTraceOption = GetProcAddress(hDll, "PR_GetTraceOption");
        _PR_GetUDPMethods = GetProcAddress(hDll, "PR_GetUDPMethods");
        _PR_GetUniqueIdentity = GetProcAddress(hDll, "PR_GetUniqueIdentity");
        _PR_GetValueSem = GetProcAddress(hDll, "PR_GetValueSem");
        _PR_ImplodeTime = GetProcAddress(hDll, "PR_ImplodeTime");
        _PR_ImportFile = GetProcAddress(hDll, "PR_ImportFile");
        _PR_ImportTCPSocket = GetProcAddress(hDll, "PR_ImportTCPSocket");
        _PR_ImportUDPSocket = GetProcAddress(hDll, "PR_ImportUDPSocket");
        _PR_IncrementCounter = GetProcAddress(hDll, "PR_IncrementCounter");
        _PR_Init = GetProcAddress(hDll, "PR_Init");
        _PR_Initialize = GetProcAddress(hDll, "PR_Initialize");
        _PR_InitializeNetAddr = GetProcAddress(hDll, "PR_InitializeNetAddr");
        _PR_Initialized = GetProcAddress(hDll, "PR_Initialized");
        _PR_Interrupt = GetProcAddress(hDll, "PR_Interrupt");
        _PR_IntervalNow = GetProcAddress(hDll, "PR_IntervalNow");
        _PR_IntervalToMicroseconds = GetProcAddress(hDll, "PR_IntervalToMicroseconds");
        _PR_IntervalToMilliseconds = GetProcAddress(hDll, "PR_IntervalToMilliseconds");
        _PR_IntervalToSeconds = GetProcAddress(hDll, "PR_IntervalToSeconds");
        _PR_JoinThread = GetProcAddress(hDll, "PR_JoinThread");
        _PR_KillProcess = GetProcAddress(hDll, "PR_KillProcess");
        _PR_Listen = GetProcAddress(hDll, "PR_Listen");
        _PR_LoadLibrary = GetProcAddress(hDll, "PR_LoadLibrary");
        _PR_LoadStaticLibrary = GetProcAddress(hDll, "PR_LoadStaticLibrary");
        _PR_LocalTimeParameters = GetProcAddress(hDll, "PR_LocalTimeParameters");
        _PR_Lock = GetProcAddress(hDll, "PR_Lock");
        _PR_LockFile = GetProcAddress(hDll, "PR_LockFile");
        _PR_LockOrderedLock = GetProcAddress(hDll, "PR_LockOrderedLock");
        _PR_LogFlush = GetProcAddress(hDll, "PR_LogFlush");
        _PR_LogPrint = GetProcAddress(hDll, "PR_LogPrint");
        _PR_Malloc = GetProcAddress(hDll, "PR_Malloc");
        _PR_MemMap = GetProcAddress(hDll, "PR_MemMap");
        _PR_MemUnmap = GetProcAddress(hDll, "PR_MemUnmap");
        _PR_MicrosecondsToInterval = GetProcAddress(hDll, "PR_MicrosecondsToInterval");
        _PR_MillisecondsToInterval = GetProcAddress(hDll, "PR_MillisecondsToInterval");
        _PR_MkDir = GetProcAddress(hDll, "PR_MkDir");
        _PR_NTFast_Accept = GetProcAddress(hDll, "PR_NTFast_Accept");
        _PR_NTFast_AcceptRead = GetProcAddress(hDll, "PR_NTFast_AcceptRead");
        _PR_NTFast_AcceptRead_WithTimeoutCallback = GetProcAddress(hDll, "PR_NTFast_AcceptRead_WithTimeoutCallback");
        _PR_NTFast_UpdateAcceptContext = GetProcAddress(hDll, "PR_NTFast_UpdateAcceptContext");
#ifndef USING_NSPR40
        _PR_NT_UseNonblock = GetProcAddress(hDll, "PR_NT_UseNonblock");
        _PR_NetAddrSize = GetProcAddress(hDll, "PR_NetAddrSize");
#endif
        _PR_NetAddrToString = GetProcAddress(hDll, "PR_NetAddrToString");
        _PR_NewCondVar = GetProcAddress(hDll, "PR_NewCondVar");
        _PR_NewLock = GetProcAddress(hDll, "PR_NewLock");
        _PR_NewLogModule = GetProcAddress(hDll, "PR_NewLogModule");
        _PR_NewMonitor = GetProcAddress(hDll, "PR_NewMonitor");
        _PR_NewNamedMonitor = GetProcAddress(hDll, "PR_NewNamedMonitor");
        _PR_NewPollableEvent = GetProcAddress(hDll, "PR_NewPollableEvent");
        _PR_NewProcessAttr = GetProcAddress(hDll, "PR_NewProcessAttr");
        _PR_NewSem = GetProcAddress(hDll, "PR_NewSem");
        _PR_NewTCPSocket = GetProcAddress(hDll, "PR_NewTCPSocket");
        _PR_NewTCPSocketPair = GetProcAddress(hDll, "PR_NewTCPSocketPair");
        _PR_NewThreadPrivateIndex = GetProcAddress(hDll, "PR_NewThreadPrivateIndex");
        _PR_NewUDPSocket = GetProcAddress(hDll, "PR_NewUDPSocket");
        _PR_NormalizeTime = GetProcAddress(hDll, "PR_NormalizeTime");
        _PR_Notify = GetProcAddress(hDll, "PR_Notify");
        _PR_NotifyAll = GetProcAddress(hDll, "PR_NotifyAll");
        _PR_NotifyAllCondVar = GetProcAddress(hDll, "PR_NotifyAllCondVar");
        _PR_NotifyCondVar = GetProcAddress(hDll, "PR_NotifyCondVar");
        _PR_Now = GetProcAddress(hDll, "PR_Now");
        _PR_Open = GetProcAddress(hDll, "PR_Open");
        _PR_OpenDir = GetProcAddress(hDll, "PR_OpenDir");
        _PR_ParseTimeString = GetProcAddress(hDll, "PR_ParseTimeString");
        _PR_Poll = GetProcAddress(hDll, "PR_Poll");
        _PR_PopIOLayer = GetProcAddress(hDll, "PR_PopIOLayer");
        _PR_PostSem = GetProcAddress(hDll, "PR_PostSem");
        _PR_ProcessAttrSetCurrentDirectory = GetProcAddress(hDll, "PR_ProcessAttrSetCurrentDirectory");
        _PR_ProcessAttrSetStdioRedirect = GetProcAddress(hDll, "PR_ProcessAttrSetStdioRedirect");
        _PR_ProcessExit = GetProcAddress(hDll, "PR_ProcessExit");
        _PR_PushIOLayer = GetProcAddress(hDll, "PR_PushIOLayer");
        _PR_Read = GetProcAddress(hDll, "PR_Read");
        _PR_ReadDir = GetProcAddress(hDll, "PR_ReadDir");
        _PR_Realloc = GetProcAddress(hDll, "PR_Realloc");
        _PR_RecordTraceEntries = GetProcAddress(hDll, "PR_RecordTraceEntries");
        _PR_Recv = GetProcAddress(hDll, "PR_Recv");
        _PR_RecvFrom = GetProcAddress(hDll, "PR_RecvFrom");
        _PR_Rename = GetProcAddress(hDll, "PR_Rename");
        _PR_ResetAlarm = GetProcAddress(hDll, "PR_ResetAlarm");
        _PR_ResetProcessAttr = GetProcAddress(hDll, "PR_ResetProcessAttr");
        _PR_ResumeAll = GetProcAddress(hDll, "PR_ResumeAll");
        _PR_RmDir = GetProcAddress(hDll, "PR_RmDir");
        _PR_ScanStackPointers = GetProcAddress(hDll, "PR_ScanStackPointers");
        _PR_SecondsToInterval = GetProcAddress(hDll, "PR_SecondsToInterval");
        _PR_Seek = GetProcAddress(hDll, "PR_Seek");
        _PR_Seek64 = GetProcAddress(hDll, "PR_Seek64");
        _PR_Select = GetProcAddress(hDll, "PR_Select");
        _PR_Send = GetProcAddress(hDll, "PR_Send");
        _PR_SendTo = GetProcAddress(hDll, "PR_SendTo");
        _PR_SetAlarm = GetProcAddress(hDll, "PR_SetAlarm");
        _PR_SetCPUAffinityMask = GetProcAddress(hDll, "PR_SetCPUAffinityMask");
        _PR_SetConcurrency = GetProcAddress(hDll, "PR_SetConcurrency");
        _PR_SetCounter = GetProcAddress(hDll, "PR_SetCounter");
        _PR_SetError = GetProcAddress(hDll, "PR_SetError");
        _PR_SetErrorText = GetProcAddress(hDll, "PR_SetErrorText");
        _PR_SetFDCacheSize = GetProcAddress(hDll, "PR_SetFDCacheSize");
#ifndef USING_NSPR40
        _PR_SetIPv6Enable = GetProcAddress(hDll, "PR_SetIPv6Enable");
#endif
        _PR_SetLibraryPath = GetProcAddress(hDll, "PR_SetLibraryPath");
        _PR_SetLogBuffering = GetProcAddress(hDll, "PR_SetLogBuffering");
        _PR_SetLogFile = GetProcAddress(hDll, "PR_SetLogFile");
        _PR_SetPollableEvent = GetProcAddress(hDll, "PR_SetPollableEvent");
#ifndef USING_NSPR40
        _PR_SetSockOpt = GetProcAddress(hDll, "PR_SetSockOpt");
#endif
        _PR_SetSocketOption = GetProcAddress(hDll, "PR_SetSocketOption");
        _PR_SetStdioRedirect = GetProcAddress(hDll, "PR_SetStdioRedirect");
        _PR_SetThreadAffinityMask = GetProcAddress(hDll, "PR_SetThreadAffinityMask");
        _PR_SetThreadDumpProc = GetProcAddress(hDll, "PR_SetThreadDumpProc");
        _PR_SetThreadGCAble = GetProcAddress(hDll, "PR_SetThreadGCAble");
        _PR_SetThreadPriority = GetProcAddress(hDll, "PR_SetThreadPriority");
        _PR_SetThreadPrivate = GetProcAddress(hDll, "PR_SetThreadPrivate");
        _PR_SetThreadRecycleMode = GetProcAddress(hDll, "PR_SetThreadRecycleMode");
        _PR_SetTraceOption = GetProcAddress(hDll, "PR_SetTraceOption");
        _PR_ShowStatus = GetProcAddress(hDll, "PR_ShowStatus");
        _PR_Shutdown = GetProcAddress(hDll, "PR_Shutdown");
        _PR_Sleep = GetProcAddress(hDll, "PR_Sleep");
        _PR_Socket = GetProcAddress(hDll, "PR_Socket");
        _PR_StackPop = GetProcAddress(hDll, "PR_StackPop");
        _PR_StackPush = GetProcAddress(hDll, "PR_StackPush");
        _PR_Stat = GetProcAddress(hDll, "PR_Stat");
        _PR_StringToNetAddr = GetProcAddress(hDll, "PR_StringToNetAddr");
        _PR_SubtractFromCounter = GetProcAddress(hDll, "PR_SubtractFromCounter");
        _PR_SuspendAll = GetProcAddress(hDll, "PR_SuspendAll");
        _PR_Sync = GetProcAddress(hDll, "PR_Sync");
        _PR_TLockFile = GetProcAddress(hDll, "PR_TLockFile");
        _PR_TestAndEnterMonitor = GetProcAddress(hDll, "PR_TestAndEnterMonitor");
        _PR_TestAndLock = GetProcAddress(hDll, "PR_TestAndLock");
        _PR_ThreadScanStackPointers = GetProcAddress(hDll, "PR_ThreadScanStackPointers");
        _PR_TicksPerSecond = GetProcAddress(hDll, "PR_TicksPerSecond");
        _PR_Trace = GetProcAddress(hDll, "PR_Trace");
        _PR_TransmitFile = GetProcAddress(hDll, "PR_TransmitFile");
        _PR_USPacificTimeParameters = GetProcAddress(hDll, "PR_USPacificTimeParameters");
        _PR_UnblockClockInterrupts = GetProcAddress(hDll, "PR_UnblockClockInterrupts");
        _PR_UnloadLibrary = GetProcAddress(hDll, "PR_UnloadLibrary");
        _PR_Unlock = GetProcAddress(hDll, "PR_Unlock");
        _PR_UnlockFile = GetProcAddress(hDll, "PR_UnlockFile");
        _PR_UnlockOrderedLock = GetProcAddress(hDll, "PR_UnlockOrderedLock");
        _PR_VersionCheck = GetProcAddress(hDll, "PR_VersionCheck");
        _PR_Wait = GetProcAddress(hDll, "PR_Wait");
        _PR_WaitCondVar = GetProcAddress(hDll, "PR_WaitCondVar");
        _PR_WaitForPollableEvent = GetProcAddress(hDll, "PR_WaitForPollableEvent");
        _PR_WaitProcess = GetProcAddress(hDll, "PR_WaitProcess");
        _PR_WaitRecvReady = GetProcAddress(hDll, "PR_WaitRecvReady");
        _PR_WaitSem = GetProcAddress(hDll, "PR_WaitSem");
        _PR_Write = GetProcAddress(hDll, "PR_Write");
        _PR_Writev = GetProcAddress(hDll, "PR_Writev");
        _PR_Yield = GetProcAddress(hDll, "PR_Yield");
        _PR_cnvtf = GetProcAddress(hDll, "PR_cnvtf");
        _PR_dtoa = GetProcAddress(hDll, "PR_dtoa");
        _PR_fprintf = GetProcAddress(hDll, "PR_fprintf");
        _PR_htonl = GetProcAddress(hDll, "PR_htonl");
        _PR_htonll = GetProcAddress(hDll, "PR_htonll");
        _PR_htons = GetProcAddress(hDll, "PR_htons");
        _PR_ntohl = GetProcAddress(hDll, "PR_ntohl");
        _PR_ntohll = GetProcAddress(hDll, "PR_ntohll");
        _PR_ntohs = GetProcAddress(hDll, "PR_ntohs");
        _PR_smprintf = GetProcAddress(hDll, "PR_smprintf");
        _PR_smprintf_free = GetProcAddress(hDll, "PR_smprintf_free");
        _PR_snprintf = GetProcAddress(hDll, "PR_snprintf");
        _PR_sprintf_append = GetProcAddress(hDll, "PR_sprintf_append");
        _PR_sscanf = GetProcAddress(hDll, "PR_sscanf");
        _PR_strtod = GetProcAddress(hDll, "PR_strtod");
        _PR_sxprintf = GetProcAddress(hDll, "PR_sxprintf");
        _PR_vfprintf = GetProcAddress(hDll, "PR_vfprintf");
        _PR_vsmprintf = GetProcAddress(hDll, "PR_vsmprintf");
        _PR_vsnprintf = GetProcAddress(hDll, "PR_vsnprintf");
        _PR_vsprintf_append = GetProcAddress(hDll, "PR_vsprintf_append");
        _PR_vsxprintf = GetProcAddress(hDll, "PR_vsxprintf");
        _SetExecutionEnvironment = GetProcAddress(hDll, "SetExecutionEnvironment");
        __PR_AddSleepQ = GetProcAddress(hDll, "_PR_AddSleepQ");
        ___PR_CreateThread = GetProcAddress(hDll, "_PR_CreateThread");
        __PR_DelSleepQ = GetProcAddress(hDll, "_PR_DelSleepQ");
        __PR_GetPrimordialCPU = GetProcAddress(hDll, "_PR_GetPrimordialCPU");
        __PR_NativeCreateThread = GetProcAddress(hDll, "_PR_NativeCreateThread");
        _libVersionPoint = GetProcAddress(hDll, "libVersionPoint");
	}else{
		DWORD err;
		err = GetLastError();
		//char buff[1024];
		//sprintf(buff, "Error opening %s.  Error=%d", UP_MODULE_NAME, err);
		return 1;
	}
	return 0;
}

BOOL APIENTRY DllMain( HANDLE hModule, 
DWORD ul_reason_for_call, 
LPVOID lpReserved )
{
    switch( ul_reason_for_call ) {
        case DLL_PROCESS_ATTACH:
		    if (load_nspr_table())
			    return FALSE;
        break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
	        break;
        case DLL_PROCESS_DETACH:
		    FreeLibrary(hDll);
		    break;
	    default:
	        break;
    }
    return TRUE;
}

