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
#include "Types/Constants.h"
#include "Types/Types.h"
#include "Utils/Log.h"

#include <string>
#include <utility>

#if defined(PLATFORM_NXDK) || defined(PLATFORM_WINDOWS)
#define FHANDLE_TYPE HANDLE
#else
#define FHANDLE_TYPE int
#define INVALID_HANDLE_VALUE -1
#endif

namespace ds2 {
namespace Host {

class File {
public:
  File(std::string const &path, OpenFlags flags, uint32_t mode);
  ~File();

public:
  File(const File &other) = delete;
  File &operator=(const File &other) = delete;

public:
  File(File &&other) : _fd(INVALID_HANDLE_VALUE), _lastError(kErrorInvalidHandle) {
    *this = std::move(other);
  }

  File &operator=(File &&other) {
    DS2ASSERT(&other != this);

    std::swap(_fd, other._fd);
    std::swap(_lastError, other._lastError);

    return *this;
  }

public:
  ErrorCode pread(ByteVector &buf, uint64_t &count, uint64_t offset);
  ErrorCode pwrite(ByteVector const &buf, uint64_t &count, uint64_t offset);
  ErrorCode fstat(ByteVector &buffer) const;

public:
  bool valid() const { return (_fd != INVALID_HANDLE_VALUE); }
  ErrorCode lastError() const { return _lastError; }

public:
  static ErrorCode chmod(std::string const &path, uint32_t mode);

public:
  static ErrorCode unlink(std::string const &path);

public:
  static ErrorCode createDirectory(std::string const &path, uint32_t flags);

public:
  static ErrorCode fileSize(std::string const &path, uint64_t &size);
  static ErrorCode fileMode(std::string const &path, uint32_t &mode);

protected:
  FHANDLE_TYPE _fd;
  ErrorCode _lastError;

};
} // namespace Host
} // namespace ds2
