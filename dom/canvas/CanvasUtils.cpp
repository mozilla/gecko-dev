/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdarg.h>

#include "nsICanvasRenderingContextInternal.h"
#include "nsIHTMLCollection.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "nsIPrincipal.h"

#include "nsGfxCIID.h"

#include "nsTArray.h"

#include "CanvasUtils.h"
#include "mozilla/gfx/Matrix.h"
#include "WebGL2Context.h"

#include "nsIScriptError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "jsapi.h"

#define TOPIC_CANVAS_PERMISSIONS_PROMPT "canvas-permissions-prompt"
#define TOPIC_CANVAS_PERMISSIONS_PROMPT_HIDE_DOORHANGER \
  "canvas-permissions-prompt-hide-doorhanger"
#define PERMISSION_CANVAS_EXTRACT_DATA "canvas"_ns

using namespace mozilla::gfx;

static bool IsUnrestrictedPrincipal(nsIPrincipal& aPrincipal) {
  // The system principal can always extract canvas data.
  if (aPrincipal.IsSystemPrincipal()) {
    return true;
  }

  // Allow chrome: and resource: (this especially includes PDF.js)
  if (aPrincipal.SchemeIs("chrome") || aPrincipal.SchemeIs("resource")) {
    return true;
  }

  // Allow extension principals.
  return aPrincipal.GetIsAddonOrExpandedAddonPrincipal();
}

namespace mozilla::CanvasUtils {

class OffscreenCanvasPermissionRunnable final
    : public dom::WorkerMainThreadRunnable {
 public:
  OffscreenCanvasPermissionRunnable(dom::WorkerPrivate* aWorkerPrivate,
                                    nsIPrincipal* aPrincipal)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "OffscreenCanvasPermissionRunnable"_ns),
        mPrincipal(aPrincipal) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    AssertIsOnMainThread();

    mResult = GetCanvasExtractDataPermission(*mPrincipal);
    return true;
  }

  uint32_t GetResult() const { return mResult; }

 private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  uint32_t mResult = nsIPermissionManager::UNKNOWN_ACTION;
};

uint32_t GetCanvasExtractDataPermission(nsIPrincipal& aPrincipal) {
  if (IsUnrestrictedPrincipal(aPrincipal)) {
    return true;
  }

  if (NS_IsMainThread()) {
    nsresult rv;
    nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, nsIPermissionManager::UNKNOWN_ACTION);

    uint32_t permission;
    rv = permissionManager->TestPermissionFromPrincipal(
        &aPrincipal, PERMISSION_CANVAS_EXTRACT_DATA, &permission);
    NS_ENSURE_SUCCESS(rv, nsIPermissionManager::UNKNOWN_ACTION);

    return permission;
  }
  if (auto* workerPrivate = dom::GetCurrentThreadWorkerPrivate()) {
    RefPtr<OffscreenCanvasPermissionRunnable> runnable =
        new OffscreenCanvasPermissionRunnable(workerPrivate, &aPrincipal);
    ErrorResult rv;
    runnable->Dispatch(workerPrivate, dom::WorkerStatus::Canceling, rv);
    if (rv.Failed()) {
      return nsIPermissionManager::UNKNOWN_ACTION;
    }
    return runnable->GetResult();
  }
  return nsIPermissionManager::UNKNOWN_ACTION;
}

