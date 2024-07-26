/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Perfetto.h"
#include <stdlib.h>

const char* ProfilerCategoryNames[] = {
#define CATEGORY_JSON_BEGIN_CATEGORY(name, labelAsString, color) #name,
#define CATEGORY_JSON_SUBCATEGORY(supercategory, name, labelAsString)
#define CATEGORY_JSON_END_CATEGORY
    MOZ_PROFILING_CATEGORY_LIST(CATEGORY_JSON_BEGIN_CATEGORY,
                                CATEGORY_JSON_SUBCATEGORY,
                                CATEGORY_JSON_END_CATEGORY)
#undef CATEGORY_JSON_BEGIN_CATEGORY
#undef CATEGORY_JSON_SUBCATEGORY
#undef CATEGORY_JSON_END_CATEGORY
};

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

void InitPerfetto() {
  if (!getenv("MOZ_DISABLE_PERFETTO")) {
    perfetto::TracingInitArgs args;
    args.backends |= perfetto::kSystemBackend;
    perfetto::Tracing::Initialize(args);
    perfetto::TrackEvent::Register();
  }
}
