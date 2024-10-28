/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PermissionUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIPermissionManager.h"

namespace mozilla::dom {

static const nsLiteralCString kPermissionTypes[] = {
    // clang-format off
    "geo"_ns,
    "desktop-notification"_ns,
    // Alias `push` to `desktop-notification`.
    "desktop-notification"_ns,
    "persistent-storage"_ns,
    // "midi" is the only public permission but internally we have both "midi"
    // and "midi-sysex" (and yes, this is confusing).
    "midi"_ns,
    "storage-access"_ns,
    "screen-wake-lock"_ns,
    "camera"_ns,
    "microphone"_ns
    // clang-format on
};

const size_t kPermissionNameCount = ContiguousEnumSize<PermissionName>::value;

static_assert(std::size(kPermissionTypes) == kPermissionNameCount,
              "kPermissionTypes and PermissionName count should match");

const nsLiteralCString& PermissionNameToType(PermissionName aName) {
  MOZ_ASSERT((size_t)aName < std::size(kPermissionTypes));
  return kPermissionTypes[static_cast<size_t>(aName)];
}

Maybe<PermissionName> TypeToPermissionName(const nsACString& aType) {
  // Annoyingly, "midi-sysex" is an internal permission. The public permission
  // name is "midi" so we have to special-case it here...
  if (aType.Equals("midi-sysex"_ns)) {
    return Some(PermissionName::Midi);
  }

  // "storage-access" permissions are also annoying and require a special case.
  if (StringBeginsWith(aType, "3rdPartyStorage^"_ns) ||
      StringBeginsWith(aType, "3rdPartyFrameStorage^"_ns)) {
    return Some(PermissionName::Storage_access);
  }

  for (size_t i = 0; i < std::size(kPermissionTypes); ++i) {
    if (kPermissionTypes[i].Equals(aType)) {
      return Some(static_cast<PermissionName>(i));
    }
  }

  return Nothing();
}

PermissionState ActionToPermissionState(uint32_t aAction, PermissionName aName,
                                        nsIGlobalObject* aGlobal) {
  MOZ_ASSERT(aGlobal);

  switch (aAction) {
    case nsIPermissionManager::ALLOW_ACTION:
      return PermissionState::Granted;

    case nsIPermissionManager::DENY_ACTION:
      return PermissionState::Denied;

    case nsIPermissionManager::PROMPT_ACTION:
      if ((aName == PermissionName::Camera ||
           aName == PermissionName::Microphone) &&
          !aGlobal->ShouldResistFingerprinting(RFPTarget::MediaDevices)) {
        // A persisted PROMPT_ACTION means the user chose "Always Ask"
        // which shows as "granted" to prevent websites from priming the
        // user to escalate permission any further.
        // Revisit if https://github.com/w3c/permissions/issues/414 reopens.
        //
        // This feature is not offered in resist-fingerprinting mode.
        return PermissionState::Granted;
      }
      return PermissionState::Prompt;

    default:
      return PermissionState::Prompt;
  }
}

}  // namespace mozilla::dom
