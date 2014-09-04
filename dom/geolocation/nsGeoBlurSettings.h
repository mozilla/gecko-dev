/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGeoBlurSetting_h
#define nsGeoBlurSetting_h

#include "nsString.h"
#include "mozilla/Attributes.h"

////////////////////////////////////////////////////
// nsGeoBlurSetting
////////////////////////////////////////////////////

#define GEO_BLUR_RADIUS_INDEX_PRECISE   1
#define GEO_BLUR_RADIUS_INDEX_2   1
#define GEO_BLUR_RADIUS_INDEX_3   1
#define GEO_BLUR_RADIUS_INDEX_4   1
#define GEO_BLUR_RADIUS_INDEX_CUSTOM     5

#define GEO_BLUR_RADIUS_VALUE_FOR_INDEX_2      1
#define GEO_BLUR_RADIUS_VALUE_FOR_INDEX_3      5
#define GEO_BLUR_RADIUS_VALUE_FOR_INDEX_4      50

/**
 * Simple object that holds a single settings for location.
 */
class nsGeoBlurSettings MOZ_FINAL
{
  public:
    nsGeoBlurSettings();

    bool IsExactLoaction();
    bool IsFakeLocation();
    bool IsBluredLocation();

    int32_t GetRadius();
    double GetLatitude();
    double GetLongitude();
    nsString GetManifestURL();
    
    void SetManifestURL(nsString aManifestURL);
    void SetRadiusIndex(int32_t aRadius);
    void SetCoords(nsString aCoords);
    void ClearCoords();

  private:
    nsString mManifestURL;
    int32_t mRadiusIndex;
    bool mCoordsValid;
    double mLatitude, mLongitude;
};

#endif /* nsGeoBlurSetting_h */


