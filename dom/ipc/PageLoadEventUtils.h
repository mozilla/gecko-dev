/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_page_load_event_utils_h__
#define mozilla_dom_page_load_event_utils_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/PageloadEvent.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::performance::pageload_event::PageloadEventData> {
  typedef mozilla::performance::pageload_event::PageloadEventData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
#define WRITE_METRIC_PARAM(name, type) WriteParam(aWriter, aParam.name);
    FOR_EACH_PAGELOAD_METRIC(WRITE_METRIC_PARAM)
#undef WRITE_METRIC_PARAM
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool ok = true;
#define READ_METRIC_PARAM(name, type) \
  ok = ok && ReadParam(aReader, &aResult->name);
    FOR_EACH_PAGELOAD_METRIC(READ_METRIC_PARAM)
#undef READ_METRIC_PARAM
    return ok;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_page_load_event_utils_h__
