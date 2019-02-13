/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SerializedLoadContext_h
#define SerializedLoadContext_h

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"

class nsILoadContext;

/*
 *  This file contains the IPC::SerializedLoadContext class, which is used to
 *  copy data across IPDL from Child process contexts so it is available in the
 *  Parent.
 */

class nsIChannel;
class nsIWebSocketChannel;

namespace IPC {

class SerializedLoadContext
{
public:
  SerializedLoadContext()
  {
    Init(nullptr);
  }

  explicit SerializedLoadContext(nsILoadContext* aLoadContext);
  explicit SerializedLoadContext(nsIChannel* aChannel);
  explicit SerializedLoadContext(nsIWebSocketChannel* aChannel);

  void Init(nsILoadContext* aLoadContext);

  bool IsNotNull() const { return mIsNotNull; }
  bool IsPrivateBitValid() const { return mIsPrivateBitValid; }

  // used to indicate if child-side LoadContext * was null.
  bool mIsNotNull;
  // used to indicate if child-side mUsePrivateBrowsing flag is valid, even if
  // mIsNotNull is false, i.e., child LoadContext was null.
  bool mIsPrivateBitValid;
  bool mIsContent;
  bool mUsePrivateBrowsing;
  bool mUseRemoteTabs;
  bool mIsInBrowserElement;
  uint32_t mAppId;
};

// Function to serialize over IPDL
template<>
struct ParamTraits<SerializedLoadContext>
{
  typedef SerializedLoadContext paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mIsNotNull);
    WriteParam(aMsg, aParam.mIsContent);
    WriteParam(aMsg, aParam.mIsPrivateBitValid);
    WriteParam(aMsg, aParam.mUsePrivateBrowsing);
    WriteParam(aMsg, aParam.mUseRemoteTabs);
    WriteParam(aMsg, aParam.mAppId);
    WriteParam(aMsg, aParam.mIsInBrowserElement);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->mIsNotNull) ||
        !ReadParam(aMsg, aIter, &aResult->mIsContent) ||
        !ReadParam(aMsg, aIter, &aResult->mIsPrivateBitValid) ||
        !ReadParam(aMsg, aIter, &aResult->mUsePrivateBrowsing) ||
        !ReadParam(aMsg, aIter, &aResult->mUseRemoteTabs) ||
        !ReadParam(aMsg, aIter, &aResult->mAppId) ||
        !ReadParam(aMsg, aIter, &aResult->mIsInBrowserElement)) {
      return false;
    }

    return true;
  }
};

} // namespace IPC

#endif // SerializedLoadContext_h
