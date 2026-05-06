#include "Host/ProcessSpawner.h"

namespace ds2 {
namespace Host {

ProcessSpawner::ProcessSpawner() : _pid(0), _exitStatus(0), _signalCode(0) {}

ProcessSpawner::~ProcessSpawner() = default;

bool ProcessSpawner::setExecutable(std::string const &path) {
  _executablePath = path;
  return true;
}

bool ProcessSpawner::setShellCommand(std::string const &command) {
  _executablePath = command;
  return true;
}

bool ProcessSpawner::setWorkingDirectory(std::string const &path) {
  _workingDirectory = path;
  return true;
}

bool ProcessSpawner::setArguments(StringCollection const &args) {
  _arguments = args;
  return true;
}

bool ProcessSpawner::setEnvironment(EnvironmentBlock const &env) {
  _environment = env;
  return true;
}

bool ProcessSpawner::addEnvironment(std::string const &key,
                                    std::string const &val) {
  _environment[key] = val;
  return true;
}

ErrorCode ProcessSpawner::run(std::function<bool()> preExecAction) {
  if (!preExecAction()) {
    return kErrorUnsupported;
  }
  return kErrorUnsupported;
}

ErrorCode ProcessSpawner::wait() { return kErrorUnsupported; }

bool ProcessSpawner::isRunning() const { return false; }

void ProcessSpawner::flushAndExit() {}

ErrorCode ProcessSpawner::input(ByteVector const &) { return kErrorUnsupported; }

} // namespace Host
} // namespace ds2
