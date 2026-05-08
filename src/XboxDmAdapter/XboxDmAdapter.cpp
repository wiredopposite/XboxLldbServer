#include <memory>
#include <minIni.h>
#include <xboxkrnl/xboxkrnl.h>
#include <nxdk/net.h>

#include "XboxDmAdapter/Private/XboxDmAdapter.h"

#include "Core/SessionThread.h"
#include "GDBRemote/DebugSessionImpl.h"
#include "GDBRemote/SlaveSessionImpl.h"
#include "GDBRemote/PlatformSessionImpl.h"
#include "Host/QueueChannel.h"
#include "Host/Socket.h"
#include "Target/Process.h"
#include "Utils/Log.h"

// XNet config status, for the Tell* IP queries.
struct XNetConfigStatus_Stub {
    union {
        struct { unsigned char b[4]; } bytes;
        ULONG S_addr;
    } S_un;
} XNetStatus_unused;

// ---------------------------------------------------------------------------
// Adapter-private state
// ---------------------------------------------------------------------------

typedef struct _DMSTATE {
    DMINIT Init;
    DMXAPI Xapi;
    DMGD   Gd;
    ULONG  DbgIp;
    ULONG  DbgSubnet;
    ULONG  DbgGateway;
    BOOL   IsStaticIp;
    BOOL   IsDebugging;
    HAL_SHUTDOWN_REGISTRATION ShutdownReg;
} DMSTATE, *PDMSTATE;

static const char *DmpSettingsIniPath =
    "\\Device\\Harddisk0\\Partition1\\XboxDmAdapterSettings.ini";

static PVOID    XProfpGlobals;
static DMSTATE  DmState;

// Worker thread state
static HANDLE   gWorkerThread = nullptr;
static volatile BOOL gShutdownRequested = FALSE;

// The kernel's KiDebugRoutine is always installed (in-kernel KD is built in
// on the dev kernel — see ExpStartDebugMonitor, init.c:632). When we install
// ours we save the previous so traps we don't claim still reach KD.
static PDEBUG_ROUTINE DmpPrevDebugRoutine = nullptr;

// Publish DMINIT to the kernel via KPCR->Prcb->DebugMonitorData. The kernel's
// internal `DmGetCurrentDmi()` macro reads this slot to locate TellRoutine,
// XapiData, D3DDriverData. nxdk doesn't export KeGetCurrentPrcb, so write
// the cell directly: KPCR is at fs:0 and Prcb->DebugMonitorData lives at
// fs:0x278 (per private/ntos/inc/i386.h KPRCB layout).
static inline void DmpSetDebugMonitorData(PVOID Data) {
    __asm__ __volatile__("mov %0, %%fs:0x278"
                         :
                         : "r"(Data)
                         : "memory");
}

// LLDB server defaults — eventually configurable via the .ini.
static const USHORT kDefaultLldbPort = 12345;

// ---------------------------------------------------------------------------
// Settings / drive helpers (stubbed; persistence not load-bearing for LLDB)
// ---------------------------------------------------------------------------

VOID DmpEnterDebugger(VOID) {
    DmState.IsDebugging = TRUE;
}

VOID DmpPrepareReboot(VOID) {
    ds2::Xbox::StopLldbWorker();
}

VOID DmpMapDebugDrive(PDMDRIVE Drive) {
    // Optional: would mount a host-side debug drive. Not needed for LLDB.
}

VOID DmpWriteSettingsIni(VOID) {
    // TODO: persist DmState.IsStaticIp, DbgIp, DbgSubnet, DbgGateway via minIni.
}

VOID DmpClearSettingsIni(VOID) {
    // TODO: ini_remove on the settings file.
}

VOID DmpAquireIpAddress(VOID) {
    // Networking comes up in the worker thread via nxNetInit; nothing to do here.
}

UINT_PTR DmpGetModuleProcAddress(PCHAR ModuleName, PCHAR ProcName) {
    // TODO: walk DmState.Init.LoadedModuleList for ModuleName, then resolve
    // ProcName via the module's export table.
    return 0;
}

// Placeholder shutdown notification. Retail xbdm masked the local APIC here
// so reboots could happen with a debugger attached. Until reboot handling is
// wired up, this is a no-op.
DMEXT VOID CDECL DmpDisableApic(PHAL_SHUTDOWN_NOTIFICATION ShutdownReg) {
}

// ---------------------------------------------------------------------------
// Kernel callbacks — these all run in trap / DPC context.
// They MUST be fast and reentrant; their only job is to forward to Process.
// ---------------------------------------------------------------------------