/*
┌──────────────────────────────────────────────────────────────────────────┐
│IsImageExtractionAllowed(dom::OffscreenCanvas*, JSContext*, nsIPrincipal&)│
└────────────────────────────────────┬─────────────────────────────────────┘
                                     │
                   ┌─────────────────▼────────────────────┐
 ┌─────No──────────│Any prompt RFP target enabled? See [1]│
 ▼                 └─────────────────┬────────────────────┘
 │                                   │Yes
 │                 ┌─────────────────▼────────┐
 ├─────Yes─────────┤Is unrestricted principal?│
 ▼                 └─────────────────┬────────┘
 │                                   │No
 │                 ┌─────────────────▼────────┐
 │          ┌──No──┤Are third parties blocked?│
 │          │      └─────────────────┬────────┘
 │          │                        │Yes
 │          │      ┌─────────────────▼─────────────┐
 │          │      │Are we in a third-party window?├───────Yes──────────┐
 │          │      └─────────────────┬─────────────┘                    ▼
 │          │                        │No                                │
 │          │      ┌─────────────────▼──┐                               │
 │          └──────►Do we show a prompt?├────────────Yes─┐              │
 │                 └─────────────────┬──┘                ▼              │
 │                                   │No                 │              │
 │                 ┌─────────────────▼─────────────┐     │              │
 │                 │Do we allow reading canvas data│     │              │
 │                 │in response to user input?     ├─No──┤              │
 │                 └─────────────────┬─────────────┘     ▼              │
 │                                   │Yes                │              │
 │                 ┌─────────────────▼─────────┐         │              │
 ├─────Yes─────────┼Are we handling user input?│         │              │
 ▼                 └─────────────────┬─────────┘         │              │
 │                                   │No                 │              │
 │                 ┌─────────────────▼─────────────┐     │              │
┌▼─────┐           │Show Permission Prompt (either ◄─────┘          ┌───▼──┐
│return│           │w/ doorhanger, or w/o depending│                │return│
│true  │           │on User Input)                 ├────────────────►false │
└──────┘           └───────────────────────────────┘                └──────┘
[1]: CanvasImageExtractionPrompt, CanvasExtractionBeforeUserInputIsBlocked,
     CanvasExtractionFromThirdPartiesIsBlocked are the RFP targets mentioned.
 */
bool IsImageExtractionAllowed_impl(
    bool aCanvasImageExtractionPrompt,
    bool aCanvasExtractionBeforeUserInputIsBlocked,
    bool aCanvasExtractionFromThirdPartiesIsBlocked, JSContext* aCx,
    nsIPrincipal& aPrincipal,
    const std::function<bool()>& aGetIsThirdPartyWindow,
    const std::function<void(const nsAutoString&)>& aReportToConsole,
    const std::function<void(bool)>& aTryPrompt) {
  /*
   * There are three RFPTargets that change the behavior here, and they can be
   * in any combination
   * - CanvasImageExtractionPrompt - whether or not to prompt the user for
   * canvas extraction. If enabled, before canvas is extracted we will ensure
   * the user has granted permission.
   * - CanvasExtractionBeforeUserInputIsBlocked - if enabled, canvas extraction
   * before user input has occurred is always blocked, regardless of any other
   * Target behavior
   * - CanvasExtractionFromThirdPartiesIsBlocked - if enabled, canvas extraction
   * by third parties is always blocked, regardless of any other Target behavior
   *
   * There are two odd cases:
   * 1) When CanvasImageExtractionPrompt=false but
   *    CanvasExtractionBeforeUserInputIsBlocked=true Conceptually this is
   *    "Always allow canvas extraction in response to user input, and never
   *     allow it otherwise"
   *
   *    That's fine as a concept, but it might be a little confusing, so we
   *    still want to show the permission icon in the address bar, but never
   *    the permission doorhanger.
   * 2) When CanvasExtractionFromThirdPartiesIsBlocked=false - we will prompt
   *    the user for permission _for the frame_ (maybe with the doorhanger,
   *    maybe not).  The prompt shows the frame's origin, but it's easy to
   *    mistake that for the origin of the top-level page and grant it when you
   *    don't mean to.  This combination isn't likely to be used by anyone
   *    except those opting in, so that's alright.
   */

  if (!aCanvasImageExtractionPrompt &&
      !aCanvasExtractionBeforeUserInputIsBlocked &&
      !aCanvasExtractionFromThirdPartiesIsBlocked) {
    return true;
  }

  // Don't proceed if we don't have a document or JavaScript context.
  if (!aCx) {
    return false;
  }

  if (IsUnrestrictedPrincipal(aPrincipal)) {
    return true;
  }

  Maybe<nsAutoCString> origin = Nothing();
  auto getOrigin = [&]() {
    if (origin.isSome()) {
      return origin->IsEmpty();
    }

    nsAutoCString originResult;
    nsresult rv = aPrincipal.GetOrigin(originResult);
    origin = NS_SUCCEEDED(rv) ? Some(originResult) : Some(""_ns);

    return NS_SUCCEEDED(rv);
  };

  if (aCanvasExtractionFromThirdPartiesIsBlocked) {
    if (aGetIsThirdPartyWindow()) {
      nsAutoString message;
      message.AppendPrintf(
          "Blocked %s third party from extracting canvas data.",
          getOrigin() ? origin->get() : "unknown");
      aReportToConsole(message);
      return false;
    }
  }

  if (!aCanvasImageExtractionPrompt &&
      !aCanvasExtractionBeforeUserInputIsBlocked) {
    return true;
  }

  // -------------------------------------------------------------------
  // Check a site's permission

  // If the user has previously granted or not granted permission, we can return
  // immediately. Load Permission Manager service.
  uint64_t permission = GetCanvasExtractDataPermission(aPrincipal);
  switch (permission) {
    case nsIPermissionManager::ALLOW_ACTION:
      return true;
    case nsIPermissionManager::DENY_ACTION:
      return false;
    default:
      break;
  }

  // -------------------------------------------------------------------
  // At this point, there's only one way to return true: if we are always
  // allowing canvas in response to user input, and not prompting
  bool hidePermissionDoorhanger = false;
  if (!aCanvasImageExtractionPrompt &&
      aCanvasExtractionBeforeUserInputIsBlocked) {
    // If so, see if this is in response to user input.
    if (dom::UserActivation::IsHandlingUserInput()) {
      return true;
    }

    hidePermissionDoorhanger = true;
  }

  // -------------------------------------------------------------------
  // Now we know we're going to block it, and log something to the console,
  // and show some sort of prompt maybe with the doorhanger, maybe not

  hidePermissionDoorhanger |= aCanvasExtractionBeforeUserInputIsBlocked &&
                              !dom::UserActivation::IsHandlingUserInput();

  nsAutoString message;
  message.AppendPrintf("Blocked %s from extracting canvas data",
                       getOrigin() ? origin->get() : "unknown");
  message.AppendPrintf(hidePermissionDoorhanger
                           ? " because no user input was detected"
                           : " but prompting the user.");
  aReportToConsole(message);

  aTryPrompt(hidePermissionDoorhanger);

  return false;
}

