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

#include "Base.h"
#include "Constants.h"
#include "Target/ThreadBase.h"
#include "Types.h"

namespace ds2 {
namespace Utils {

class Stringify {
public:
  static char const *Error(ErrorCode error);
  static char const *ThreadState(Target::ThreadBase::State state);
  static char const *StopEvent(StopInfo::Event event);
  static char const *StopReason(StopInfo::Reason reason);

  static char const *WSAError(DWORD error);
  static char const *DebugEvent(DWORD event);
  static char const *ExceptionCode(DWORD code);
};
} // namespace Utils
} // namespace ds2

// Private stuff used by implementation functions.
#if defined(STRINGIFY_H_INTERNAL)
#include "Utils/Log.h"
#include "Utils/String.h"

#if defined(OS_DARWIN)
#define ATT_TLS __thread
#elif defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_WIN32)
#define ATT_TLS thread_local
#else
#error "Target not supported."
#endif

#define DO_STRINGIFY(VALUE)                                                    \
  case VALUE:                                                                  \
    return #VALUE;

#define DO_STRINGIFY_ALIAS(VALUE, NAME)                                        \
  case VALUE:                                                                  \
    return #NAME;

#define DO_DEFAULT(MESSAGE, VALUE)                                             \
  default:                                                                     \
    do {                                                                       \
      DS2LOG(Warning, MESSAGE ": %#lx", (unsigned long)VALUE);                 \
      static ATT_TLS char tmp[20];                                             \
      ds2::Utils::SNPrintf(tmp, sizeof(tmp), "%#lx", (unsigned long)VALUE);    \
      return tmp;                                                              \
    } while (0);
#endif
