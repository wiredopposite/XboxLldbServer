#pragma once

#include <windows.h>

#pragma ms_struct on

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _DM_TELL_CODE
{
    DmTellEnterDebugger = 1,
    DmTellPrepareReboot,
    DmTellMapDebugDrive,
    DmTellSetFrameCounter,
    DmTellSetPerfCounters,
    DmTellRegisterPerfCounter,
    DmTellUnregisterPerfCounter,
    DmTellSetDebugIp,
    DmTellGetDebugIp,
    DmTellInsertAllocEntry,
    DmTellRemoveAllocEntry,
    DmTellIsDebugging,
    DmTellAquireIp,
    DmTellSetStaticIpFlag,
    DmTellGetStaticIpFlag,
    DmTellSetStaticIpAddress,
    DmTellGetStaticIpAddress,
    DmTellSetStaticSubnet,
    DmTellGetStaticSubnet,
    DmTellSetStaticGateway,
    DmTellGetStaticGateway,
    DmTellGetActiveIp,
    DmTellGetActiveSubnet,
    DmTellGetActiveGateway,
    DmTellWipeConfig,
    DmTellGetModuleProcAddress,
    DmTellRequestHalt
} DM_TELL_CODE, *PDM_TELL_CODE;

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
typedef KTRAP_FRAME KEXCEPTION_FRAME, *PKEXCEPTION_FRAME;

typedef struct _DMREGISTERPERFCOUNTERPARAMBLOCK
{
    PCHAR szName;
    ULONG dwType;
    PVOID pvArg;
} DMREGISTERPERFCOUNTERPARAMBLOCK, *PDMREGISTERPERFCOUNTERPARAMBLOCK;
_Static_assert(sizeof(DMREGISTERPERFCOUNTERPARAMBLOCK) == 0xc, "DMREGISTERPERFCOUNTERPARAMBLOCK size mismatch");

typedef 
BOOLEAN 
(CDECL *PDEBUG_ROUTINE)(
    PKTRAP_FRAME TrapFrame, 
    PKEXCEPTION_FRAME ExceptionFrame,
    PEXCEPTION_RECORD ExceptionRecord,
    PCONTEXT Context,
    BOOLEAN FirstChance
);

typedef struct _DMINIT
{
    // PDEBUG_ROUTINE *DebugRoutine;
    BOOLEAN (CDECL **DebugRoutine)(PKTRAP_FRAME, PKEXCEPTION_FRAME, PEXCEPTION_RECORD, PCONTEXT, BOOLEAN);
    PLIST_ENTRY LoadedModuleList;
    ULONG Flags;
    VOID (CDECL **ClockIntRoutine)(PKTRAP_FRAME TrapFrame);
    VOID (CDECL **ProfIntRoutine)(PKTRAP_FRAME TrapFrame);
    ULONG (CDECL *TellRoutine)(ULONG Code, PVOID *Data);
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

#ifdef __cplusplus
}
#endif

#pragma ms_struct reset
