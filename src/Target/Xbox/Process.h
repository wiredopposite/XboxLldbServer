#pragma once

#include "Host/ProcessSpawner.h"
#include "Target/ProcessBase.h"

namespace ds2 {
namespace Target {
namespace Xbox {

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
  HANDLE _handle;
  ErrorCode updateInfo() override;

  struct ThreadEvent {
    ThreadEvent *next;
    HANDLE thread;
    BOOLEAN create;
  };

  static ThreadEvent *_threadEventList;
  static HANDLE _threadEventSem;

public:
  static Target::Process *Create(Host::ProcessSpawner &spawner);
  static Target::Process *Attach(ProcessId pid);
  static void PushThreadEvent(HANDLE thread, BOOLEAN create);
  static bool PopThreadEvent(HANDLE &thread, BOOLEAN &create);

};
} // namespace Xbox
} // namespace Target
} // namespace ds2
