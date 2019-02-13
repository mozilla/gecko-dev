/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundutils_h__
#define mozilla_ipc_backgroundutils_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"
#include "nsCOMPtr.h"
#include "nscore.h"

class nsIPrincipal;

namespace IPC {

template<>
struct ParamTraits<mozilla::OriginAttributes>
{
  typedef mozilla::OriginAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    nsAutoCString suffix;
    aParam.CreateSuffix(suffix);
    WriteParam(aMsg, suffix);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsAutoCString suffix;
    return ReadParam(aMsg, aIter, &suffix) &&
           aResult->PopulateFromSuffix(suffix);
  }
};

} // IPC namespace

namespace mozilla {
namespace net {
class LoadInfoArgs;
}

namespace ipc {

class PrincipalInfo;

/**
 * Convert a PrincipalInfo to an nsIPrincipal.
 *
 * MUST be called on the main thread only.
 */
already_AddRefed<nsIPrincipal>
PrincipalInfoToPrincipal(const PrincipalInfo& aPrincipalInfo,
                         nsresult* aOptionalResult = nullptr);

/**
 * Convert an nsIPrincipal to a PrincipalInfo.
 *
 * MUST be called on the main thread only.
 */
nsresult
PrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                         PrincipalInfo* aPrincipalInfo);

/**
 * Convert a LoadInfo to LoadInfoArgs struct.
 */
nsresult
LoadInfoToLoadInfoArgs(nsILoadInfo *aLoadInfo,
                       mozilla::net::LoadInfoArgs* outLoadInfoArgs);

/**
 * Convert LoadInfoArgs to a LoadInfo.
 */
nsresult
LoadInfoArgsToLoadInfo(const mozilla::net::LoadInfoArgs& aLoadInfoArgs,
                       nsILoadInfo** outLoadInfo);



} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundutils_h__
