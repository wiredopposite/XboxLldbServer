//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include <cstdlib>

extern "C" double atof(char const *str) {
  if (str == nullptr) {
    return 0.0;
  }
  char *end = nullptr;
  return std::strtod(str, &end);
}
