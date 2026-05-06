//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "Utils/Log.h"
#include "Host/Platform.h"
#include "Utils/Backtrace.h"
#include "Utils/CompilerSupport.h"
#include "Utils/String.h"

#include <cstdio>
#include <cstring>
#include <limits.h>
#include <sstream>
#include <vector>

namespace ds2 {
namespace {

LogLevel sLogLevel;
bool sColorsEnabled = false;
// stderr is handled a bit differently on Windows, especially when running
// under powershell. We can simply use stdout for log output.
#if defined(OS_WIN32)
FILE *sOutputStream = stdout;
#else
FILE *sOutputStream = stderr;
#endif
std::string sOutputFilename;
} // namespace

LogLevel GetLogLevel() { return sLogLevel; }

void SetLogLevel(LogLevel level) { sLogLevel = level; }

std::string const &GetLogOutputFilename() { return sOutputFilename; }

void SetLogOutputFilename(std::string const &filename) {
  FILE *stream = fopen(filename.c_str(), "a");
  if (stream == nullptr) {
    DS2LOG(Error, "unable to open %s for writing: %s", filename.c_str(),
           strerror(errno));
    return;
  }

  sOutputStream = stream;
  sOutputFilename = filename;
}

void SetLogColorsEnabled(bool enabled) { sColorsEnabled = enabled; }

static void vLog(int level, char const *classname, char const *funcname,
                 char const *format, va_list ap) {
  if (level < sLogLevel) {
    return;
  }

  std::stringstream ss;

  std::vector<char> buffer;
  size_t required_bytes = 128;

  do {
    va_list ap_copy;
    va_copy(ap_copy, ap);
    buffer.resize(required_bytes + 1);
    required_bytes =
        ds2::Utils::VSNPrintf(buffer.data(), buffer.size(), format, ap_copy);
    va_end(ap_copy);
  } while (required_bytes >= buffer.size());

  std::stringstream functag;
  if (classname != nullptr)
    functag << classname << "::";
  functag << funcname;

  ss << '[' << Host::Platform::GetCurrentProcessId() << ']';
  ss << '[' << functag.str() << ']';

  char const *color = nullptr;
  char const *label = nullptr;

  switch (level) {
  case kLogLevelFatal:
    color = "\x1b[1;31m";
    label = "FATAL  ";
    break;
  case kLogLevelError:
    color = "\x1b[1;31m";
    label = "ERROR  ";
    break;
  case kLogLevelWarning:
    color = "\x1b[1;33m";
    label = "WARNING";
    break;
  case kLogLevelInfo:
    color = "\x1b[1;32m";
    label = "INFO   ";
    break;
  case kLogLevelDebug:
    color = "\x1b[1;36m";
    label = "DEBUG  ";
    break;
  case kLogLevelPacket:
    color = "\x1b[0;35m";
    label = "PACKET  ";
    break;
  default:
    DS2_UNREACHABLE();
  }

  ss << ' ';

  if (color != nullptr && sColorsEnabled) {
    ss << color << label << "\x1b[m" << ':';
  } else if (label != nullptr) {
    ss << label << ':';
  }

  ss << ' ' << buffer.data() << std::endl;

  OutputDebugStringA(ss.str().c_str());

  fputs(ss.str().c_str(), sOutputStream);
  fflush(sOutputStream);

  if (level == kLogLevelFatal) {
    // ds2::Utils::PrintBacktrace();
    abort();
  }
}

void Log(int level, char const *classname, char const *funcname,
         char const *format, ...) {
  va_list ap;

  va_start(ap, format);
  vLog(level, classname, funcname, format, ap);
  va_end(ap);
}
} // namespace ds2
