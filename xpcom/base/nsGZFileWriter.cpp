/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGZFileWriter.h"
#include "nsIFile.h"
#include "nsString.h"
#include "zlib.h"
#include "mozilla/ScopeExit.h"

#ifdef XP_WIN
#  include <io.h>
#  define _dup dup
#else
#  include <unistd.h>
#endif

nsGZFileWriter::nsGZFileWriter()
    : mInitialized(false), mFinished(false), mGZFile(nullptr) {
  mZStream.avail_out = sizeof(mBuffer);
  mZStream.next_out = mBuffer;
}

nsGZFileWriter::~nsGZFileWriter() {
  if (mInitialized && !mFinished) {
    Finish();
  }
}

nsresult nsGZFileWriter::Init(nsIFile* aFile) {
  if (NS_WARN_IF(mInitialized) || NS_WARN_IF(mFinished)) {
    return NS_ERROR_FAILURE;
  }

  // Get a FILE out of our nsIFile.  Convert that into a file descriptor which
  // gzip can own.  Then close our FILE, leaving only gzip's fd open.

  FILE* file;
  nsresult rv = aFile->OpenANSIFileDesc("wb", &file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return InitANSIFileDesc(file);
}

nsresult nsGZFileWriter::InitANSIFileDesc(FILE* aFile) {
  if (NS_WARN_IF(mInitialized) || NS_WARN_IF(mFinished)) {
    return NS_ERROR_FAILURE;
  }

  int err = deflateInit2(&mZStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         MAX_WBITS + /* gzip encoding */ 16,
                         /* DEF_MEM_LEVEL */ 8, Z_DEFAULT_STRATEGY);
  if (err != Z_OK) {
    return NS_ERROR_FAILURE;
  }
  mGZFile = aFile;
  mInitialized = true;

  return NS_OK;
}

nsresult nsGZFileWriter::Write(const nsACString& aStr) {
  if (NS_WARN_IF(!mInitialized) || NS_WARN_IF(mFinished)) {
    return NS_ERROR_FAILURE;
  }

  // gzwrite uses a return value of 0 to indicate failure.  Otherwise, it
  // returns the number of uncompressed bytes written.  To ensure we can
  // distinguish between success and failure, don't call gzwrite when we have 0
  // bytes to write.
  if (aStr.IsEmpty()) {
    return NS_OK;
  }

  mZStream.avail_in = aStr.Length();
  mZStream.next_in =
      reinterpret_cast<Bytef*>(const_cast<char*>(aStr.BeginReading()));

  auto cleanup = mozilla::MakeScopeExit([&] {
    mZStream.avail_in = 0;
    mZStream.next_in = nullptr;
  });
  auto onerror = mozilla::MakeScopeExit([&] {
    mFinished = true;
    fclose(mGZFile);
  });

  do {
    if (mZStream.avail_out == 0) {
      if (fwrite(mBuffer, 1, sizeof(mBuffer), mGZFile) != sizeof(mBuffer)) {
        return NS_ERROR_FAILURE;
      }
      mZStream.avail_out = sizeof(mBuffer);
      mZStream.next_out = mBuffer;
    }
    int err = deflate(&mZStream, Z_NO_FLUSH);
    if (err == Z_STREAM_ERROR) {
      return NS_ERROR_FAILURE;
    }
  } while (mZStream.avail_in);

  onerror.release();
  return NS_OK;
}

nsresult nsGZFileWriter::Finish() {
  if (NS_WARN_IF(!mInitialized) || NS_WARN_IF(mFinished)) {
    return NS_ERROR_FAILURE;
  }

  mZStream.avail_in = 0;
  mZStream.next_in = nullptr;

  auto cleanup = mozilla::MakeScopeExit([&] {
    mFinished = true;
    fclose(mGZFile);
  });

  int err;
  do {
    err = deflate(&mZStream, Z_FINISH);
    if (err == Z_STREAM_ERROR) {
      return NS_ERROR_FAILURE;
    }
    size_t length = sizeof(mBuffer) - mZStream.avail_out;
    if (fwrite(mBuffer, 1, length, mGZFile) != length) {
      return NS_ERROR_FAILURE;
    }
    mZStream.avail_out = sizeof(mBuffer);
    mZStream.next_out = mBuffer;
  } while (err != Z_STREAM_END);

  // Ignore errors from fclose; it's not like there's anything we can do about
  // it, at this point!
  return NS_OK;
}