bool IsImageExtractionAllowed(dom::Document* aDocument, JSContext* aCx,
                              nsIPrincipal& aPrincipal) {
  if (NS_WARN_IF(!aDocument)) {
    return false;
  }

  bool canvasImageExtractionPrompt = aDocument->ShouldResistFingerprinting(
      RFPTarget::CanvasImageExtractionPrompt);
  bool canvasExtractionBeforeUserInputIsBlocked =
      aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionBeforeUserInputIsBlocked);
  bool canvasExtractionFromThirdPartiesIsBlocked =
      aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionFromThirdPartiesIsBlocked);

  // This part is duplicate but it helps us return faster
  // before we create bunch of lambdas
  if (!canvasImageExtractionPrompt &&
      !canvasExtractionBeforeUserInputIsBlocked &&
      !canvasExtractionFromThirdPartiesIsBlocked) {
    return true;
  }

  auto getIsThirdPartyWindow = [&]() {
    return aDocument->GetWindowContext()
               ? aDocument->GetWindowContext()->GetIsThirdPartyWindow()
               : false;
  };

  auto reportToConsole = [&](const nsAutoString& message) {
    nsContentUtils::ReportToConsoleNonLocalized(
        message, nsIScriptError::warningFlag, "Security"_ns, aDocument);
  };

  auto prompt = [&](bool hidePermissionDoorhanger) {
    nsAutoCString origin;
    nsresult rv = aPrincipal.GetOrigin(origin);
    if (NS_FAILED(rv)) {
      return;
    }

    nsPIDOMWindowOuter* win = aDocument->GetWindow();

    if (XRE_IsContentProcess()) {
      dom::BrowserChild* browserChild = dom::BrowserChild::GetFrom(win);
      if (browserChild) {
        browserChild->SendShowCanvasPermissionPrompt(origin,
                                                     hidePermissionDoorhanger);
      }
    } else {
      nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService();
      if (obs) {
        obs->NotifyObservers(
            win,
            hidePermissionDoorhanger
                ? TOPIC_CANVAS_PERMISSIONS_PROMPT_HIDE_DOORHANGER
                : TOPIC_CANVAS_PERMISSIONS_PROMPT,
            NS_ConvertUTF8toUTF16(origin).get());
      }
    }
  };

  return IsImageExtractionAllowed_impl(
      canvasImageExtractionPrompt, canvasExtractionBeforeUserInputIsBlocked,
      canvasExtractionFromThirdPartiesIsBlocked, aCx, aPrincipal,
      getIsThirdPartyWindow, reportToConsole, prompt);
}

