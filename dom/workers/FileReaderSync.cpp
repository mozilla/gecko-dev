/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileReaderSync.h"

#include "jsfriendapi.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/File.h"
#include "nsContentUtils.h"
#include "mozilla/dom/FileReaderSyncBinding.h"
#include "nsCExternalHandlerService.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsIConverterInputStream.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

#include "RuntimeService.h"

USING_WORKERS_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;
using mozilla::dom::Optional;
using mozilla::dom::GlobalObject;

// static
already_AddRefed<FileReaderSync>
FileReaderSync::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsRefPtr<FileReaderSync> frs = new FileReaderSync();

  return frs.forget();
}

bool
FileReaderSync::WrapObject(JSContext* aCx,
                           JS::Handle<JSObject*> aGivenProto,
                           JS::MutableHandle<JSObject*> aReflector)
{
  return FileReaderSyncBinding_workers::Wrap(aCx, this, aGivenProto, aReflector);
}

void
FileReaderSync::ReadAsArrayBuffer(JSContext* aCx,
                                  JS::Handle<JSObject*> aScopeObj,
                                  Blob& aBlob,
                                  JS::MutableHandle<JSObject*> aRetval,
                                  ErrorResult& aRv)
{
  uint64_t blobSize = aBlob.GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  UniquePtr<char[], JS::FreePolicy> bufferData(js_pod_malloc<char>(blobSize));
  if (!bufferData) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  aBlob.GetInternalStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  uint32_t numRead;
  aRv = stream->Read(bufferData.get(), blobSize, &numRead);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  NS_ASSERTION(numRead == blobSize, "failed to read data");

  JSObject* arrayBuffer = JS_NewArrayBufferWithContents(aCx, blobSize, bufferData.get());
  if (!arrayBuffer) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  bufferData.release();

  aRetval.set(arrayBuffer);
}

void
FileReaderSync::ReadAsBinaryString(Blob& aBlob,
                                   nsAString& aResult,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> stream;
  aBlob.GetInternalStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  uint32_t numRead;
  do {
    char readBuf[4096];
    aRv = stream->Read(readBuf, sizeof(readBuf), &numRead);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    uint32_t oldLength = aResult.Length();
    AppendASCIItoUTF16(Substring(readBuf, readBuf + numRead), aResult);
    if (aResult.Length() - oldLength != numRead) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  } while (numRead > 0);
}

void
FileReaderSync::ReadAsText(Blob& aBlob,
                           const Optional<nsAString>& aEncoding,
                           nsAString& aResult,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> stream;
  aBlob.GetInternalStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsAutoCString encoding;
  unsigned char sniffBuf[3] = { 0, 0, 0 };
  uint32_t numRead;
  aRv = stream->Read(reinterpret_cast<char*>(sniffBuf),
                     sizeof(sniffBuf), &numRead);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // The BOM sniffing is baked into the "decode" part of the Encoding
  // Standard, which the File API references.
  if (!nsContentUtils::CheckForBOM(sniffBuf, numRead, encoding)) {
    // BOM sniffing failed. Try the API argument.
    if (!aEncoding.WasPassed() ||
        !EncodingUtils::FindEncodingForLabel(aEncoding.Value(),
                                             encoding)) {
      // API argument failed. Try the type property of the blob.
      nsAutoString type16;
      aBlob.GetType(type16);
      NS_ConvertUTF16toUTF8 type(type16);
      nsAutoCString specifiedCharset;
      bool haveCharset;
      int32_t charsetStart, charsetEnd;
      NS_ExtractCharsetFromContentType(type,
                                       specifiedCharset,
                                       &haveCharset,
                                       &charsetStart,
                                       &charsetEnd);
      if (!EncodingUtils::FindEncodingForLabel(specifiedCharset, encoding)) {
        // Type property failed. Use UTF-8.
        encoding.AssignLiteral("UTF-8");
      }
    }
  }

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(stream);
  if (!seekable) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Seek to 0 because to undo the BOM sniffing advance. UTF-8 and UTF-16
  // decoders will swallow the BOM.
  aRv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aRv = ConvertStream(stream, encoding.get(), aResult);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void
FileReaderSync::ReadAsDataURL(Blob& aBlob, nsAString& aResult,
                              ErrorResult& aRv)
{
  nsAutoString scratchResult;
  scratchResult.AssignLiteral("data:");

  nsString contentType;
  aBlob.GetType(contentType);

  if (contentType.IsEmpty()) {
    scratchResult.AppendLiteral("application/octet-stream");
  } else {
    scratchResult.Append(contentType);
  }
  scratchResult.AppendLiteral(";base64,");

  nsCOMPtr<nsIInputStream> stream;
  aBlob.GetInternalStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())){
    return;
  }

  uint64_t size = aBlob.GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())){
    return;
  }

  nsCOMPtr<nsIInputStream> bufferedStream;
  aRv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream, size);
  if (NS_WARN_IF(aRv.Failed())){
    return;
  }

  nsAutoString encodedData;
  aRv = Base64EncodeInputStream(bufferedStream, encodedData, size);
  if (NS_WARN_IF(aRv.Failed())){
    return;
  }

  scratchResult.Append(encodedData);

  aResult = scratchResult;
}

nsresult
FileReaderSync::ConvertStream(nsIInputStream *aStream,
                              const char *aCharset,
                              nsAString &aResult)
{
  nsCOMPtr<nsIConverterInputStream> converterStream =
    do_CreateInstance("@mozilla.org/intl/converter-input-stream;1");
  NS_ENSURE_TRUE(converterStream, NS_ERROR_FAILURE);

  nsresult rv = converterStream->Init(aStream, aCharset, 8192,
                  nsIConverterInputStream::DEFAULT_REPLACEMENT_CHARACTER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUnicharInputStream> unicharStream =
    do_QueryInterface(converterStream);
  NS_ENSURE_TRUE(unicharStream, NS_ERROR_FAILURE);

  uint32_t numChars;
  nsString result;
  while (NS_SUCCEEDED(unicharStream->ReadString(8192, result, &numChars)) &&
         numChars > 0) {
    uint32_t oldLength = aResult.Length();
    aResult.Append(result);
    if (aResult.Length() - oldLength != result.Length()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return rv;
}

