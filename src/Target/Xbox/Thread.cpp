#include <windows.h>
#include "Target/ThreadBase.h"
#include "Target/Xbox/Thread.h"
#include "Target/Xbox/Process.h"
#include "Host/Platform.h"

#define super ds2::Target::ThreadBase
using ds2::Host::Platform;

namespace ds2 {
namespace Target {
namespace Xbox {

Thread::Thread(Process *process, ThreadId tid, HANDLE handle) 
  : super(process, tid), _handle(handle) { }

Thread::~Thread() { 
  ::CloseHandle(_handle); 
}

ErrorCode Thread::terminate() { 
  return kErrorUnsupported;
}

ErrorCode Thread::suspend() { 
  DWORD result = SuspendThread(_handle);
  if (result == (DWORD)-1) {
    return Platform::TranslateError(::GetLastError());
  }
  _state = kStopped;
  _stopInfo.event = StopInfo::kEventStop;
  _stopInfo.reason = StopInfo::kReasonNone;
  return kSuccess;
}

ErrorCode Thread::step(int signal, Address const &address) { 
  CHK(modifyRegisters(
      [](Architecture::CPUState &state) { state.gp.eflags |= (1 << 8); }));
  CHK(resume(signal, address));
  return kSuccess;
}

ErrorCode Thread::resume(int signal, Address const &address) { 
  DWORD result = ResumeThread(_handle);
  if (result == (DWORD)-1) {
    return Platform::TranslateError(::GetLastError());
  }
  _state = kRunning;
  return kSuccess; 
}

ErrorCode Thread::readCPUState(Architecture::CPUState &) {
  return kErrorUnsupported;
}

ErrorCode Thread::writeCPUState(Architecture::CPUState const &) {
  return kErrorUnsupported;
}

void Thread::updateState() {}

} // namespace Xbox
} // namespace Target
} // namespace ds2
