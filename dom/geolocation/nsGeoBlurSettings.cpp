/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeoBlurSettings.h"

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////
nsGeoBlurSettings::nsGeoBlurSettings()
  : mRadiusIndex(GEO_BLUR_RADIUS_INDEX_PRECISE)
  , mCoordsValid(false)
  , mLatitude(0)
  , mLongitude(0)
{
}

int32_t
nsGeoBlurSettings::GetRadius()
{
  if(mRadiusIndex == GEO_BLUR_RADIUS_INDEX_2) {
    return GEO_BLUR_RADIUS_VALUE_FOR_INDEX_2;
  }

  if(mRadiusIndex == GEO_BLUR_RADIUS_INDEX_2) {
    return GEO_BLUR_RADIUS_VALUE_FOR_INDEX_3;
  }

  return GEO_BLUR_RADIUS_VALUE_FOR_INDEX_4;
}

double
nsGeoBlurSettings::GetLatitude()
{
  return mLatitude;
}

double
nsGeoBlurSettings::GetLongitude()
{
  return mLongitude;
}

nsString
nsGeoBlurSettings::GetManifestURL()
{
	return mManifestURL;
}

void
nsGeoBlurSettings::SetManifestURL(nsString aManifestURL)
{
	mManifestURL = aManifestURL;
}

void
nsGeoBlurSettings::SetRadiusIndex(int32_t aRadiusIndex)
{
	mRadiusIndex = aRadiusIndex;
}

void
nsGeoBlurSettings::SetCoords(nsString aCoords)
{
	ClearCoords();

	if(aCoords.IsEmpty()) {
		return;
	}

	uint32_t middle = aCoords.Find(",");
	if(aCoords.CharAt(0) == '@' && middle > 0 && middle < aCoords.Length() - 1) {
		nsString val = aCoords;
		val.Cut(middle, aCoords.Length() - middle);
		val.Cut(0, 1);
		nsresult result;
		mLatitude = val.ToDouble(&result);

		if(result != NS_OK) {
			mLatitude = 0;
			return;
		}

		val = aCoords;
		val.Cut(0, middle + 1);
		mLongitude = val.ToDouble(&result);

		if(result != NS_OK) {
			mLatitude = 0;
			mLongitude = 0;
			return;
		}

		mCoordsValid = true;
	}
}

void
nsGeoBlurSettings::ClearCoords() {
  mCoordsValid = false;

	mLatitude = 0;
	mLongitude = 0;
}

bool
nsGeoBlurSettings::IsExactLoaction()
{
	return mRadiusIndex == GEO_BLUR_RADIUS_INDEX_PRECISE;
}

bool
nsGeoBlurSettings::IsFakeLocation()
{
	return mRadiusIndex == GEO_BLUR_RADIUS_INDEX_CUSTOM;
}

bool
nsGeoBlurSettings::IsBluredLocation()
{
	return mRadiusIndex > GEO_BLUR_RADIUS_INDEX_PRECISE && mRadiusIndex < GEO_BLUR_RADIUS_INDEX_CUSTOM;
}
