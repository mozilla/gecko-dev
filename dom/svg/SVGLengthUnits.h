/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how SVG_LENGTH_UNIT is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

SVG_LENGTH_EMPTY_UNIT(SVG_LENGTHTYPE_NUMBER, nsCSSUnit::eCSSUnit_Pixel)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_PX, "px", nsCSSUnit::eCSSUnit_Pixel)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_PERCENTAGE, "%", nsCSSUnit::eCSSUnit_Percent)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_MM, "mm", nsCSSUnit::eCSSUnit_Millimeter)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_CM, "cm", nsCSSUnit::eCSSUnit_Centimeter)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_IN, "in", nsCSSUnit::eCSSUnit_Inch)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_PT, "pt", nsCSSUnit::eCSSUnit_Point)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_PC, "pc", nsCSSUnit::eCSSUnit_Pica)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_EMS, "em", nsCSSUnit::eCSSUnit_EM)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_EXS, "ex", nsCSSUnit::eCSSUnit_XHeight)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_Q, "q", nsCSSUnit::eCSSUnit_Quarter)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_CH, "ch", nsCSSUnit::eCSSUnit_Char)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_REM, "rem", nsCSSUnit::eCSSUnit_RootEM)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_IC, "ic", nsCSSUnit::eCSSUnit_Ideographic)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_CAP, "cap", nsCSSUnit::eCSSUnit_CapHeight)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_VW, "vw", nsCSSUnit::eCSSUnit_VW)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_VH, "vh", nsCSSUnit::eCSSUnit_VH)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_VMIN, "vmin", nsCSSUnit::eCSSUnit_VMin)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_VMAX, "vmax", nsCSSUnit::eCSSUnit_VMax)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_LH, "lh", nsCSSUnit::eCSSUnit_LineHeight)
SVG_LENGTH_UNIT(SVG_LENGTHTYPE_RLH, "rlh", nsCSSUnit::eCSSUnit_RootLineHeight)
