/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Nyx.h"
#include "mozilla/dom/TypedArray.h"

namespace mozilla::dom {

/* static */
void Nyx::Log(const GlobalObject&, const nsAString& aMsg) {
  NS_ConvertUTF16toUTF8 cStr(aMsg);
  MOZ_FUZZING_NYX_PRINTF("%s\n", cStr.get());
}

/* static */
bool Nyx::IsEnabled(const GlobalObject&, const nsAString& aFuzzerName) {
  return fuzzing::Nyx::instance().is_enabled(
      NS_ConvertUTF16toUTF8(aFuzzerName).get());
}

/* static */
bool Nyx::IsReplay(const GlobalObject&) {
#ifdef FUZZING_SNAPSHOT
  return fuzzing::Nyx::instance().is_replay();
#endif
  return false;
}

/* static */
bool Nyx::IsStarted(const GlobalObject&) {
  return fuzzing::Nyx::instance().started();
}

/* static */
void Nyx::Start(const GlobalObject&) {
  MOZ_FUZZING_NYX_PRINT("INFO: Performing snapshot...\n");
  fuzzing::Nyx::instance().start();
}

/* static */
void Nyx::Release(const GlobalObject&, uint32_t aIterations) {
  MOZ_FUZZING_NYX_PRINT("INFO: Performing snapshot...\n");
  fuzzing::Nyx::instance().release(aIterations);
}

/* static */
void Nyx::GetRawData(const GlobalObject& aGlobal,
                     JS::MutableHandle<JSObject*> aRetval, ErrorResult& aRv) {
  const size_t maxMsgSize = 4096;

  FallibleTArray<uint8_t> data;

  // Allocate memory for the buffer
  if (!data.SetLength(maxMsgSize, fallible)) {
    MOZ_FUZZING_NYX_ABORT("ERROR: Failed to initialize buffer!\n");
  }

  // Retrieve raw data into the buffer
  uint32_t bufsize =
      fuzzing::Nyx::instance().get_raw_data(data.Elements(), data.Length());

  if (!data.SetLength(bufsize, fallible)) {
    MOZ_FUZZING_NYX_ABORT("ERROR: Failed to resize buffer!\n");
  }

  if (bufsize == 0xFFFFFFFF) {
    MOZ_FUZZING_NYX_DEBUG("Nyx: Out of data.\n");
    fuzzing::Nyx::instance().release(0);
  }

  JS::Rooted<JSObject*> buffer(
      aGlobal.Context(), ArrayBuffer::Create(aGlobal.Context(), data, aRv));
  if (aRv.Failed()) {
    return;
  }
  aRetval.set(buffer);
}

}  // namespace mozilla::dom
