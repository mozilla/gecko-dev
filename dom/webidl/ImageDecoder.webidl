/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webcodecs/#image-decoding
 */

// Bug 1696216: Should be AllowSharedBufferSource or ReadableStream
typedef ([AllowShared] ArrayBufferView or [AllowShared] ArrayBuffer or ReadableStream) ImageBufferSource;
dictionary ImageDecoderInit {
  required DOMString type;
  required ImageBufferSource data;
  ColorSpaceConversion colorSpaceConversion = "default";
  [EnforceRange] unsigned long desiredWidth;
  [EnforceRange] unsigned long desiredHeight;
  boolean preferAnimation;
  sequence<ArrayBuffer> transfer = [];
};

dictionary ImageDecodeOptions {
  [EnforceRange] unsigned long frameIndex = 0;
  boolean completeFramesOnly = true;
};

dictionary ImageDecodeResult {
  required VideoFrame image;
  required boolean complete;
};

[Exposed=(Window,DedicatedWorker),
 SecureContext,
 Func="mozilla::dom::ImageDecoder::PrefEnabled"]
interface ImageTrack {
  readonly attribute boolean animated;
  readonly attribute unsigned long frameCount;
  readonly attribute unrestricted float repetitionCount;
  attribute boolean selected;
};

[Exposed=(Window,DedicatedWorker),
 SecureContext,
 Func="mozilla::dom::ImageDecoder::PrefEnabled"]
interface ImageTrackList {
  getter ImageTrack (unsigned long index);

  readonly attribute Promise<undefined> ready;
  readonly attribute unsigned long length;
  readonly attribute long selectedIndex;
  readonly attribute ImageTrack? selectedTrack;
};

[Exposed=(Window,DedicatedWorker),
 SecureContext,
 Func="mozilla::dom::ImageDecoder::PrefEnabled"]
interface ImageDecoder {
  [Throws]
  constructor(ImageDecoderInit init);

  readonly attribute DOMString type;
  readonly attribute boolean complete;
  readonly attribute Promise<undefined> completed;
  readonly attribute ImageTrackList tracks;

  [Throws]
  Promise<ImageDecodeResult> decode(optional ImageDecodeOptions options = {});
  undefined reset();
  undefined close();

  [Throws]
  static Promise<boolean> isTypeSupported(DOMString type);
};
