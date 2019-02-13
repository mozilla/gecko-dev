/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ImageCacheKey is the key type for the image cache (see imgLoader.h).
 */

#ifndef mozilla_image_src_ImageCacheKey_h
#define mozilla_image_src_ImageCacheKey_h

#include "mozilla/Maybe.h"

class nsIURI;

namespace mozilla {
namespace image {

class ImageURL;

/**
 * An ImageLib cache entry key.
 *
 * We key the cache on the initial URI (before any redirects), with some
 * canonicalization applied. See ComputeHash() for the details.
 */
class ImageCacheKey final
{
public:
  explicit ImageCacheKey(nsIURI* aURI);
  explicit ImageCacheKey(ImageURL* aURI);

  ImageCacheKey(const ImageCacheKey& aOther);
  ImageCacheKey(ImageCacheKey&& aOther);

  bool operator==(const ImageCacheKey& aOther) const;
  uint32_t Hash() const { return mHash; }

  /// A weak pointer to the URI spec for this cache entry. For logging only.
  const char* Spec() const;

  /// Is this cache entry for a chrome image?
  bool IsChrome() const { return mIsChrome; }

private:
  static uint32_t ComputeHash(ImageURL* aURI,
                              const Maybe<uint64_t>& aBlobSerial);

  nsRefPtr<ImageURL> mURI;
  Maybe<uint64_t> mBlobSerial;
  uint32_t mHash;
  bool mIsChrome;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_src_ImageCacheKey_h