DMEXT BOOLEAN CDECL
DmpTrapHandler(PKTRAP_FRAME TrapFrame, PKEXCEPTION_FRAME ExceptionFrame,
               PEXCEPTION_RECORD ExceptionRecord, PCONTEXT Context,
               BOOLEAN FirstChance) {
    auto *proc = ds2::Target::Process::Instance();
    if (proc && proc->onTrap(TrapFrame, ExceptionRecord, Context, FirstChance)) {
        return TRUE;
    }
    // Not attached, or Process declined — fall through to the previous
    // handler (KD's KiDebugRoutine).
    if (DmpPrevDebugRoutine) {
        return DmpPrevDebugRoutine(TrapFrame, ExceptionFrame, ExceptionRecord,
                                   Context, FirstChance);
    }
    return FALSE;
}

DMEXT VOID NTAPI
DmpCreateThreadNotifyRoutine(PETHREAD Thread, HANDLE ThreadId, BOOLEAN Create) {
    auto *proc = ds2::Target::Process::Instance();
    if (proc) {
        proc->onThreadEvent(Thread, ThreadId, Create);
    }
}

DMEXT VOID FASTCALL
DmProfileThreadSwitchNotifyCallback(HANDLE /*OldThreadId*/,
                                    HANDLE /*NewThreadId*/) {
    // Profiler-only. No-op for LLDB.
}

DMEXT VOID CDECL DmpClockInt(PKTRAP_FRAME /*ptf*/) {}
DMEXT VOID CDECL DmpProfInt(PKTRAP_FRAME /*ptf*/) {}
DMEXT VOID CDECL DmpProfileIntervalRoutine(ULONG /*interval*/) {}

// ---------------------------------------------------------------------------
// Tell routine — external RPC entry. Matches retail xbdm semantics for the
// codes the kernel actually sends; LLDB-specific cases are minimal.
// ---------------------------------------------------------------------------

