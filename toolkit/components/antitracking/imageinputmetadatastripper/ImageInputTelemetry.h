/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_imageinputtypetelemetry_h
#define mozilla_imageinputtypetelemetry_h

#include "nsStringFwd.h"

class nsIPrincipal;

namespace mozilla {
class InternalClipboardEvent;
class WidgetDragEvent;
namespace dom {
class Blob;
class DataTransfer;
}  // namespace dom

// Class for collection of image input file type and input type telemetry.
class ImageInputTelemetry {
 public:
  enum ImageInputType {
    Drop,
    Paste,
    FilePicker,
  };

  // Collect telemetry on drop of user image input to the browser. This is
  // collected in DragEvent:GetDataTransfer(). Telemetry is not collected
  // for about:* pages (e.g. newtab) and for system principals (e.g. drop on
  // about:preferences).
  static void MaybeRecordDropImageInputTelemetry(
      WidgetDragEvent* aDragEvent, nsIPrincipal* aContentPrincipal);
  // Collect telemetry on paste of user image input to the browser. This is
  // collected in ClipboardEvent:GetClipboardData(). Telemetry is not collected
  // for about:* pages (e.g. searchbar in about:preferences) and for system
  // principals (e.g. paste to urlbar).
  static void MaybeRecordPasteImageInputTelemetry(
      InternalClipboardEvent* aClipboardEvent, nsIPrincipal* aContentPrincipal);
  // Collect telemetry on user selection of image files through the FilePicker.
  // The telemetry is collected in
  // HTMLInputElement::nsFilePickerShownCallback::Done(), both for single and
  // multiple collected files.
  static void MaybeRecordFilePickerImageInputTelemetry(
      dom::Blob* aFilePickerBlob);

 private:
  ImageInputTelemetry() = default;
  ~ImageInputTelemetry() = default;

  // This function returns true if kKnownImageMIMETypes contains aFileInputType.
  static bool IsKnownImageMIMEType(const nsCString& aInputFileType);
  // This function returns true if aContentPincipal is of any page possibly
  // collectiing user data (including local e.g. html pages).
  static bool IsContentPrincipal(nsIPrincipal* aContentPrincipal);
  // Iterate through input files from aDataTransfer and record a Glean event if
  // the file is an image of a type contained in kKnownImageMIMETypes.
  static void MaybeRecordImageInputTelemetry(ImageInputType aInputType,
                                             dom::DataTransfer* aDataTransfer);
  // Wrapper for recording an image_input Glean event containing the image_type
  // and the input_type (FilePicker, Paste, Drag).
  static void RecordImageInputTelemetry(const nsCString& aImageType,
                                        ImageInputType aInputType);
};

}  // namespace mozilla

#endif  // mozilla_imageinputtypetelemetry_h
