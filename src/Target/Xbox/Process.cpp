#include "Target/Process.h"
#include "Host/Platform.h"
#include "Target/Thread.h"
#include "Dm/Types.h"
#include "Utils/Log.h"

#define super ds2::Target::ProcessBase

namespace ds2 {
namespace Target {
namespace Xbox {

HANDLE Process::_threadEventSem = nullptr;
Process::ThreadEvent *Process::_threadEventList = nullptr;

extern "C" VOID NTAPI ThreadCreateRoutine (
  IN PETHREAD Thread,
  IN HANDLE ThreadId,
  IN BOOLEAN Create) 
{
  HANDLE hThread;
  NTSTATUS status = ObReferenceObjectByHandle(
    ThreadId, &PsThreadObjectType, &hThread);

  if (!NT_SUCCESS(status)) {
    DS2BUG("Failed to reference thread object by handle: %08lx", status);
    return;
  }
  Process::PushThreadEvent(hThread, Create);
}

void Process::PushThreadEvent(HANDLE thread, BOOLEAN create) {
  auto waitResult = WaitForSingleObject(&_threadEventSem, INFINITE);
  DS2ASSERT(waitResult != WAIT_FAILED);

  ThreadEvent *event = new ThreadEvent();
  event->thread = thread;
  event->create = create;
  event->next = _threadEventList;
  _threadEventList = event;

  ReleaseSemaphore(&_threadEventSem, 1, NULL);
}

bool Process::PopThreadEvent(HANDLE &thread, BOOLEAN &create) {
  auto waitResult = WaitForSingleObject(&_threadEventSem, INFINITE);
  DS2ASSERT(waitResult != WAIT_FAILED);

  if (_threadEventList == nullptr) {
    ReleaseSemaphore(&_threadEventSem, 1, NULL);
    return false;
  }

  ThreadEvent *event = _threadEventList;
  while (event->next != nullptr) {
    event = event->next;
  }

  thread = event->thread;
  create = event->create;

  if (_threadEventList == event) {
    _threadEventList = nullptr;
  }
  else {
    ThreadEvent *prev = _threadEventList;
    while (prev->next != event) {
      prev = prev->next;
    }
    prev->next = nullptr;
  }

  delete event;

  ReleaseSemaphore(&_threadEventSem, 1, NULL);
  return true;
}

Process::Process() : super(), _handle(INVALID_HANDLE_VALUE) {}

Process::~Process() { detach(); }

ErrorCode Process::initialize(ProcessId pid, uint32_t flags) {
  CHK(super::initialize(pid, flags));

  if (!_threadEventSem) {
    _threadEventSem = CreateSemaphore(NULL, 0, UINT_MAX, "NXDKThreadEventSem");
    DS2ASSERT(_threadEventSem != NULL);
    PsSetCreateThreadNotifyRoutine(ThreadCreateRoutine);
  }

  if (_threads.empty()) {
    _currentThread = new Thread(
      this, 
      GetCurrentThreadId(), 
      GetCurrentThread());
  }

  return kSuccess;
}

ErrorCode Process::detach() {
  cleanup();
  _flags = 0;
  _pid = kAnyProcessId;
  _terminated = true;
  return kSuccess;
}

ErrorCode Process::interrupt() { return kErrorUnsupported; }

ErrorCode Process::terminate() {
  _terminated = true;
  return kSuccess;
}

bool Process::isAlive() const { return !_terminated; }

ErrorCode Process::readString(Address const &address, 
                              std::string &str, 
                              size_t length, 
                              size_t *nread) 
{
  return kErrorUnsupported;
}

ErrorCode Process::readMemory(Address const &address, 
                              void *data, 
                              size_t length, 
                              size_t *nread) 
{
  return kErrorUnsupported;
}

ErrorCode Process::writeMemory(Address const &address,
                               void const *data, 
                               size_t length, 
                               size_t *nwritten) 
{
  return kErrorUnsupported;
}

ErrorCode Process::enumerateSharedLibraries( 
  std::function<void(SharedLibraryInfo const &)> const &) 
{
  return kErrorUnsupported;
}

template <typename ThreadCollectionType, typename ThreadIdType>
static Thread *findThread(ThreadCollectionType const &threads,
                          ThreadIdType tid) {
  auto threadIt = threads.find(tid);
  DS2ASSERT(threadIt != threads.end());
  return threadIt->second;
}

ErrorCode Process::wait() { 
  if (_terminated) {
    DS2ASSERT(_currentThread != nullptr);
    _currentThread->_stopInfo.event = StopInfo::kEventKill;
    return kSuccess;
  }

  HANDLE thread;
  BOOLEAN create;

  while (true) {
    if (!PopThreadEvent(thread, create)) {
      continue;
    }
    if (!create) {
      _currentThread = findThread(_threads, GetThreadId(thread));
      _currentThread->updateState();
      _currentThread->_stopInfo.event = StopInfo::kEventExit;
      return kSuccess;
    }
    else {
      _currentThread = new Thread(this, GetThreadId(thread), thread);
      CHK(_currentThread->resume());
      continue;
    }
  }
}

ErrorCode Process::allocateMemory(size_t size, uint32_t protection, uint64_t *address) {
  return kErrorUnsupported;
}

ErrorCode Process::deallocateMemory(uint64_t address, size_t size) {
  return kErrorUnsupported;
}

ErrorCode Process::getMemoryRegionInfo(Address const &address, MemoryRegionInfo &info) {
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

Target::Process *Process::Create(Host::ProcessSpawner &) {
  return nullptr;
}

Target::Process *Process::Attach(ProcessId pid) {
  auto process = make_protected_unique();
  if (process->initialize(pid, kFlagAttachedProcess) != kSuccess) {
    return nullptr;
  }
  return process.release();
}

} // namespace Xbox
} // namespace Target
} // namespace ds2
