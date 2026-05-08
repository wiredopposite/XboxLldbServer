#include "Target/Process.h"
#include "Target/Thread.h"
#include "Host/Platform.h"
#include "Utils/Log.h"

#include <cstring>

#define super ds2::Target::ProcessBase

namespace ds2 {
namespace Target {

namespace {

Process *gInstance = nullptr;

inline ThreadId TidFromHandle(HANDLE id) {
  return static_cast<ThreadId>(reinterpret_cast<uintptr_t>(id));
}
inline ThreadId TidFromEthread(PETHREAD eth) {
  return static_cast<ThreadId>(reinterpret_cast<uintptr_t>(eth));
}

} // namespace

Process *Process::Instance() { return gInstance; }

Process::Process() : super() {
  KeInitializeEvent(&_eventReady, SynchronizationEvent, FALSE);
  KeInitializeEvent(&_resumeRequested, SynchronizationEvent, FALSE);
  std::memset(_events, 0, sizeof(_events));
  gInstance = this;
}

Process::~Process() {
  detach();
  if (gInstance == this)
    gInstance = nullptr;
}

ErrorCode Process::initialize(ProcessId pid, uint32_t flags) {
  CHK(super::initialize(pid, flags));

  // Seed the thread map with the bootstrap thread (the one running
  // DmEntryPoint). It's not a trapping thread, but ProcessBase asserts on
  // having a current thread.
  if (_threads.empty()) {
    PETHREAD self = reinterpret_cast<PETHREAD>(KeGetCurrentThread());
    auto *t = new Thread(this, TidFromEthread(self), self);
    _currentThread = t;
  }
  return kSuccess;
}

ErrorCode Process::detach() {
  _attached = false;
  // Wake anyone parked in onTrap so the system can keep running.
  KeSetEvent(&_resumeRequested, 0, FALSE);

  Event e{};
  e.kind = EventKind::Detach;
  pushEvent(e);
  KeSetEvent(&_eventReady, 0, FALSE);

  cleanup();
  _flags = 0;
  _pid = kAnyProcessId;
  _terminated = true;
  return kSuccess;
}

ErrorCode Process::interrupt() {
  // No external way to break a running kernel; the GDB \x03 path is not
  // wired to deliver to us. Reserve for future implementation.
  return kErrorUnsupported;
}

ErrorCode Process::terminate() {
  // We can't terminate the kernel. Treat as detach.
  return detach();
}

bool Process::isAlive() const { return !_terminated; }

// --------------------- Event queue ---------------------
// Lock-free SPSC ring. Push from trap/notify context, pop from LLDB session.

bool Process::pushEvent(Event const &e) {
  LONG tail = _eventTail;
  LONG nextTail = (tail + 1) % static_cast<LONG>(kEventQueueSize);
  if (nextTail == _eventHead) {
    return false; // full — drop rather than deadlock the kernel
  }
  _events[tail] = e;
  InterlockedExchange(&_eventTail, nextTail);
  return true;
}

bool Process::popEvent(Event &out) {
  LONG head = _eventHead;
  if (head == _eventTail) {
    return false;
  }
  out = _events[head];
  InterlockedExchange(&_eventHead,
                      (head + 1) % static_cast<LONG>(kEventQueueSize));
  return true;
}

// --------------------- All-stop helpers ---------------------

void Process::suspendOtherThreads(PKTHREAD except) {
  _suspendedForStop.clear();
  for (auto const &kv : _threads) {
    Thread *t = kv.second;
    PKTHREAD kth = reinterpret_cast<PKTHREAD>(t->ethread());
    if (kth == nullptr || kth == except)
      continue;
    KeSuspendThread(kth);
    _suspendedForStop.push_back(kth);
  }
}

void Process::resumeOtherThreads() {
  for (PKTHREAD kth : _suspendedForStop) {
    KeResumeThread(kth);
  }
  _suspendedForStop.clear();
}

// --------------------- Trap handler entry from XboxDmAdapter ---------------------

BOOLEAN Process::onTrap(PKTRAP_FRAME /*TrapFrame*/,
                        PEXCEPTION_RECORD ExceptionRecord, PCONTEXT Context,
                        BOOLEAN FirstChance) {
  if (!_attached) {
    return FALSE; // pass through to default kernel handling
  }

  PETHREAD self = reinterpret_cast<PETHREAD>(KeGetCurrentThread());

  _capturedContext = *Context;
  _stoppedEthread = self;
  _contextModified = FALSE;

  // All-stop: freeze every other tracked thread.
  suspendOtherThreads(reinterpret_cast<PKTHREAD>(self));

  // Queue the event and wake the LLDB session.
  Event e{};
  e.kind = EventKind::Trap;
  e.ethread = self;
  e.threadId = reinterpret_cast<HANDLE>(TidFromEthread(self));
  e.exceptionCode = ExceptionRecord ? ExceptionRecord->ExceptionCode : 0;
  e.exceptionAddr = ExceptionRecord ? ExceptionRecord->ExceptionAddress : nullptr;
  e.firstChance = FirstChance;
  pushEvent(e);
  KeSetEvent(&_eventReady, 0, FALSE);

  // Park here until the debugger says continue. While we sleep, the LLDB
  // session is the only writer of _capturedContext / _modifiedContext.
  KeWaitForSingleObject(&_resumeRequested, Executive, KernelMode, FALSE,
                        nullptr);

  // Apply any modifications staged by the session, then resume the rest of
  // the system.
  if (_contextModified) {
    *Context = _modifiedContext;
    _contextModified = FALSE;
  }
  _stoppedEthread = nullptr;
  resumeOtherThreads();

  return TRUE;
}

// --------------------- Thread create/exit notification ---------------------

void Process::onThreadEvent(PETHREAD Thread, HANDLE ThreadId, BOOLEAN Create) {
  Event e{};
  e.kind = Create ? EventKind::ThreadCreate : EventKind::ThreadExit;
  e.ethread = Thread;
  e.threadId = ThreadId;
  pushEvent(e);
  KeSetEvent(&_eventReady, 0, FALSE);
}

// --------------------- Captured-context accessors ---------------------

ErrorCode Process::getCapturedContext(CONTEXT &out) {
  if (_stoppedEthread == nullptr) {
    return kErrorProcessNotFound;
  }
  out = _contextModified ? _modifiedContext : _capturedContext;
  return kSuccess;
}

ErrorCode Process::setModifiedContext(CONTEXT const &in) {
  _modifiedContext = in;
  _contextModified = TRUE;
  return kSuccess;
}

void Process::releaseStoppedThread() {
  KeSetEvent(&_resumeRequested, 0, FALSE);
}

// --------------------- LLDB-side wait() ---------------------
//
// Pump events until we get one that the session can report. Thread-create
// events update our table and loop.
//
ErrorCode Process::wait() {
  if (_terminated) {
    if (_currentThread != nullptr) {
      _currentThread->_stopInfo.event = StopInfo::kEventKill;
    }
    return kSuccess;
  }

  for (;;) {
    Event e{};
    while (!popEvent(e)) {
      KeWaitForSingleObject(&_eventReady, Executive, KernelMode, FALSE,
                            nullptr);
      if (_terminated) {
        return kSuccess;
      }
    }

    switch (e.kind) {
    case EventKind::ThreadCreate: {
      ThreadId tid = TidFromHandle(e.threadId);
      if (_threads.find(tid) == _threads.end()) {
        new Thread(this, tid, e.ethread); // self-inserts via ThreadBase
      }
      continue;
    }

    case EventKind::ThreadExit: {
      ThreadId tid = TidFromHandle(e.threadId);
      auto it = _threads.find(tid);
      if (it != _threads.end() && it->second != _currentThread) {
        removeThread(tid);
      }
      continue;
    }

    case EventKind::Trap: {
      ThreadId tid = TidFromEthread(e.ethread);
      auto it = _threads.find(tid);
      Thread *thr = (it != _threads.end())
                        ? it->second
                        : new Thread(this, tid, e.ethread);
      _currentThread = thr;
      thr->_state = Thread::kStopped;
      thr->_stopInfo.event = StopInfo::kEventStop;

      switch (e.exceptionCode) {
      case STATUS_BREAKPOINT:
        thr->_stopInfo.reason = StopInfo::kReasonBreakpoint;
        break;
      case STATUS_SINGLE_STEP:
        thr->_stopInfo.reason = StopInfo::kReasonTrace;
        break;
      case STATUS_ACCESS_VIOLATION:
      case STATUS_IN_PAGE_ERROR:
      case STATUS_STACK_OVERFLOW:
        thr->_stopInfo.reason = StopInfo::kReasonMemoryError;
        break;
      case STATUS_ILLEGAL_INSTRUCTION:
      case STATUS_PRIVILEGED_INSTRUCTION:
        thr->_stopInfo.reason = StopInfo::kReasonInstructionError;
        break;
      default:
        thr->_stopInfo.reason = StopInfo::kReasonTrap;
        break;
      }
      return kSuccess;
    }

    case EventKind::Detach:
      _terminated = true;
      if (_currentThread) {
        _currentThread->_stopInfo.event = StopInfo::kEventExit;
      }
      return kSuccess;
    }
  }
}

// --------------------- Memory access ---------------------
//
// Single shared address space — read/write is just memcpy after probing each
// page with MmIsAddressValid. For writes to read-only pages (e.g. .text for
// breakpoint patching) we go through MmDbgWriteCheck which returns a writable
// alias and an opaque PTE we hand back to MmDbgReleaseAddress.

ErrorCode Process::readString(Address const &address, std::string &str,
                              size_t length, size_t *nread) {
  str.clear();
  for (size_t i = 0; i < length; ++i) {
    char c;
    size_t n = 0;
    ErrorCode err = readMemory(address + i, &c, 1, &n);
    if (err != kSuccess || n != 1)
      return err;
    if (c == '\0') {
      if (nread)
        *nread = i + 1;
      return kSuccess;
    }
    str.push_back(c);
  }
  if (nread)
    *nread = length;
  return kSuccess;
}

ErrorCode Process::readMemory(Address const &address, void *data, size_t length,
                              size_t *nread) {
  auto *src = reinterpret_cast<uint8_t const *>(
      static_cast<uintptr_t>(address.value()));
  auto *dst = reinterpret_cast<uint8_t *>(data);
  size_t copied = 0;
  for (size_t i = 0; i < length; ++i) {
    if (!MmIsAddressValid(const_cast<uint8_t *>(src + i))) {
      break;
    }
    dst[i] = src[i];
    ++copied;
  }
  if (nread)
    *nread = copied;
  if (copied == length)
    return kSuccess;
  return copied == 0 ? kErrorInvalidAddress : kSuccess;
}

ErrorCode Process::writeMemory(Address const &address, void const *data,
                               size_t length, size_t *nwritten) {
  auto *dst = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(address.value()));
  auto *src = reinterpret_cast<uint8_t const *>(data);

