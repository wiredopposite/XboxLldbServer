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

#include "Types.h"

#include <functional>

namespace ds2 {
namespace Host {

class ProcessSpawner {
protected:
  typedef std::function<void(void *buf, size_t size)> RedirectDelegate;

  std::string _executablePath;
  StringCollection _arguments;
  EnvironmentBlock _environment;
  std::string _workingDirectory;
  std::string _outputBuffer;
  ProcessId _pid;
  int _exitStatus;
  int _signalCode;

public:
  ProcessSpawner();
  ~ProcessSpawner();

public:
  bool setExecutable(std::string const &path);
  bool setShellCommand(std::string const &command);
  bool setWorkingDirectory(std::string const &path);

public:
  bool setArguments(StringCollection const &args);

  template <typename... Args> inline bool setArguments(Args const &... args) {
    std::string args_[] = {args...};
    return setArguments(StringCollection(&args_[0], &args_[sizeof...(Args)]));
  }

public:
  bool setEnvironment(EnvironmentBlock const &env);
  bool addEnvironment(std::string const &key, std::string const &val);

public:
  bool redirectInputToConsole() { return true; }
  bool redirectOutputToConsole() { return true; }
  bool redirectErrorToConsole() { return true; }

public:
  bool redirectInputToNull() { return true; }
  bool redirectOutputToNull() { return true; }
  bool redirectErrorToNull() { return true; }

public:
  bool redirectInputToFile(std::string const &) { return false; }
  bool redirectOutputToFile(std::string const &) { return false; }
  bool redirectErrorToFile(std::string const &) { return false; }

public:
  bool redirectOutputToBuffer() { return true; }
  bool redirectErrorToBuffer() { return true; }

public:
  bool redirectInputToTerminal() { return false; }

public:
  bool redirectOutputToDelegate(RedirectDelegate) { return false; }
  bool redirectErrorToDelegate(RedirectDelegate) { return false; }

public:
  ErrorCode run(std::function<bool()> preExecAction = []() { return true; });
  ErrorCode wait();
  bool isRunning() const;
  void flushAndExit();

public:
  inline ProcessId pid() const { return _pid; }
  inline int exitStatus() const { return _exitStatus; }
  inline int signalCode() const { return _signalCode; }
  inline std::string const &executable() const { return _executablePath; }
  inline StringCollection const &arguments() const { return _arguments; }
  inline EnvironmentBlock const &environment() const { return _environment; }

public:
  ErrorCode input(ByteVector const &buf);
  inline std::string const &output() const { return _outputBuffer; }
};
} // namespace Host
} // namespace ds2
