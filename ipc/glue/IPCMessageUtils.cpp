/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCMessageUtils.h"
#include "mozilla/CheckedInt.h"

namespace IPC {

bool ByteLengthIsValid(uint32_t aNumElements, size_t aElementSize,
                       int* aByteLength) {
  auto length = mozilla::CheckedInt<int>(aNumElements) * aElementSize;
  if (!length.isValid()) {
    return false;
  }
  *aByteLength = length.value();
  return true;
}

void ParamTraits<JSStructuredCloneData>::Write(Message* aMsg, const paramType& aParam) {
  MOZ_ASSERT(!(aParam.Size() % sizeof(uint64_t)));
  WriteParam(aMsg, aParam.Size());

  // Structured clone data can differ when replaying due to bugs. This can
  // affect the size of the IPDL messages we send, which will cause us to
  // crash when the recorded sendmsg calls are returning size information
  // for the messages sent while recording instead of the messages sent
  // while replaying. For now we paper over this by making sure we send
  // structured clone data with a length consistent with what happened
  // while recording, padding or truncating the buffer as necessary.

  Vector<char> allData;
  aParam.ForEachDataChunk([&](const char* aData, size_t aSize) {
    if (!allData.append(aData, aSize)) {
      MOZ_CRASH("ParamTraits<JSStructuredCloneData>::Write");
    }
    return true;
  });
  MOZ_RELEASE_ASSERT(allData.length() == aParam.Size());

  size_t recordedSize = mozilla::recordreplay::RecordReplayValue(
    "WriteStructuredCloneData", allData.length()
  );

  if (allData.length() != recordedSize) {
    if (!allData.resize(recordedSize)) {
      MOZ_CRASH("ParamTraits<JSStructuredCloneData>::Write");
    }
  }

  if (allData.length()) {
    (void) aMsg->WriteBytes(&allData[0], allData.length(), sizeof(uint64_t));
  }
}

bool ParamTraits<JSStructuredCloneData>::Read(const Message* aMsg, PickleIterator* aIter,
                                              paramType* aResult) {
  size_t length = 0;
  if (!ReadParam(aMsg, aIter, &length)) {
    return false;
  }
  MOZ_ASSERT(!(length % sizeof(uint64_t)));

  mozilla::BufferList<InfallibleAllocPolicy> buffers(0, 0, 4096);

  // Borrowing is not suitable to use for IPC to hand out data
  // because we often want to store the data somewhere for
  // processing after IPC has released the underlying buffers. One
  // case is PContentChild::SendGetXPCOMProcessAttributes. We can't
  // return a borrowed buffer because the out param outlives the
  // IPDL callback.
  if (length &&
      !aMsg->ExtractBuffers(aIter, length, &buffers, sizeof(uint64_t))) {
    return false;
  }

  bool success;
  mozilla::BufferList<js::SystemAllocPolicy> out =
      buffers.MoveFallible<js::SystemAllocPolicy>(&success);
  if (!success) {
    return false;
  }

  *aResult = JSStructuredCloneData(
      std::move(out), JS::StructuredCloneScope::DifferentProcess);

  return true;
}

}  // namespace IPC