ImageExtraction ImageExtractionResult(dom::HTMLCanvasElement* aCanvasElement,
                                      JSContext* aCx,
                                      nsIPrincipal& aPrincipal) {
  if (IsUnrestrictedPrincipal(aPrincipal)) {
    return ImageExtraction::Unrestricted;
  }

  nsCOMPtr<dom::Document> ownerDoc = aCanvasElement->OwnerDoc();
  if (!IsImageExtractionAllowed(ownerDoc, aCx, aPrincipal)) {
    return ImageExtraction::Placeholder;
  }

  if (ownerDoc->ShouldResistFingerprinting(RFPTarget::CanvasRandomization)) {
    return ImageExtraction::Randomize;
  }

  return ImageExtraction::Unrestricted;
}

bool IsImageExtractionAllowed(dom::OffscreenCanvas* aOffscreenCanvas,
                              JSContext* aCx, nsIPrincipal& aPrincipal) {
  if (!aOffscreenCanvas) {
    return false;
  }

  bool canvasImageExtractionPrompt =
      aOffscreenCanvas->ShouldResistFingerprinting(
          RFPTarget::CanvasImageExtractionPrompt);
  bool canvasExtractionBeforeUserInputIsBlocked =
      aOffscreenCanvas->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionBeforeUserInputIsBlocked);
  bool canvasExtractionFromThirdPartiesIsBlocked =
      aOffscreenCanvas->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionFromThirdPartiesIsBlocked);

  // This part is duplicate but it helps us return faster
  // before we create bunch of lambdas
  if (!canvasImageExtractionPrompt &&
      !canvasExtractionBeforeUserInputIsBlocked &&
      !canvasExtractionFromThirdPartiesIsBlocked) {
    return true;
  }

  Maybe<uint64_t> winId = aOffscreenCanvas->GetWindowID();
  if (winId.isSome() && *winId == UINT64_MAX) {
    // Workers with no window return UINT64_MAX as their window ID.
    winId = Nothing();
  }

  RefPtr<dom::WindowContext> win;
  if (winId.isSome()) {
    win = dom::WindowGlobalParent::GetById(*winId);
    if (!win) {
      winId = Nothing();
    }
  }

  auto getIsThirdPartyWindow = [&]() {
    if (!win) {
      return false;
    }

    return win->GetIsThirdPartyWindow();
  };

  auto reportToConsole = [&](const nsAutoString& message) {
    if (winId.isNothing()) {
      return;
    }

    nsContentUtils::ReportToConsoleByWindowID(
        message, nsIScriptError::warningFlag, "Security"_ns, *winId);
  };

  nsAutoCString origin;
  nsresult rv = aPrincipal.GetOrigin(origin);
  if (NS_FAILED(rv)) {
    origin = ""_ns;
  }

  auto prompt = [=](bool hidePermissionDoorhanger) {
    if (origin.IsEmpty()) {
      return;
    }

    NS_DispatchToMainThread(
        NS_NewRunnableFunction("IsImageExtractionAllowedOffscreen", [=]() {
          if (XRE_IsContentProcess()) {
            if (!win || !win->GetExtantDoc() ||
                !win->GetExtantDoc()->GetWindow()) {
              return;
            }

            dom::BrowserChild* browserChild =
                dom::BrowserChild::GetFrom(win->GetExtantDoc()->GetWindow());

            if (browserChild) {
              browserChild->SendShowCanvasPermissionPrompt(
                  origin, hidePermissionDoorhanger);
            }
          } else {
            nsCOMPtr<nsIObserverService> obs =
                mozilla::services::GetObserverService();
            if (obs) {
              obs->NotifyObservers(
                  win,
                  hidePermissionDoorhanger
                      ? TOPIC_CANVAS_PERMISSIONS_PROMPT_HIDE_DOORHANGER
                      : TOPIC_CANVAS_PERMISSIONS_PROMPT,
                  NS_ConvertUTF8toUTF16(origin).get());
            }
          }
        }));
  };

  return IsImageExtractionAllowed_impl(
      canvasImageExtractionPrompt, canvasExtractionBeforeUserInputIsBlocked,
      canvasExtractionFromThirdPartiesIsBlocked, aCx, aPrincipal,
      getIsThirdPartyWindow, reportToConsole, prompt);
}

