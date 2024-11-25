/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/OwnedRustBuffer.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/UniFFIBinding.h"
#include "mozilla/dom/UniFFICallbacks.h"
#include "mozilla/Maybe.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::uniffi {
extern mozilla::LazyLogModule gUniffiLogger;

void UniffiCallbackMethodHandlerBase::FireAndForget(
    UniquePtr<UniffiCallbackMethodHandlerBase> aHandler,
    StaticRefPtr<dom::UniFFICallbackHandler>* aJsHandler) {
  nsresult dispatchResult = NS_DispatchToMainThread(NS_NewRunnableFunction(
      "UniFFI callback", [handler = std::move(aHandler),
                          aJsHandler]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
        // Take our own reference to the callback handler to ensure that it
        // stays alive for the duration of this call
        RefPtr<dom::UniFFICallbackHandler> jsHandler = *aJsHandler;
        if (!jsHandler) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error,
                  ("[UniFFI] %s called, but JS handler not registered",
                   handler->mInterfaceName));
          return;
        }

        JSObject* global = jsHandler->CallbackGlobalOrNull();
        if (!global) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error,
                  ("[UniFFI] JS handler for %s has null global",
                   handler->mInterfaceName));
          return;
        }

        dom::AutoEntryScript aes(global, handler->mInterfaceName);

        IgnoredErrorResult error;
        handler->MakeCall(aes.cx(), jsHandler, error);

        if (error.Failed()) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error,
                  ("[UniFFI] Error invoking JS handler for %s",
                   handler->mInterfaceName));
          return;
        }
      }));

  if (NS_FAILED(dispatchResult)) {
    MOZ_LOG(gUniffiLogger, LogLevel::Error,
            ("[UniFFI] Error dispatching UniFFI callback task"));
  }
}

void UniffiCallbackFreeHandler::MakeCall(JSContext* aCx,
                                         dom::UniFFICallbackHandler* aJsHandler,
                                         ErrorResult& aError) {
  aJsHandler->Destroy(mObjectHandle, aError);
}

}  // namespace mozilla::uniffi
