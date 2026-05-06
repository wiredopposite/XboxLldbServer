#pragma once

#include <windows.h>

// #ifdef DM_SHARED_LIB
#if 1
#define DMAPI extern "C" __declspec(dllexport)
#else
#define DMAPI extern "C" __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

typedef struct _DMXAPI
{
    UCHAR XapiStarted;
    ULONG LastErrorTlsOff;
    ULONG CurrentFiberTlsOff;
} DMXAPI, *PDMXAPI;

typedef struct _DMGD
{
    PULONG FrameCounter;
    PULONG FlipCounter;
    VOID *Surface;//struct D3DSurface *Surface;
    PVOID *RegisterBase;
    PVOID PerfCounters;
    PULONG pdwOpcode;
    PUCHAR *ppSnapshotBuffer;
    PVOID D3DInternalsFunction;
    ULONG DMGDVersion;
} DMGD, *PDMGD;

typedef struct _DMDVD
{
    ULARGE_INTEGER TimeStamp;
    ULONG Parameters;
    ULONG ResponseTime;
} DMDVD, *PDMDVD;

typedef 
EXCEPTION_DISPOSITION 
(CDECL *PEXCEPTION_ROUTINE)(
    PEXCEPTION_RECORD, 
    PVOID, 
    PCONTEXT, 
    PVOID
);

typedef struct _EXCEPTION_REGISTRATION_RECORD
{
    struct _EXCEPTION_REGISTRATION_RECORD *Next;
    PEXCEPTION_ROUTINE Handler;
} EXCEPTION_REGISTRATION_RECORD, *PEXCEPTION_REGISTRATION_RECORD;
_Static_assert(sizeof(EXCEPTION_REGISTRATION_RECORD) == 0x8, "EXCEPTION_REGISTRATION_RECORD size mismatch");

typedef struct _KTRAP_FRAME 
{
    ULONG DbgEbp;
    ULONG DbgEip;
    ULONG DbgArgMark;
    ULONG DbgArgPointer;
    ULONG TempDegCs;
    ULONG TempEsp;
    ULONG Edx;
    ULONG Ecx;
    ULONG Eax;
    PEXCEPTION_REGISTRATION_RECORD ExceptionList;
    ULONG Edi;
    ULONG Esi;
    ULONG Ebx;
    ULONG Ebp;
    ULONG ErrCode;
    ULONG Eip;
    ULONG SegCs;
    ULONG EFlags;
    ULONG HardwareEsp;
    ULONG HardwareSegSs;
} KTRAP_FRAME, *PKTRAP_FRAME;
_Static_assert(sizeof(KTRAP_FRAME) == 0x50, "KTRAP_FRAME size mismatch");

typedef 
BOOLEAN 
(*PDEBUG_ROUTINE)(
    PKTRAP_FRAME, 
    struct _KEXCEPTION_FRAME*,
    PEXCEPTION_RECORD,
    PCONTEXT,
    BOOLEAN
);

typedef struct _DMINIT
{
    PDEBUG_ROUTINE *DebugRoutine;
    PLIST_ENTRY LoadedModuleList;
    ULONG Flags;
    VOID (CDECL **ClockIntRoutine)(PKTRAP_FRAME TrapFrame);
    VOID (CDECL **ProfIntRoutine)(PKTRAP_FRAME TrapFrame);
    ULONG (CDECL *TellRoutine)(ULONG, PVOID*);
    VOID (FASTCALL **CtxSwapNotifyRoutine)(HANDLE OldThreadId, HANDLE NewThreadId);
    struct _XProfpGlobals *XProfpDataPtr;
    PDMGD D3DDriverData;
    PDMXAPI XapiData;
    PUCHAR DisallowXbdm;
    VOID (CDECL *HalStartProfileRoutine)(ULONG);
    VOID (CDECL *HalStopProfileRoutine)(ULONG);
    VOID (CDECL *HalProfileIntervalRoutine)(ULONG);
    PVOID *DpcDispatchNotifyRoutine;
    PDMDVD NextDVDSample;
} DMINIT, *PDMINIT;
_Static_assert(sizeof(DMINIT) == 0x40, "sizeof(DMINIT) must be 0x40");

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