  // MmDbgWriteCheck unlocks a (possibly read-only) page for write so we can
  // patch breakpoints into .text. It returns an opaque PTE we must hand back
  // to MmDbgReleaseAddress when finished.
  HARDWARE_PTE savedPte{};
  PVOID writable = MmDbgWriteCheck(dst, &savedPte);
  auto *w = reinterpret_cast<uint8_t *>(writable ? writable : dst);

  size_t copied = 0;
  for (size_t i = 0; i < length; ++i) {
    if (!MmIsAddressValid(w + i)) {
      break;
    }
    w[i] = src[i];
    ++copied;
  }

  if (writable) {
    MmDbgReleaseAddress(dst, &savedPte);
  }

  if (nwritten)
    *nwritten = copied;
  if (copied == length)
    return kSuccess;
  return copied == 0 ? kErrorInvalidAddress : kSuccess;
}

ErrorCode Process::enumerateSharedLibraries(
    std::function<void(SharedLibraryInfo const &)> const &cb) {
  // TODO: walk DMINIT.LoadedModuleList (LDR_DATA_TABLE_ENTRY chain) and
  // produce a SharedLibraryInfo entry per module.
  return kErrorUnsupported;
}

ErrorCode Process::allocateMemory(size_t size, uint32_t protection,
                                  uint64_t *address) {
  // TODO: wrap MmAllocateContiguousMemoryEx / MmAllocateSystemMemory and
  // translate `protection` to an Xbox PAGE_* constant.
  return kErrorUnsupported;
}

ErrorCode Process::deallocateMemory(uint64_t address, size_t size) {
  // TODO: pair with allocateMemory's chosen kernel allocator.
  return kErrorUnsupported;
}

ErrorCode Process::getMemoryRegionInfo(Address const &address,
                                       MemoryRegionInfo &info) {
  // TODO: query MmQueryAddressProtect / MmQueryAllocationSize and fill in
  // info.start/length/protection.
  return kErrorUnsupported;
}

ErrorCode Process::updateInfo() {
  _info.clear();
  _info.pid = _pid;
  _info.cpuType = kCPUTypeX86;
  _info.cpuSubType = kCPUSubTypeX86_ALL;
  _info.endian = kEndianLittle;
  _info.pointerSize = sizeof(void *);
  _info.osType = "Xbox";
  _info.osVendor = "Microsoft";
  return kSuccess;
}

Target::Process *Process::Create(Host::ProcessSpawner &) { return nullptr; }

Target::Process *Process::Attach(ProcessId pid) {
  auto process = make_protected_unique();
  if (process->initialize(pid, kFlagAttachedProcess) != kSuccess) {
    return nullptr;
  }
  process->setAttached(true);
  return process.release();
}

} // namespace Target
} // namespace ds2