ImageExtraction ImageExtractionResult(dom::OffscreenCanvas* aOffscreenCanvas,
                                      JSContext* aCx,
                                      nsIPrincipal& aPrincipal) {
  if (IsUnrestrictedPrincipal(aPrincipal)) {
    return ImageExtraction::Unrestricted;
  }

  if (!IsImageExtractionAllowed(aOffscreenCanvas, aCx, aPrincipal)) {
    return ImageExtraction::Placeholder;
  }

  if (aOffscreenCanvas->ShouldResistFingerprinting(
          RFPTarget::CanvasRandomization)) {
    if (GetCanvasExtractDataPermission(aPrincipal) ==
        nsIPermissionManager::ALLOW_ACTION) {
      return ImageExtraction::Unrestricted;
    }
    return ImageExtraction::Randomize;
  }

  return ImageExtraction::Unrestricted;
}

bool GetCanvasContextType(const nsAString& str,
                          dom::CanvasContextType* const out_type) {
  if (str.EqualsLiteral("2d")) {
    *out_type = dom::CanvasContextType::Canvas2D;
    return true;
  }

  if (str.EqualsLiteral("webgl") || str.EqualsLiteral("experimental-webgl")) {
    *out_type = dom::CanvasContextType::WebGL1;
    return true;
  }

  if (StaticPrefs::webgl_enable_webgl2()) {
    if (str.EqualsLiteral("webgl2")) {
      *out_type = dom::CanvasContextType::WebGL2;
      return true;
    }
  }

  if (gfxVars::AllowWebGPU()) {
    if (str.EqualsLiteral("webgpu")) {
      *out_type = dom::CanvasContextType::WebGPU;
      return true;
    }
  }

  if (str.EqualsLiteral("bitmaprenderer")) {
    *out_type = dom::CanvasContextType::ImageBitmap;
    return true;
  }

  return false;
}

/**
 * This security check utility might be called from an source that never taints
 * others. For example, while painting a CanvasPattern, which is created from an
 * ImageBitmap, onto a canvas. In this case, the caller could set the CORSUsed
 * true in order to pass this check and leave the aPrincipal to be a nullptr
 * since the aPrincipal is not going to be used.
 */
void DoDrawImageSecurityCheck(dom::HTMLCanvasElement* aCanvasElement,
                              nsIPrincipal* aPrincipal, bool forceWriteOnly,
                              bool CORSUsed) {
  // Callers should ensure that mCanvasElement is non-null before calling this
  if (!aCanvasElement) {
    NS_WARNING("DoDrawImageSecurityCheck called without canvas element!");
    return;
  }

  if (aCanvasElement->IsWriteOnly() && !aCanvasElement->mExpandedReader) {
    return;
  }

  // If we explicitly set WriteOnly just do it and get out
  if (forceWriteOnly) {
    aCanvasElement->SetWriteOnly();
    return;
  }

  // No need to do a security check if the image used CORS for the load
  if (CORSUsed) return;

  if (NS_WARN_IF(!aPrincipal)) {
    MOZ_ASSERT_UNREACHABLE("Must have a principal here");
    aCanvasElement->SetWriteOnly();
    return;
  }

  if (aCanvasElement->NodePrincipal()->Subsumes(aPrincipal)) {
    // This canvas has access to that image anyway
    return;
  }

  if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
    // This is a resource from an extension content script principal.

    if (aCanvasElement->mExpandedReader &&
        aCanvasElement->mExpandedReader->Subsumes(aPrincipal)) {
      // This canvas already allows reading from this principal.
      return;
    }

    if (!aCanvasElement->mExpandedReader) {
      // Allow future reads from this same princial only.
      aCanvasElement->SetWriteOnly(aPrincipal);
      return;
    }

    // If we got here, this must be the *second* extension tainting
    // the canvas.  Fall through to mark it WriteOnly for everyone.
  }

  aCanvasElement->SetWriteOnly();
}

