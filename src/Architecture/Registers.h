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

#if defined(ARCH_ARM)
#include "Architecture/ARM/Registers.h"
#elif defined(ARCH_ARM64)
#include "Architecture/ARM/Registers.h"
#include "Architecture/ARM64/Registers.h"
#elif defined(ARCH_X86)
#include "Architecture/I386/Registers.h"
#elif defined(ARCH_X86_64)
#include "Architecture/I386/Registers.h"
#include "Architecture/X86_64/Registers.h"
#else
#error "Architecture not supported."
#endif
