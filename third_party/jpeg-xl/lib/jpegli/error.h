// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_ERROR_H_
#define LIB_JPEGLI_ERROR_H_

#include <stdarg.h>
#include <stdint.h>

#include "lib/jpegli/common.h"
#include "lib/jxl/base/compiler_specific.h"

namespace jpegli {

bool FormatString(char* buffer, const char* format, ...);

}  // namespace jpegli

// `error_exit` should be no-return; but let's add some guarantees on our side.
#define JPEGLI_ERROR(format, ...)                                            \
  jpegli::FormatString(cinfo->err->msg_parm.s, ("%s:%d: " format), __FILE__, \
                       __LINE__, ##__VA_ARGS__),                             \
      (*cinfo->err->error_exit)(reinterpret_cast<j_common_ptr>(cinfo)),      \
      JXL_CRASH()

#define JPEGLI_WARN(format, ...)                                             \
  jpegli::FormatString(cinfo->err->msg_parm.s, ("%s:%d: " format), __FILE__, \
                       __LINE__, ##__VA_ARGS__),                             \
      (*cinfo->err->emit_message)(reinterpret_cast<j_common_ptr>(cinfo), -1)

#define JPEGLI_TRACE(level, format, ...)                                     \
  if (cinfo->err->trace_level >= (level))                                    \
  jpegli::FormatString(cinfo->err->msg_parm.s, ("%s:%d: " format), __FILE__, \
                       __LINE__, ##__VA_ARGS__),                             \
      (*cinfo->err->emit_message)(reinterpret_cast<j_common_ptr>(cinfo),     \
                                  (level))

#define JPEGLI_CHECK(condition)                     \
  do {                                              \
    if (!(condition)) {                             \
      JPEGLI_ERROR("JPEGLI_CHECK: %s", #condition); \
    }                                               \
  } while (0)

#endif  // LIB_JPEGLI_ERROR_H_
