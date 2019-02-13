/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Forward declarations to avoid including all of nsStyleStruct.h.
 */

#ifndef nsStyleStructFwd_h_
#define nsStyleStructFwd_h_

enum nsStyleStructID {

/*
 * Define the constants eStyleStruct_Font, etc.
 *
 * The C++ standard, section 7.2, guarantees that enums begin with 0 and
 * increase by 1.
 *
 * We separate the IDs of Reset and Inherited structs so that we can use
 * the IDs as indices (offset by nsStyleStructID_*_Start) into arrays of
 * one type or the other.
 */

nsStyleStructID_None = -1,
nsStyleStructID_Inherited_Start = 0,
// a dummy value so the value after it is the same as ..._Inherited_Start
nsStyleStructID_DUMMY1 = nsStyleStructID_Inherited_Start - 1,

#define STYLE_STRUCT_INHERITED(name, checkdata_cb) \
  eStyleStruct_##name,
#define STYLE_STRUCT_RESET(name, checkdata_cb)
#include "nsStyleStructList.h"
#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

nsStyleStructID_Reset_Start,
// a dummy value so the value after it is the same as ..._Reset_Start
nsStyleStructID_DUMMY2 = nsStyleStructID_Reset_Start - 1,

#define STYLE_STRUCT_RESET(name, checkdata_cb) \
  eStyleStruct_##name,
#define STYLE_STRUCT_INHERITED(name, checkdata_cb)
#include "nsStyleStructList.h"
#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

// one past the end; length of 0-based list
nsStyleStructID_Length,

nsStyleStructID_Inherited_Count =
  nsStyleStructID_Reset_Start - nsStyleStructID_Inherited_Start,
nsStyleStructID_Reset_Count =
  nsStyleStructID_Length - nsStyleStructID_Reset_Start,

// An ID used for properties that are not in style structs.  This is
// used only in some users of nsStyleStructID, such as
// nsCSSProps::kSIDTable, including some that store SIDs in a bitfield,
// such as nsCSSCompressedDataBlock::mStyleBits.
eStyleStruct_BackendOnly = nsStyleStructID_Length

};

// A bit corresponding to each struct ID
#define NS_STYLE_INHERIT_BIT(sid_)        (1 << uint64_t(eStyleStruct_##sid_))

#endif /* nsStyleStructFwd_h_ */
