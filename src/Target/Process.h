//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#pragma once

#include "Types/Base.h"
#include "Host/ProcessSpawner.h"
#include "Target/ProcessBase.h"
#include "XboxDmAdapter/Private/XboxDmAdapter.h"

#include <xboxkrnl/xboxkrnl.h>

namespace ds2 {
namespace Target {

class Process : public ds2::Target::ProcessBase,
                public ds2::make_unique_enabler<Process> {
protected:
  Process();

public:
  ~Process() override;

protected:
  ErrorCode initialize(ProcessId pid, uint32_t flags) override;

public:
  ErrorCode detach() override;
  ErrorCode interrupt() override;
  ErrorCode terminate() override;
  bool isAlive() const override;

public:
  ErrorCode readString(Address const &address, std::string &str, size_t length,
                       size_t *nread = nullptr) override;
  ErrorCode readMemory(Address const &address, void *data, size_t length,
                       size_t *nread = nullptr) override;
  ErrorCode writeMemory(Address const &address, void const *data, size_t length,
                        size_t *nwritten = nullptr) override;

public:
  ErrorCode enumerateSharedLibraries(
      std::function<void(SharedLibraryInfo const &)> const &cb) override;

public:
  ErrorCode wait() override;

public:
  ErrorCode allocateMemory(size_t size, uint32_t protection,
                           uint64_t *address) override;
  ErrorCode deallocateMemory(uint64_t address, size_t size) override;

public:
  ErrorCode getMemoryRegionInfo(Address const &address,
                                MemoryRegionInfo &info) override;

protected:
  ErrorCode updateInfo() override;

public:
  static Target::Process *Create(Host::ProcessSpawner &spawner);
  static Target::Process *Attach(ProcessId pid);

  // The single in-kernel "process" everyone shares. There is exactly one.
  static Process *Instance();

  // ---- Kernel-callback entry points (called from XboxDmAdapter) ----

  // Called from DmpTrapHandler. Runs on the trapping thread, in trap context.
  // Captures the context, queues an event, signals waiters, and (if attached)
  // BLOCKS the trapping thread until the debugger says continue.
  // On return, *Context contains any modifications the debugger applied.
  // Returns TRUE if the trap was consumed (kernel returns to user code with
  // possibly modified ctx), FALSE to pass through to default handling.
  BOOLEAN onTrap(PKTRAP_FRAME TrapFrame, PEXCEPTION_RECORD ExceptionRecord,
                 PCONTEXT Context, BOOLEAN FirstChance);

  // Called from DmpCreateThreadNotifyRoutine. Pushes a ThreadCreate/Exit
  // event onto the queue.
  void onThreadEvent(PETHREAD Thread, HANDLE ThreadId, BOOLEAN Create);

  // ---- LLDB session entry points ----

  // Copies the captured context for the currently stopped thread.
  ErrorCode getCapturedContext(CONTEXT &out);

  // Stages a modified context. Will be applied to the trap on resume.
  ErrorCode setModifiedContext(CONTEXT const &in);

  // Releases the trap-stopped thread (called by the resume path in the
  // session). Resumes any threads we suspended for all-stop along the way.
  void releaseStoppedThread();

  // Marks the debugger as attached. Trap handler short-circuits when not.
  void setAttached(bool attached) { _attached = attached; }
  bool attached() const { return _attached; }

protected:
  // ---- Event queue ----
  enum class EventKind { Trap, ThreadCreate, ThreadExit, Detach };
  struct Event {
    EventKind kind;
    PETHREAD ethread;
    HANDLE threadId;
    DWORD exceptionCode;
    PVOID exceptionAddr;
    BOOLEAN firstChance;
  };

  // Bounded ring so the trap path never allocates. Single producer at any
  // given moment (Xbox is single-core; the trap handler and the thread-notify
  // routine cannot truly run concurrently), single consumer (LLDB session).
  static constexpr size_t kEventQueueSize = 64;
  Event _events[kEventQueueSize];
  volatile LONG _eventHead = 0; // consumer
  volatile LONG _eventTail = 0; // producer
  KEVENT _eventReady;           // queue non-empty
  KEVENT _resumeRequested;      // trap thread should resume

  // ---- Stop state ----
  // No lock needed: while a trap is parked on _resumeRequested, the trap
  // thread does not touch this state, so the LLDB session is the sole writer.
  // After _resumeRequested is signalled, the LLDB session is parked in wait()
  // again, so the trap thread is the sole writer.
  PETHREAD _stoppedEthread = nullptr;
  CONTEXT _capturedContext{};
  CONTEXT _modifiedContext{};
  BOOLEAN _contextModified = FALSE;

  // For all-stop mode: every other live thread we suspended on stop and must
  // resume on continue.
  std::vector<PKTHREAD> _suspendedForStop;

  // Whether a debugger has attached. When false, onTrap returns FALSE
  // immediately (kernel handles the exception itself).
  volatile bool _attached = false;

private:
  bool pushEvent(Event const &e);
  bool popEvent(Event &out);
  void suspendOtherThreads(PKTHREAD except);
  void resumeOtherThreads();
};

} // namespace Target
} // namespace ds2
