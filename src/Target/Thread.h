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
#include "Target/ThreadBase.h"

namespace ds2 {
namespace Target {

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

} // namespace Target
} // namespace ds2
