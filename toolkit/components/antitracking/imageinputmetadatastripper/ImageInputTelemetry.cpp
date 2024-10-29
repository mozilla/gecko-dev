/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageInputTelemetry.h"

#include "mozilla/ContentEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/glean/GleanMetrics.h"
#include "nsIPrincipal.h"

namespace mozilla {

// Known MIME types as listed in nsMimeTypes.h + HEIC types
static constexpr nsLiteralCString kKnownImageMIMETypes[] = {
    "image/gif"_ns,
    "image/jpeg"_ns,
    "image/jpg"_ns,
    "image/pjpeg"_ns,
    "image/png"_ns,
    "image/apng"_ns,
    "image/x-png"_ns,
    "image/x-portable-pixmap"_ns,
    "image/x-xbitmap"_ns,
    "image/x-xbm"_ns,
    "image/xbm"_ns,
    "image/x-jg"_ns,
    "image/tiff"_ns,
    "image/bmp"_ns,
    "image/x-ms-bmp"_ns,
    "image/x-ms-clipboard-bmp"_ns,
    "image/x-icon"_ns,
    "image/vnd.microsoft.icon"_ns,
    "image/icon"_ns,  // "video/x-mng"_ns, - This is checked in
                      // ImageInputTelemetry::IsKnownImageMIMEType()
    "image/x-jng"_ns,
    "image/svg+xml"_ns,
    "image/webp"_ns,
    "image/avif"_ns,
    "image/jxl"_ns,
    // Additionally added HEIC image formats due to mobile popularity
    "image/heic"_ns,
    "image/heif"_ns,
};

/* private static */ bool ImageInputTelemetry::IsKnownImageMIMEType(
    const nsCString& aInputFileType) {
  if (aInputFileType.Find("video/x-mng"_ns) != kNotFound) {
    return true;
  }
  if (aInputFileType.Find("image/"_ns) != kNotFound) {
    for (const nsCString& knownImageMIMEType : kKnownImageMIMETypes) {
      if (aInputFileType.Equals(knownImageMIMEType)) {
        return true;
      }
    }
  }
  return false;
}

/* private static */ bool ImageInputTelemetry::IsContentPrincipal(
    nsIPrincipal* aContentPrincipal) {
  bool isSystemPrincipal;
  aContentPrincipal->GetIsSystemPrincipal(&isSystemPrincipal);
  return !(isSystemPrincipal || aContentPrincipal->SchemeIs("about"));
}

/* private static */ void ImageInputTelemetry::RecordImageInputTelemetry(
    const nsCString& aImageType, ImageInputType aInputType) {
  nsAutoCString inputType;
  switch (aInputType) {
    case ImageInputType::Drop:
      inputType = "Drop"_ns;
      break;
    case ImageInputType::Paste:
      inputType = "Paste"_ns;
      break;
    case ImageInputType::FilePicker:
      inputType = "FilePicker"_ns;
      break;
  }
  glean::image_input_telemetry::ImageInputExtra extra = {
      .imageType = Some(aImageType),
      .inputType = Some(inputType),
  };
  glean::image_input_telemetry::image_input.Record(Some(extra));
}

/* private static */ void ImageInputTelemetry::MaybeRecordImageInputTelemetry(
    ImageInputType aInputType, dom::DataTransfer* aDataTransfer) {
  // Check if input datatransfers contains files.
  RefPtr<dom::FileList> files =
      aDataTransfer->GetFiles(*nsContentUtils::GetSystemPrincipal());
  if (!files) {
    return;
  }

  nsAutoString fileType;
  nsCString fileTypeC;
  for (uint32_t i = 0; i < files->Length(); i++) {
    dom::File* file = files->Item(i);
    if (!file) {
      continue;
    }

    file->GetType(fileType);
    fileTypeC = NS_ConvertUTF16toUTF8(fileType);
    if (IsKnownImageMIMEType(fileTypeC)) {
      ImageInputTelemetry::RecordImageInputTelemetry(fileTypeC, aInputType);
    }
  }
}

/* static */ void ImageInputTelemetry::MaybeRecordDropImageInputTelemetry(
    WidgetDragEvent* aDragEvent, nsIPrincipal* aContentPrincipal) {
  MOZ_ASSERT(aDragEvent);
  MOZ_ASSERT(aContentPrincipal);

  // Only collect telemetry when drag data is accessed on drop.
  if (aDragEvent->mMessage != eDrop || !aDragEvent->mDataTransfer) {
    return;
  }

  // Only process events on content, not about pages, e.g. default drop
  // handler displays dropped file.
  if (!IsContentPrincipal(aContentPrincipal)) {
    return;
  }

  ImageInputTelemetry::MaybeRecordImageInputTelemetry(
      ImageInputType::Drop, aDragEvent->mDataTransfer);
}

/* static */ void ImageInputTelemetry::MaybeRecordPasteImageInputTelemetry(
    InternalClipboardEvent* aClipboardEvent, nsIPrincipal* aContentPrincipal) {
  MOZ_ASSERT(aClipboardEvent);
  MOZ_ASSERT(aContentPrincipal);

  // Only collect telemetry when clipboard data is accessed on paste.
  if (aClipboardEvent->mMessage != ePaste || !aClipboardEvent->mClipboardData) {
    return;
  }

  // Only process events on content, neither system (e.g. URL bar) nor
  // about pages (e.g. searchbar in about:preferences).
  if (!IsContentPrincipal(aContentPrincipal)) {
    return;
  }

  ImageInputTelemetry::MaybeRecordImageInputTelemetry(
      ImageInputType::Paste, aClipboardEvent->mClipboardData);
}

/* static */ void ImageInputTelemetry::MaybeRecordFilePickerImageInputTelemetry(
    dom::Blob* aFilePickerBlob) {
  MOZ_ASSERT(aFilePickerBlob);

  nsAutoString fileType;
  nsCString fileTypeC;
  aFilePickerBlob->GetType(fileType);
  fileTypeC = NS_ConvertUTF16toUTF8(fileType);
  if (IsKnownImageMIMEType(fileTypeC)) {
    ImageInputTelemetry::RecordImageInputTelemetry(fileTypeC,
                                                   ImageInputType::FilePicker);
  }
}

}  // namespace mozilla
