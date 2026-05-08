#pragma once

#include "XboxDmAdapter/XboxDmAdapter.h"
#include "XboxDmAdapter/Private/Types.h"

#ifdef __cplusplus
namespace ds2 {
namespace Xbox {

// Spawn the LLDB worker thread (network listener). Called from DmEntryPoint
// after kernel callbacks have been wired up. Does not block.
void StartLldbWorker();

// Tear down the LLDB worker (best-effort). Called from DmTellRequestHalt or
// shutdown notification.
void StopLldbWorker();

} // namespace Xbox
} // namespace ds2
#endif

DMAPI HRESULT NTAPI DmUnregisterPerformanceCounter(LPCSTR Name);
