#pragma once

#include "Target/ThreadBase.h"

namespace ds2 {
namespace Target {
namespace Xbox {

class Thread : public ds2::Target::ThreadBase {
protected:
  friend class Process;
  Thread(Process *process, ThreadId tid, HANDLE handle);

public:
  ~Thread() override;

public:
  ErrorCode terminate() override;
  ErrorCode suspend() override;
  ErrorCode step(int signal = 0, Address const &address = Address()) override;
  ErrorCode resume(int signal = 0,
                   Address const &address = Address()) override;

public:
  ErrorCode readCPUState(Architecture::CPUState &state) override;
  ErrorCode writeCPUState(Architecture::CPUState const &state) override;

protected:
  HANDLE _handle;
  void updateState() override;
};

} // namespace Xbox
} // namespace Target
} // namespace ds2
