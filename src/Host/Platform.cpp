//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "Host/Platform.h"
#include "Types/Base.h"

namespace ds2 {
namespace Host {

  
void Platform::Initialize() { setvbuf(stdout, nullptr, _IONBF, 0); }

size_t Platform::GetPageSize() { return 4096; }

const char *Platform::GetHostName(bool) { return "xbox"; }

char const *Platform::GetOSTypeName() { return "nxdk"; }

char const *Platform::GetOSVendorName() { return "microsoft"; }

char const *Platform::GetOSVersion() { return "unknown"; }

char const *Platform::GetOSBuild() { return "unknown"; }

char const *Platform::GetOSKernelVersion() { return "unknown"; }

bool Platform::GetUserName(UserId const &, std::string &) { return false; }

bool Platform::GetGroupName(GroupId const &, std::string &) { return false; }

int Platform::OpenFile(std::string const &, uint32_t, uint32_t) { return -1; }

bool Platform::CloseFile(int) { return false; }

bool Platform::IsFilePresent(std::string const &) { return false; }

std::string Platform::GetWorkingDirectory() { return std::string("/"); }

bool Platform::SetWorkingDirectory(std::string const &) { return false; }

ds2::ProcessId Platform::GetCurrentProcessId() { return 1; }

const char *Platform::GetSelfExecutablePath() { return "default.xbe"; }

bool Platform::GetCurrentEnvironment(EnvironmentBlock &env) {
  env.clear();
  return true;
}

ErrorCode Platform::TranslateError(DWORD err) { 
  switch (err) {
  case ERROR_PATH_NOT_FOUND:
  case ERROR_FILE_NOT_FOUND:
  case ERROR_SECTOR_NOT_FOUND:
    return kErrorNotFound;
  case ERROR_INVALID_PARAMETER:
    return kErrorInvalidArgument;
  case ERROR_ACCESS_DENIED:
    return kErrorAccessDenied;
  case ERROR_FILE_EXISTS:
  case ERROR_ALREADY_EXISTS:
    return kErrorAlreadyExist;
  case ERROR_BUSY_DRIVE:
  case ERROR_PATH_BUSY:
  case ERROR_BUSY:
  // case ERROR_PIPE_BUSY:
  // case ERROR_IRQ_BUSY:
  // case RPC_S_SERVER_TOO_BUSY:
  // case ERROR_CTX_MODEM_RESPONSE_BUSY:
  // case ERROR_CTX_WINSTATION_BUSY:
  // case FRS_ERR_SYSVOL_IS_BUSY:
  // case ERROR_DS_BUSY:
  // case ERROR_DS_DRA_BUSY:
  // case ERROR_DS_CROSS_REF_BUSY:
    return kErrorBusy;
  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
  // case ERROR_NOT_ENOUGH_SERVER_MEMORY:
  // case ERROR_IPSEC_IKE_OUT_OF_MEMORY:
    return kErrorNoMemory;
  }
  return ds2::kErrorUnsupported; 
}

ErrorCode Platform::TranslateError() { return TranslateError(::GetLastError()); }

bool Platform::GetProcessInfo(ProcessId pid, ProcessInfo &info) {
  info.clear();
  info.pid = pid;
  info.name = "xbox";
  info.cpuType = kCPUTypeX86;
  info.cpuSubType = kCPUSubTypeX86_ALL;
  info.endian = kEndianLittle;
  info.pointerSize = sizeof(void *);
  info.osType = "Xbox";
  info.osVendor = "Microsoft";
  return true;
}

void Platform::EnumerateProcesses(
    bool, UserId const &, std::function<void(ProcessInfo const &)> const &cb) {
  ProcessInfo info;
  GetProcessInfo(GetCurrentProcessId(), info);
  cb(info);
}

bool Platform::TerminateProcess(ProcessId) { return false; }

std::string Platform::GetThreadName(ProcessId, ThreadId) { return std::string(); }

bool Platform::GetExecutableFileBuildID(std::string const &, ByteVector &) {
  return false;
}

ds2::CPUType Platform::GetCPUType() {
#if defined(ARCH_ARM) || defined(ARCH_ARM64)
  return (sizeof(void *) == 8) ? kCPUTypeARM64 : kCPUTypeARM;
#elif defined(ARCH_X86) || defined(ARCH_X86_64)
  return (sizeof(void *) == 8) ? kCPUTypeX86_64 : kCPUTypeI386;
#elif defined(ARCH_RISCV)
#if __riscv_xlen == 32
  return kCPUTypeRISCV32;
#elif __riscv_xlen == 64
  return kCPUTypeRISCV64;
#elif __riscv_xlen == 128
  return kCPUTypeRISCV128;
#endif
#else
#error "Architecture not supported."
#endif
}

ds2::CPUSubType Platform::GetCPUSubType() {
#if defined(ARCH_ARM)
#if defined(__ARM_ARCH_7EM__)
  return kCPUSubTypeARM_V7EM;
#elif defined(__ARM_ARCH_7M__)
  return kCPUSubTypeARM_V7M;
#elif defined(OS_WIN32) || defined(__ARM_ARCH_7__) ||                          \
    defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__)
  return kCPUSubTypeARM_V7;
#endif
#endif // ARCH_ARM
  return kCPUSubTypeInvalid;
}

ds2::Endian Platform::GetEndian() {
#if defined(ENDIAN_LITTLE)
  return kEndianLittle;
#elif defined(ENDIAN_BIG)
  return kEndianBig;
#elif defined(ENDIAN_MIDDLE)
  return kEndianPDP;
#else
  return kEndianUnknown;
#endif
}

size_t Platform::GetPointerSize() { return sizeof(void *); }
} // namespace Host
} // namespace ds2
