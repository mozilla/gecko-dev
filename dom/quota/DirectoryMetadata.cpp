/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectoryMetadata.h"

#include "mozilla/Result.h"
#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/StreamUtils.h"
#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"

namespace mozilla::dom::quota {

Result<OriginStateMetadata, nsresult> ReadDirectoryMetadataHeader(
    nsIBinaryInputStream& aStream) {
  AssertIsOnIOThread();

  OriginStateMetadata originStateMetadata;

  QM_TRY_UNWRAP(originStateMetadata.mLastAccessTime,
                MOZ_TO_RESULT_INVOKE_MEMBER(aStream, Read64));

  QM_TRY_UNWRAP(originStateMetadata.mPersisted,
                MOZ_TO_RESULT_INVOKE_MEMBER(aStream, ReadBoolean));

  QM_TRY_INSPECT(const bool& reservedData1,
                 MOZ_TO_RESULT_INVOKE_MEMBER(aStream, Read32));
  if (reservedData1 != 0) {
    QM_TRY(MOZ_TO_RESULT(false));
  }

  // XXX Use for the persistence type.
  QM_TRY_INSPECT(const bool& reservedData2,
                 MOZ_TO_RESULT_INVOKE_MEMBER(aStream, Read32));
  Unused << reservedData2;

  return originStateMetadata;
}

nsresult WriteDirectoryMetadataHeader(
    nsIBinaryOutputStream& aStream,
    const OriginStateMetadata& aOriginStateMetadata) {
  AssertIsOnIOThread();

  QM_TRY(MOZ_TO_RESULT(aStream.Write64(aOriginStateMetadata.mLastAccessTime)));

  QM_TRY(MOZ_TO_RESULT(aStream.WriteBoolean(aOriginStateMetadata.mPersisted)));

  // Reserved data 1
  QM_TRY(MOZ_TO_RESULT(aStream.Write32(0)));

  // Reserved data 2
  QM_TRY(MOZ_TO_RESULT(aStream.Write32(0)));

  return NS_OK;
}

Result<OriginStateMetadata, nsresult> LoadDirectoryMetadataHeader(
    nsIFile& aDirectory) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& stream,
      GetBinaryInputStream(aDirectory, nsLiteralString(METADATA_V2_FILE_NAME)));

  QM_TRY_INSPECT(const OriginStateMetadata& originStateMetadata,
                 ReadDirectoryMetadataHeader(*stream));

  QM_TRY(MOZ_TO_RESULT(stream->Close()));

  return originStateMetadata;
}

nsresult SaveDirectoryMetadataHeader(
    nsIFile& aDirectory, const OriginStateMetadata& aOriginStateMetadata) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& file,
      CloneFileAndAppend(aDirectory, nsLiteralString(METADATA_V2_FILE_NAME)));

  QM_TRY_INSPECT(const auto& stream,
                 GetBinaryOutputStream(*file, FileFlag::Update));
  MOZ_ASSERT(stream);

  QM_TRY(MOZ_TO_RESULT(
      WriteDirectoryMetadataHeader(*stream, aOriginStateMetadata)));

  QM_TRY(MOZ_TO_RESULT(stream->Flush()));

  QM_TRY(MOZ_TO_RESULT(stream->Close()));

  return NS_OK;
}

}  // namespace mozilla::dom::quota
