/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util/CompleteFile.h"

#include <cstring>     // std::strcmp
#include <stdio.h>     // FILE, fileno, fopen, getc, getc_unlocked, _getc_nolock
#include <sys/stat.h>  // stat, fstat

#include "jsapi.h"        // JS_ReportErrorNumberLatin1
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_CANT_OPEN

bool js::ReadCompleteFile(JSContext* cx, FILE* fp, FileContents& buffer) {
  /* Get the complete length of the file, if possible. */
  struct stat st;
  int ok = fstat(fileno(fp), &st);
  if (ok != 0) {
    return false;
  }
  if (st.st_size > 0) {
    if (!buffer.reserve(st.st_size)) {
      return false;
    }
  }

  /* Use the fastest available getc. */
  auto fast_getc =
#if defined(HAVE_GETC_UNLOCKED)
      getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
      _getc_nolock
#else
      getc
#endif
      ;

  // Read in the whole file. Note that we can't assume the data's length
  // is actually st.st_size, because 1) some files lie about their size
  // (/dev/zero and /dev/random), and 2) reading files in text mode on
  // Windows collapses "\r\n" pairs to single \n characters.
  for (;;) {
    int c = fast_getc(fp);
    if (c == EOF) {
      break;
    }
    if (!buffer.append(c)) {
      return false;
    }
  }

  return true;
}

/*
 * Open a source file for reading. Supports "-" and nullptr to mean stdin. The
 * return value must be fclosed unless it is stdin.
 */
bool js::AutoFile::open(JSContext* cx, const char* filename) {
  if (!filename || std::strcmp(filename, "-") == 0) {
    fp_ = stdin;
  } else {
    fp_ = fopen(filename, "r");
    if (!fp_) {
      /*
       * Use Latin1 variant here because the encoding of filename is
       * platform dependent.
       */
      JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_CANT_OPEN,
                                 filename, "No such file or directory");
      return false;
    }
  }
  return true;
}
