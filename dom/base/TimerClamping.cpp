/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimerClamping.h"

namespace mozilla {

/* static */
double
TimerClamping::ReduceSTimeValue(double aTime)
{
  static const double maxResolutionS = .002;
  return floor(aTime / maxResolutionS) * maxResolutionS;
}

/* static */
double
TimerClamping::ReduceMsTimeValue(double aTime)
{
  static const double maxResolutionMs = 2;
  return floor(aTime / maxResolutionMs) * maxResolutionMs;
}

/* static */
double
TimerClamping::ReduceUsTimeValue(double aTime)
{
  static const double maxResolutionUs = 2000;
  return floor(aTime / maxResolutionUs) * maxResolutionUs;
}

}