#include <windows.h>
#include "Host/File.h"
#include "Host/Platform.h"

namespace ds2 {
namespace Host {

// struct PathMapEntry {
//   const char *hostPath;
//   const char *xboxPath;
// };

// static const PathMapEntry kPathMap[] = {
//   { "e:/", "\\Device\\Harddisk0\\Partition1\\" },
//   { "c:/", "\\Device\\Harddisk0\\Partition2\\" },
//   { "d:/", "\\Device\\CdRom0\\" },
//   { "f:/", "\\Device\\Harddisk0\\Parition6\\" },
//   { "g:/", "\\Device\\Harddisk0\\Parition7\\" },
// };

struct WinOpenParams {
  DWORD desiredAccess = 0;
  DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  DWORD creationDisposition = OPEN_EXISTING;
  DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
};

static BOOLEAN convertPath(std::string const &path, std::string &converted) {
  if (path.empty() || path[0] != '/') {
    return FALSE;
  }

  bool cdRom = false;
  int partNum = 0;
  // converted = "\\??\\" + path;
  // if (path.starts_with("e", std::string::compare::))
  // std::replace(converted.begin(), converted.end(), '/', '\\');
  return TRUE;
}

static BOOLEAN convertFlags(OpenFlags ds2Flags, WinOpenParams &params) {
  if (ds2Flags & kOpenFlagNoFollow || ds2Flags & kOpenFlagCloseOnExec)
    return FALSE;

  if (ds2Flags & kOpenFlagRead)
    params.desiredAccess |= GENERIC_READ;
  if (ds2Flags & kOpenFlagWrite || ds2Flags & kOpenFlagTruncate)
    params.desiredAccess |= GENERIC_WRITE;
  if (ds2Flags & kOpenFlagAppend)
    params.desiredAccess |= FILE_APPEND_DATA;

  if (ds2Flags & kOpenFlagNonBlocking)
    params.flagsAndAttributes |= FILE_FLAG_OVERLAPPED;

  bool create = (ds2Flags & kOpenFlagCreate) != 0;
  bool newOnly = (ds2Flags & kOpenFlagNewOnly) != 0;
  bool truncate = (ds2Flags & kOpenFlagTruncate) != 0;

  if (newOnly) {
    params.creationDisposition = CREATE_NEW;
  }
  else if (create && truncate) {
    params.creationDisposition = CREATE_ALWAYS;
  }
  else if (create) {
    params.creationDisposition = OPEN_ALWAYS;
  }
  else if (truncate) {
    params.creationDisposition = TRUNCATE_EXISTING;
  }
  else {
    params.creationDisposition = OPEN_EXISTING;
  }
  return TRUE;
}

static OVERLAPPED makeOverlapped(uint64_t offset) {
  OVERLAPPED overlapped = {};
  overlapped.Offset = static_cast<DWORD>(offset & MAXDWORD);
  overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
  return overlapped;
}

File::File(std::string const &path, OpenFlags flags, uint32_t mode) {
  WinOpenParams params;
  if (!convertFlags(flags, params)) {
    _lastError = kErrorInvalidArgument;
    _fd = INVALID_HANDLE_VALUE;
    return;
  }

  _fd = ::CreateFileA(
    path.c_str(), 
    params.desiredAccess, 
    params.shareMode, 
    NULL, 
    params.creationDisposition, 
    params.flagsAndAttributes, 
    NULL);

  _lastError = 
    (_fd == INVALID_HANDLE_VALUE) 
      ? Platform::TranslateError(::GetLastError()) 
      : kSuccess;
}

File::~File() {
  if (valid()) {
    ::CloseHandle(_fd);
  }
}

ErrorCode File::pread(ByteVector &buf, uint64_t &count, uint64_t offset) {
  if (!valid()) {
    return _lastError = kErrorInvalidHandle;
  }

  if (count > MAXDWORD) {
    return _lastError = kErrorInvalidArgument;
  }

  DWORD toRead = static_cast<DWORD>(count);
  OVERLAPPED overlapped = makeOverlapped(offset);
  DWORD bytesRead = 0;

  buf.resize(toRead);

  if (!::ReadFile(_fd, buf.data(), toRead, &bytesRead, &overlapped)) {
    DWORD error = ::GetLastError();
    if (error != ERROR_IO_PENDING ||
        !::GetOverlappedResult(_fd, &overlapped, &bytesRead, TRUE)) {
      buf.clear();
      return _lastError = Platform::TranslateError(::GetLastError());
    }
  }

  buf.resize(bytesRead);
  count = bytesRead;
  return _lastError = kSuccess;
}

ErrorCode File::pwrite(ByteVector const &buf, uint64_t &count, uint64_t offset) {
  if (!valid()) {
    return _lastError = kErrorInvalidHandle;
  }

  if (count > MAXDWORD || count > buf.size()) {
    return _lastError = kErrorInvalidArgument;
  }

  DWORD toWrite = static_cast<DWORD>(count);
  OVERLAPPED overlapped = makeOverlapped(offset);
  DWORD bytesWritten = 0;

  if (!::WriteFile(_fd, buf.data(), toWrite, &bytesWritten, &overlapped)) {
    DWORD error = ::GetLastError();
    if (error != ERROR_IO_PENDING ||
        !::GetOverlappedResult(_fd, &overlapped, &bytesWritten, TRUE)) {
      return _lastError = Platform::TranslateError(::GetLastError());
    }
  }

  count = bytesWritten;
  return _lastError = kSuccess;
}

ErrorCode File::fstat(ByteVector &) const { return kErrorUnsupported; }

ErrorCode File::chmod(std::string const &path, uint32_t mode) {
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;
  if (!::GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
    return Platform::TranslateError(::GetLastError());
  }

  DWORD attributes = fileInfo.dwFileAttributes;
  if (mode & 0222) {
    attributes &= ~FILE_ATTRIBUTE_READONLY;
  } else {
    attributes |= FILE_ATTRIBUTE_READONLY;
  }

  if (!::SetFileAttributesA(path.c_str(), attributes)) {
    return Platform::TranslateError(::GetLastError());
  }

  return kSuccess;
}

ErrorCode File::unlink(std::string const &) { return kErrorUnsupported; }

ErrorCode File::createDirectory(std::string const &, uint32_t) {
  return kErrorUnsupported;
}

ErrorCode File::fileSize(std::string const &path, uint64_t &size) {
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;
  if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
    return Platform::TranslateError(::GetLastError());
  }
  size = 
    (static_cast<uint64_t>(fileInfo.nFileSizeHigh) << 32) 
    | fileInfo.nFileSizeLow;
  return kSuccess;
}

ErrorCode File::fileMode(std::string const &path, uint32_t &mode) {
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;
  if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
    return Platform::TranslateError(::GetLastError());
  }
  mode = 0444;
  if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
    mode |= 0222;
  if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    mode |= 0111;

  return kSuccess;
}

} // namespace Host
} // namespace ds2
