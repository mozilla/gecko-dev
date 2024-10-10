/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CallbackDebuggerNotification_h
#define mozilla_dom_CallbackDebuggerNotification_h

#include "DebuggerNotification.h"
#include "DebuggerNotificationManager.h"
#include "mozilla/CycleCollectedJSContext.h"

namespace mozilla::dom {

class CallbackDebuggerNotification : public DebuggerNotification {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CallbackDebuggerNotification,
                                           DebuggerNotification)

  CallbackDebuggerNotification(nsIGlobalObject* aDebuggeeGlobal,
                               DebuggerNotificationType aType,
                               CallbackDebuggerNotificationPhase aPhase,
                               nsIGlobalObject* aOwnerGlobal = nullptr)
      : DebuggerNotification(aDebuggeeGlobal, aType, aOwnerGlobal),
        mPhase(aPhase) {}

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<DebuggerNotification> CloneInto(
      nsIGlobalObject* aNewOwner) const override;

  CallbackDebuggerNotificationPhase Phase() const { return mPhase; }

 protected:
  ~CallbackDebuggerNotification() = default;

  CallbackDebuggerNotificationPhase mPhase;
};

class MOZ_RAII CallbackDebuggerNotificationGuard final {
 public:
  MOZ_CAN_RUN_SCRIPT CallbackDebuggerNotificationGuard(
      nsIGlobalObject* aDebuggeeGlobal, DebuggerNotificationType aType)
      : mDebuggeeGlobal(aDebuggeeGlobal), mType(aType) {
    Dispatch(CallbackDebuggerNotificationPhase::Pre);
  }
  CallbackDebuggerNotificationGuard(const CallbackDebuggerNotificationGuard&) =
      delete;
  CallbackDebuggerNotificationGuard(CallbackDebuggerNotificationGuard&&) =
      delete;
  CallbackDebuggerNotificationGuard& operator=(
      const CallbackDebuggerNotificationGuard&) = delete;
  CallbackDebuggerNotificationGuard& operator=(
      CallbackDebuggerNotificationGuard&&) = delete;

  MOZ_CAN_RUN_SCRIPT ~CallbackDebuggerNotificationGuard() {
    Dispatch(CallbackDebuggerNotificationPhase::Post);
  }

 private:
  MOZ_CAN_RUN_SCRIPT void Dispatch(CallbackDebuggerNotificationPhase aPhase) {
#ifdef MOZ_EXECUTION_TRACING
    if (MOZ_UNLIKELY(profiler_is_active())) {
      CycleCollectedJSContext* ccjcx = CycleCollectedJSContext::Get();
      if (ccjcx) {
        const char* typeStr = "";
        switch (mType) {
          case DebuggerNotificationType::SetTimeout:
            typeStr = "setTimeout";
            break;
          case DebuggerNotificationType::ClearTimeout:
            typeStr = "clearTimeout";
            break;
          case DebuggerNotificationType::SetInterval:
            typeStr = "setInterval";
            break;
          case DebuggerNotificationType::ClearInterval:
            typeStr = "clearInterval";
            break;
          case DebuggerNotificationType::RequestAnimationFrame:
            typeStr = "requestAnimationFrame";
            break;
          case DebuggerNotificationType::CancelAnimationFrame:
            typeStr = "cancelAnimationFrame";
            break;
          case DebuggerNotificationType::SetTimeoutCallback:
            typeStr = "setTimeout";
            break;
          case DebuggerNotificationType::SetIntervalCallback:
            typeStr = "setInterval";
            break;
          case DebuggerNotificationType::RequestAnimationFrameCallback:
            typeStr = "requestAnimationFrame";
            break;
          case DebuggerNotificationType::DomEvent:
            MOZ_CRASH("Unreachable");
            break;
        }
        if (aPhase == CallbackDebuggerNotificationPhase::Pre) {
          JS_TracerEnterLabelLatin1(ccjcx->Context(), typeStr);
        } else {
          JS_TracerLeaveLabelLatin1(ccjcx->Context(), typeStr);
        }
      }
    }
#endif

    auto manager = DebuggerNotificationManager::ForDispatch(mDebuggeeGlobal);
    if (MOZ_UNLIKELY(manager)) {
      manager->Dispatch<CallbackDebuggerNotification>(mType, aPhase);
    }
  }

  nsIGlobalObject* mDebuggeeGlobal;
  DebuggerNotificationType mType;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CallbackDebuggerNotification_h
