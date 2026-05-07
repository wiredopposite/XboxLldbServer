//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "Core/SoftwareBreakpointManager.h"
#include "Target/Process.h"
#include "Target/Thread.h"
#include "Utils/HexValues.h"
#include "Utils/Log.h"

#include <cstdlib>

#define super ds2::BreakpointManager

namespace ds2 {

SoftwareBreakpointManager::SoftwareBreakpointManager(
    Target::ProcessBase *process)
    : super(process), _enabled(false) {}

SoftwareBreakpointManager::~SoftwareBreakpointManager() { clear(); }

void SoftwareBreakpointManager::clear() {
  super::clear();
  _insns.clear();
}

ErrorCode SoftwareBreakpointManager::enableLocation(Site const &site,
                                                    Target::Thread *thread) {
  ByteVector opcode;
  ByteVector old;
  ErrorCode error;

  if (thread != nullptr) {
    DS2LOG(Warning, "thread-specific software breakpoints are unsupported");
  }

  getOpcode(site.size, opcode);
  old.resize(opcode.size());
  error = _process->readMemory(site.address, old.data(), old.size());
  if (error != kSuccess) {
    DS2LOG(Error, "cannot enable breakpoint at %" PRI_PTR ", readMemory failed",
           PRI_PTR_CAST(site.address.value()));
    return error;
  }

  error = _process->writeMemory(site.address, opcode.data(), opcode.size());
  if (error != kSuccess) {
    DS2LOG(Error,
           "cannot enable breakpoint at %" PRI_PTR ", writeMemory failed",
           PRI_PTR_CAST(site.address.value()));
    return error;
  }

  DS2LOG(Debug,
         "set breakpoint instruction 0x%s at %" PRI_PTR " (saved insn 0x%s)",
         ToHex(opcode).c_str(), PRI_PTR_CAST(site.address.value()),
         ToHex(old).c_str());

  _insns[site.address] = old;

  return kSuccess;
}

ErrorCode SoftwareBreakpointManager::disableLocation(Site const &site,
                                                     Target::Thread *thread) {
  ErrorCode error;
  ByteVector old = _insns[site.address];

  if (thread != nullptr) {
    DS2LOG(Warning, "thread-specific software breakpoints are unsupported");
  }

  error = _process->writeMemory(site.address, old.data(), old.size());
  if (error != kSuccess) {
    DS2LOG(Error, "cannot restore instruction at %" PRI_PTR,
           PRI_PTR_CAST(site.address.value()));
    return error;
  }

  DS2LOG(Debug, "reset instruction 0x%s at %" PRI_PTR, ToHex(old).c_str(),
         PRI_PTR_CAST(site.address.value()));

  _insns.erase(site.address);

  return kSuccess;
}

void SoftwareBreakpointManager::enable(Target::Thread *thread) {
  super::enable(thread);

  _enabled = true;
}

void SoftwareBreakpointManager::disable(Target::Thread *thread) {
  super::disable(thread);

  _enabled = false;
}

bool SoftwareBreakpointManager::enabled(Target::Thread *thread) const {
  if (thread != nullptr) {
    DS2LOG(Warning, "thread-specific software breakpoints are unsupported");
  }

  return _enabled;
}

bool SoftwareBreakpointManager::fillStopInfo(Target::Thread *thread,
                                             StopInfo &stopInfo) {
  BreakpointManager::Site site;
  int bpIdx = hit(thread, site);
  if (bpIdx < 0) {
    return false;
  }
  stopInfo.reason = StopInfo::kReasonBreakpoint;
  return true;
}


int SoftwareBreakpointManager::hit(Target::Thread *thread, Site &site) {
  ds2::Architecture::CPUState state;

  //
  // Ignore hardware single-stepping.
  //
  if (thread->state() == Target::Thread::kStepped)
    return 0;

  thread->readCPUState(state);
  state.setPC(state.pc() - 1);

  if (super::hit(state.pc(), site)) {
    //
    // Move the PC back to the instruction, INT3 will move
    // the instruction pointer to the next byte.
    //
    if (thread->writeCPUState(state) != kSuccess)
      abort();

    uint64_t ex = state.pc();
    thread->readCPUState(state);
    DS2ASSERT(ex == state.pc());

    return 0;
  }
  return -1;
}

void SoftwareBreakpointManager::getOpcode(size_t size,
                                          ByteVector &opcode) const {
  DS2ASSERT(size == 1);
  opcode.clear();
  opcode.push_back('\xcc'); // int 3
}

ErrorCode SoftwareBreakpointManager::isValid(Address const &address,
                                             size_t size, Mode mode) const {
  DS2ASSERT(mode == kModeExec);
  if (size != 0 && size != 1) {
    DS2LOG(Debug, "Received unsupported breakpoint size %zu", size);
    return kErrorInvalidArgument;
  }

  return super::isValid(address, size, mode);
}

size_t SoftwareBreakpointManager::chooseBreakpointSize(Address const &) const {
  // On x86 and x86_64, software breakpoints will always be of size 1
  return 1;
}
} // namespace ds2