/**
 * This security check utility might be called from an source that never taints
 * others. For example, while painting a CanvasPattern, which is created from an
 * ImageBitmap, onto a canvas. In this case, the caller could set the aCORSUsed
 * true in order to pass this check and leave the aPrincipal to be a nullptr
 * since the aPrincipal is not going to be used.
 */
void DoDrawImageSecurityCheck(dom::OffscreenCanvas* aOffscreenCanvas,
                              nsIPrincipal* aPrincipal, bool aForceWriteOnly,
                              bool aCORSUsed) {
  // Callers should ensure that mCanvasElement is non-null before calling this
  if (NS_WARN_IF(!aOffscreenCanvas)) {
    return;
  }

  nsIPrincipal* expandedReader = aOffscreenCanvas->GetExpandedReader();
  if (aOffscreenCanvas->IsWriteOnly() && !expandedReader) {
    return;
  }

  // If we explicitly set WriteOnly just do it and get out
  if (aForceWriteOnly) {
    aOffscreenCanvas->SetWriteOnly();
    return;
  }

  // No need to do a security check if the image used CORS for the load
  if (aCORSUsed) {
    return;
  }

  // If we are on a worker thread, we might not have any principals at all.
  nsIGlobalObject* global = aOffscreenCanvas->GetOwnerGlobal();
  nsIPrincipal* canvasPrincipal = global ? global->PrincipalOrNull() : nullptr;
  if (!aPrincipal || !canvasPrincipal) {
    aOffscreenCanvas->SetWriteOnly();
    return;
  }

  if (canvasPrincipal->Subsumes(aPrincipal)) {
    // This canvas has access to that image anyway
    return;
  }

  if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
    // This is a resource from an extension content script principal.

    if (expandedReader && expandedReader->Subsumes(aPrincipal)) {
      // This canvas already allows reading from this principal.
      return;
    }

    if (!expandedReader) {
      // Allow future reads from this same princial only.
      aOffscreenCanvas->SetWriteOnly(aPrincipal);
      return;
    }

    // If we got here, this must be the *second* extension tainting
    // the canvas.  Fall through to mark it WriteOnly for everyone.
  }

  aOffscreenCanvas->SetWriteOnly();
}

bool CoerceDouble(const JS::Value& v, double* d) {
  if (v.isDouble()) {
    *d = v.toDouble();
  } else if (v.isInt32()) {
    *d = double(v.toInt32());
  } else if (v.isUndefined()) {
    *d = 0.0;
  } else {
    return false;
  }
  return true;
}

bool HasDrawWindowPrivilege(JSContext* aCx, JSObject* /* unused */) {
  return nsContentUtils::CallerHasPermission(aCx,
                                             nsGkAtoms::all_urlsPermission);
}

bool CheckWriteOnlySecurity(bool aCORSUsed, nsIPrincipal* aPrincipal,
                            bool aHadCrossOriginRedirects) {
  if (!aPrincipal) {
    return true;
  }

  if (!aCORSUsed) {
    if (aHadCrossOriginRedirects) {
      return true;
    }

    nsIGlobalObject* incumbentSettingsObject = dom::GetIncumbentGlobal();
    if (!incumbentSettingsObject) {
      return true;
    }

    nsIPrincipal* principal = incumbentSettingsObject->PrincipalOrNull();
    if (NS_WARN_IF(!principal) || !(principal->Subsumes(aPrincipal))) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla::CanvasUtils