DMEXT ULONG CDECL DmpTellRoutine(ULONG Code, PVOID *Data) {
    ULONG result = 0;
    HRESULT status;

    switch (Code) {
    case DmTellEnterDebugger:
        DmpEnterDebugger();
        break;

    case DmTellPrepareReboot:
        DmpPrepareReboot();
        break;

    case DmTellMapDebugDrive:
        DmpMapDebugDrive((PDMDRIVE)Data);
        break;

    case DmTellSetFrameCounter:
        DmState.Gd.FrameCounter = (PULONG)Data;
        break;

    case DmTellSetPerfCounters:
        DmState.Gd.PerfCounters = Data;
        status = DmEnableGPUCounter((Data != NULL));
        result = (status >= 0);
        break;

    case DmTellRegisterPerfCounter: {
        auto *p = (PDMREGISTERPERFCOUNTERPARAMBLOCK)Data;
        status = DmRegisterPerformanceCounter(p->szName, p->dwType, p->pvArg);
        result = (status >= 0);
    } break;

    case DmTellUnregisterPerfCounter: {
        auto *p = (PDMREGISTERPERFCOUNTERPARAMBLOCK)Data;
        status = DmUnregisterPerformanceCounter(p->szName);
        result = (status >= 0);
    } break;

    case DmTellSetDebugIp:
        if (Data) {
            DmState.DbgIp = (ULONG)Data;
        }
        DmState.IsStaticIp = (Data != NULL);
        DmpWriteSettingsIni();
        __attribute__((fallthrough));
    case DmTellGetDebugIp:
        result = (-(ULONG)DmState.IsStaticIp) & DmState.DbgIp;
        break;

    case DmTellInsertAllocEntry: {
        auto *p = (PDM_TELL_ALLOCATION_ENTRY)Data;
        status = DmInsertAllocationEntry(p->AllocPtr, p->AllocSize, p->AllocType);
        result = (status >= 0);
    } break;

    case DmTellRemoveAllocEntry: {
        auto *p = (PDM_TELL_ALLOCATION_ENTRY)Data;
        status = DmRemoveAllocationEntry(p->AllocPtr, p->AllocSize, p->AllocType);
        result = (status >= 0);
    } break;

    case DmTellIsDebugging:
        result = DmState.IsDebugging;
        break;

    case DmTellAquireIp:
        DmpAquireIpAddress();
        break;

    case DmTellSetStaticIpFlag:
        DmState.IsStaticIp = (BOOL)(ULONG_PTR)Data;
        DmpWriteSettingsIni();
        __attribute__((fallthrough));
    case DmTellGetStaticIpFlag:
        result = DmState.IsStaticIp;
        break;

    case DmTellSetStaticIpAddress:
        DmState.DbgIp = (ULONG)Data;
        DmpWriteSettingsIni();
        __attribute__((fallthrough));
    case DmTellGetStaticIpAddress:
        result = DmState.DbgIp;
        break;

    case DmTellSetStaticSubnet:
        DmState.DbgSubnet = (ULONG)Data;
        DmpWriteSettingsIni();
        __attribute__((fallthrough));
    case DmTellGetStaticSubnet:
        result = DmState.DbgSubnet;
        break;

    case DmTellSetStaticGateway:
        DmState.DbgGateway = (ULONG)Data;
        DmpWriteSettingsIni();
        __attribute__((fallthrough));
    case DmTellGetStaticGateway:
        result = DmState.DbgGateway;
        break;

    // The active-IP queries originally read XNetGetConfigStatus. Until we
    // have a wrapper, return zero — these are informational only.
    case DmTellGetActiveIp:
    case DmTellGetActiveSubnet:
    case DmTellGetActiveGateway:
        result = 0;
        break;

    case DmTellWipeConfig:
        DmpClearSettingsIni();
        break;

    case DmTellGetModuleProcAddress: {
        auto *p = (PDM_TELL_MODULE_PROC_ADDRESS)Data;
        result = (ULONG)DmpGetModuleProcAddress(p->ModuleName, p->ProcName);
    } break;

    case DmTellRequestHalt:
        ds2::Xbox::StopLldbWorker();
        break;

    default:
        break;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Exported xbdm entry points the kernel imports
// ---------------------------------------------------------------------------

HRESULT DmRegisterPerformanceCounter(LPCSTR /*Name*/, DWORD /*Type*/, PVOID) {
    return S_OK;
}

HRESULT DmUnregisterPerformanceCounter(LPCSTR /*Name*/) {
    return S_OK;
}

HRESULT DmEnableGPUCounter(BOOL /*Enable*/) {
    return S_OK;
}

HRESULT DmInsertAllocationEntry(PVOID /*AllocPtr*/, SIZE_T /*AllocSize*/, USHORT /*AllocType*/) {
    return S_OK;
}

HRESULT DmRemoveAllocationEntry(PVOID /*AllocPtr*/, SIZE_T /*AllocSize*/, USHORT /*AllocType*/) {
    return S_OK;
}

HRESULT DmGetProcAddress(HANDLE /*Module*/, LPCSTR /*ProcName*/, PVOID *Ret) {
    if (Ret) *Ret = nullptr;
    return E_FAIL;
}

// ---------------------------------------------------------------------------
// LLDB worker thread
// ---------------------------------------------------------------------------

namespace {

using ds2::GDBRemote::DebugSessionImpl;
using ds2::GDBRemote::Session;
using ds2::GDBRemote::SessionDelegate;
using ds2::Host::QueueChannel;
using ds2::Host::Socket;

int RunDebugServer(ds2::Host::Channel *channel, SessionDelegate *impl) {
    Session session(ds2::GDBRemote::kCompatibilityModeLLDB);
    QueueChannel qchannel(channel);
    SessionThread thread(&qchannel, &session);

    session.setDelegate(impl);
    session.create(&qchannel);

    DS2LOG(Debug, "LLDB session starting");
    thread.start();

    while (session.receive(/*cooked=*/true))
        continue;

    DS2LOG(Debug, "LLDB session ended");
    return 0;
}

} // namespace

DMEXT VOID NTAPI LldbWorkerEntry(PVOID /*Context*/) {
    DS2LOG(Debug, "LLDB worker entering — bringing up network");

    nx_net_parameters_t netParams{};
    netParams.ipv4_mode = NX_NET_AUTO;
    if (nxNetInit(&netParams) != 0) {
        DS2LOG(Fatal, "nxNetInit failed; LLDB worker exiting");
        return;
    }

    while (!gShutdownRequested) {
        auto socket = std::make_unique<Socket>();
        char portStr[8];
        // hand-format port to avoid pulling in <to_chars> here
        unsigned int p = kDefaultLldbPort;
        portStr[0] = '0' + (p / 10000) % 10;
        portStr[1] = '0' + (p /  1000) % 10;
        portStr[2] = '0' + (p /   100) % 10;
        portStr[3] = '0' + (p /    10) % 10;
        portStr[4] = '0' + (p /     1) % 10;
        portStr[5] = '\0';

        if (!socket->listen("0.0.0.0", portStr)) {
            DS2LOG(Error, "LLDB listen failed: %s", socket->error().c_str());
            LARGE_INTEGER delay;
            delay.QuadPart = -1LL * 10 * 1000 * 1000; // 1s
            KeDelayExecutionThread(KernelMode, FALSE, &delay);
            continue;
        }

        DS2LOG(Debug, "LLDB listening on 0.0.0.0:%s", portStr);

        auto channel = socket->accept();
        if (!channel) {
            continue;
        }

        DS2LOG(Debug, "LLDB client connected");

        // Mark the in-kernel "process" as attached so the trap handler stops
        // forwarding TRUEs to default handling.
        if (auto *proc = ds2::Target::Process::Instance()) {
            proc->setAttached(true);
        } 
        else {
            // First connection: attach the singleton.
            ds2::Target::Process::Attach(/*pid=*/1);
        }

        auto impl = std::make_unique<DebugSessionImpl>();
        RunDebugServer(channel.get(), impl.get());

        if (auto *proc = ds2::Target::Process::Instance()) {
            proc->setAttached(false);
        }
        DS2LOG(Debug, "LLDB client disconnected");
    }

    DS2LOG(Debug, "LLDB worker exiting");
}

namespace ds2 {
namespace Xbox {

void StartLldbWorker() {
    if (gWorkerThread != nullptr)
        return;

    HANDLE thread = nullptr;
    NTSTATUS status = PsCreateSystemThreadEx(
        &thread,
        /*ThreadExtensionSize=*/0,
        /*KernelStackSize=*/64 * 1024,
        /*TlsDataSize=*/0,
        /*ThreadId=*/nullptr,
        (PKSTART_ROUTINE)LldbWorkerEntry,
        /*StartContext=*/nullptr,
        /*CreateSuspended=*/FALSE,
        /*DebuggerThread=*/TRUE,
        /*SystemRoutine=*/nullptr);

    if (!NT_SUCCESS(status)) {
        return;
    }
    gWorkerThread = thread;
}

void StopLldbWorker() {
    gShutdownRequested = TRUE;
    // The worker will pick up gShutdownRequested at the top of its loop.
    // We don't forcibly terminate (nothing safe to do that with).
}

} // namespace Xbox
} // namespace ds2

// ---------------------------------------------------------------------------
// Kernel entry point — the linker uses this as the module entry symbol.
// Returns promptly; long-running work happens on the worker thread.
// ---------------------------------------------------------------------------

DMEXT ULONG CDECL DmEntryPoint(PVOID /*pImageBase*/, PDMINIT pDmInit, ULONG) {
    XProfpGlobals = pDmInit->XProfpDataPtr;
    DmState.Init = *pDmInit;

    // Wire kernel callbacks. The first three are pointer-to-pointer slots
    // (the kernel dereferences through them); TellRoutine is a direct slot.
    // Save the prior DebugRoutine first — it's KD's handler (KiDebugRoutine,
    // always installed on the dev kernel) so unclaimed traps can chain to it.
    DmpPrevDebugRoutine = *DmState.Init.DebugRoutine;
    *DmState.Init.DebugRoutine        = DmpTrapHandler;
    *DmState.Init.ClockIntRoutine     = DmpClockInt;
    *DmState.Init.ProfIntRoutine      = DmpProfInt;
    *DmState.Init.CtxSwapNotifyRoutine = DmProfileThreadSwitchNotifyCallback;
    DmState.Init.TellRoutine          = DmpTellRoutine;
    DmState.Init.XapiData             = &DmState.Xapi;
    DmState.Init.D3DDriverData        = &DmState.Gd;

    // Track thread create/exit (queues into the Process event ring).
    PsSetCreateThreadNotifyRoutine(DmpCreateThreadNotifyRoutine);

    // Publish ourselves to the kernel via PRCB->DebugMonitorData. The kernel
    // reads TellRoutine / XapiData / D3DDriverData through this pointer.
    DmpSetDebugMonitorData(&DmState.Init);

    // Build the singleton Process now so the trap handler has a target the
    // moment a trap fires, even before any client has connected.
    ds2::Target::Process::Attach(/*pid=*/1);

    // Spin up the LLDB listener on a worker thread; return to the kernel.
    ds2::Xbox::StartLldbWorker();

    return 0;
}
