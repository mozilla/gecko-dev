/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AlertNotification.h"

#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "imgIRequest.h"
#include "imgLoader.h"
#include "nsAlertsUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(AlertNotification, nsIAlertNotification)

AlertNotification::AlertNotification()
    : mTextClickable(false), mInPrivateBrowsing(false) {}

AlertNotification::~AlertNotification() = default;

NS_IMETHODIMP
AlertNotification::Init(const nsAString& aName, const nsAString& aImageURL,
                        const nsAString& aTitle, const nsAString& aText,
                        bool aTextClickable, const nsAString& aCookie,
                        const nsAString& aDir, const nsAString& aLang,
                        const nsAString& aData, nsIPrincipal* aPrincipal,
                        bool aInPrivateBrowsing, bool aRequireInteraction,
                        bool aSilent, const nsTArray<uint32_t>& aVibrate) {
  if (!mId.IsEmpty()) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mName = aName;
  mImageURL = aImageURL;
  mTitle = aTitle;
  mText = aText;
  mTextClickable = aTextClickable;
  mCookie = aCookie;
  mDir = aDir;
  mLang = aLang;
  mData = aData;
  mPrincipal = aPrincipal;
  mInPrivateBrowsing = aInPrivateBrowsing;
  mRequireInteraction = aRequireInteraction;
  mSilent = aSilent;
  mVibrate = aVibrate.Clone();

  return InitId();
}

nsresult AlertNotification::InitId() {
  nsAutoString id;

  // Multiple profiles might overwrite each other's toast messages when a
  // common name is used for a given origin. We prevent this by including
  // the profile directory as part of the toast hash.
  nsCOMPtr<nsIFile> profDir;
  MOZ_TRY(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                 getter_AddRefs(profDir)));
  MOZ_TRY(profDir->Normalize());
  MOZ_TRY(profDir->GetPath(id));

  if (mPrincipal && mPrincipal->GetIsContentPrincipal()) {
    // Notification originated from a web notification.
    nsAutoCString origin;
    MOZ_TRY(mPrincipal->GetOrigin(origin));
    id += NS_ConvertUTF8toUTF16(origin);
  } else {
    id += u"chrome";
  }

  if (mName.IsEmpty()) {
    // No associated name, append a UUID to prevent reuse of the same tag.
    nsIDToCString uuidString(nsID::GenerateUUID());
    size_t len = strlen(uuidString.get());
    MOZ_ASSERT(len == NSID_LENGTH - 1);
    nsAutoString uuid;
    CopyASCIItoUTF16(nsDependentCSubstring(uuidString.get(), len), uuid);

    id += u"#notag:"_ns;
    id += uuid;
  } else {
    id += u"#tag:"_ns;
    id += mName;
  }

  // Windows notification tags are limited to 16 characters, or 64 characters
  // after the Creators Update; therefore we hash the tag to fit the minimum
  // range.
  HashNumber hash = HashString(id);
  mId.AppendPrintf("%010u", hash);
  return NS_OK;
}

NS_IMETHODIMP AlertNotification::GetId(nsAString& aId) {
  if (mId.IsEmpty()) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::SetActions(
    const nsTArray<RefPtr<nsIAlertAction>>& aActions) {
  mActions = aActions.Clone();
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetName(nsAString& aName) {
  if (mPrincipal && mPrincipal->GetIsContentPrincipal()) {
    // mName is no longer unique, but there has been a long assumption
    // throughout the codebase that GetName will be unique. So we return mId for
    // GetName for web triggered notifications to keep uniqueness without
    // accidentially causing subtle breakage in other modules.
    aName = mId;
  } else {
    // System callers has always been expected to provide unique names
    // themselves, so it's fine to return mName as is.
    aName = mName;
  }
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetImageURL(nsAString& aImageURL) {
  aImageURL = mImageURL;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetTitle(nsAString& aTitle) {
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetText(nsAString& aText) {
  aText = mText;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetTextClickable(bool* aTextClickable) {
  *aTextClickable = mTextClickable;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetCookie(nsAString& aCookie) {
  aCookie = mCookie;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetDir(nsAString& aDir) {
  aDir = mDir;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetLang(nsAString& aLang) {
  aLang = mLang;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetRequireInteraction(bool* aRequireInteraction) {
  *aRequireInteraction = mRequireInteraction;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetData(nsAString& aData) {
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetPrincipal(nsIPrincipal** aPrincipal) {
  NS_IF_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetURI(nsIURI** aURI) {
  if (!nsAlertsUtils::IsActionablePrincipal(mPrincipal)) {
    *aURI = nullptr;
    return NS_OK;
  }
  auto* basePrin = BasePrincipal::Cast(mPrincipal);
  return basePrin->GetURI(aURI);
}

NS_IMETHODIMP
AlertNotification::GetInPrivateBrowsing(bool* aInPrivateBrowsing) {
  *aInPrivateBrowsing = mInPrivateBrowsing;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetActionable(bool* aActionable) {
  *aActionable = nsAlertsUtils::IsActionablePrincipal(mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetSilent(bool* aSilent) {
  *aSilent = mSilent;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetVibrate(nsTArray<uint32_t>& aVibrate) {
  aVibrate = mVibrate.Clone();
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetActions(nsTArray<RefPtr<nsIAlertAction>>& aActions) {
  aActions = mActions.Clone();
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetSource(nsAString& aSource) {
  nsAlertsUtils::GetSourceHostPort(mPrincipal, aSource);
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetOpaqueRelaunchData(nsAString& aOpaqueRelaunchData) {
  aOpaqueRelaunchData = mOpaqueRelaunchData;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::SetOpaqueRelaunchData(const nsAString& aOpaqueRelaunchData) {
  mOpaqueRelaunchData = aOpaqueRelaunchData;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::LoadImage(uint32_t aTimeout,
                             nsIAlertNotificationImageListener* aListener,
                             nsISupports* aUserData, nsICancelable** aRequest) {
  NS_ENSURE_ARG(aListener);
  NS_ENSURE_ARG_POINTER(aRequest);
  *aRequest = nullptr;

  // Exit early if this alert doesn't have an image.
  if (mImageURL.IsEmpty()) {
    return aListener->OnImageMissing(aUserData);
  }
  nsCOMPtr<nsIURI> imageURI;
  NS_NewURI(getter_AddRefs(imageURI), mImageURL);
  if (!imageURI) {
    return aListener->OnImageMissing(aUserData);
  }

  RefPtr<AlertImageRequest> request = new AlertImageRequest(
      imageURI, mPrincipal, mInPrivateBrowsing, aTimeout, aListener, aUserData);
  request->Start();
  request.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetAction(const nsAString& aName,
                             nsIAlertAction** aAlertAction) {
  NS_ENSURE_ARG_POINTER(aAlertAction);
  for (const auto& action : mActions) {
    nsString name;
    MOZ_TRY(action->GetAction(name));
    if (name.Equals(aName)) {
      RefPtr<nsIAlertAction> match = action;
      match.forget(aAlertAction);
      return NS_OK;
    }
  }
  *aAlertAction = nullptr;
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION(AlertImageRequest, mURI, mPrincipal, mListener,
                         mUserData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AlertImageRequest)
  NS_INTERFACE_MAP_ENTRY(imgINotificationObserver)
  NS_INTERFACE_MAP_ENTRY(nsICancelable)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, imgINotificationObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AlertImageRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AlertImageRequest)

AlertImageRequest::AlertImageRequest(
    nsIURI* aURI, nsIPrincipal* aPrincipal, bool aInPrivateBrowsing,
    uint32_t aTimeout, nsIAlertNotificationImageListener* aListener,
    nsISupports* aUserData)
    : mURI(aURI),
      mPrincipal(aPrincipal),
      mInPrivateBrowsing(aInPrivateBrowsing),
      mTimeout(aTimeout),
      mListener(aListener),
      mUserData(aUserData) {}

AlertImageRequest::~AlertImageRequest() {
  if (mRequest) {
    mRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
  }
}

void AlertImageRequest::Notify(imgIRequest* aRequest, int32_t aType,
                               const nsIntRect* aData) {
  MOZ_ASSERT(aRequest == mRequest);

  uint32_t imgStatus = imgIRequest::STATUS_ERROR;
  nsresult rv = aRequest->GetImageStatus(&imgStatus);
  if (NS_WARN_IF(NS_FAILED(rv)) || (imgStatus & imgIRequest::STATUS_ERROR)) {
    NotifyMissing();
    return;
  }

  // If the image is already decoded, `FRAME_COMPLETE` will fire before
  // `LOAD_COMPLETE`, so we can notify the listener immediately. Otherwise,
  // we'll need to request a decode when `LOAD_COMPLETE` fires, and wait
  // for the first frame.

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    if (!(imgStatus & imgIRequest::STATUS_FRAME_COMPLETE)) {
      nsCOMPtr<imgIContainer> image;
      rv = aRequest->GetImage(getter_AddRefs(image));
      if (NS_WARN_IF(NS_FAILED(rv) || !image)) {
        NotifyMissing();
        return;
      }

      // Ask the image to decode at its intrinsic size.
      int32_t width = 0, height = 0;
      image->GetWidth(&width);
      image->GetHeight(&height);
      image->RequestDecodeForSize(gfx::IntSize(width, height),
                                  imgIContainer::FLAG_HIGH_QUALITY_SCALING);
    }
    return;
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    return NotifyComplete();
  }
}

NS_IMETHODIMP
AlertImageRequest::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(aTimer == mTimer);
  return NotifyMissing();
}

NS_IMETHODIMP
AlertImageRequest::GetName(nsACString& aName) {
  aName.AssignLiteral("AlertImageRequest");
  return NS_OK;
}

NS_IMETHODIMP
AlertImageRequest::Cancel(nsresult aReason) {
  if (mRequest) {
    mRequest->Cancel(aReason);
  }
  // We call `NotifyMissing` here because we won't receive a `LOAD_COMPLETE`
  // notification if we cancel the request before it loads (bug 1233086,
  // comment 33). Once that's fixed, `nsIAlertNotification::loadImage` could
  // return the underlying `imgIRequest` instead of the wrapper.
  return NotifyMissing();
}

nsresult AlertImageRequest::Start() {
  // Keep the request alive until we notify the image listener.
  NS_ADDREF_THIS();

  nsresult rv;
  if (mTimeout > 0) {
    rv = NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, mTimeout,
                                 nsITimer::TYPE_ONE_SHOT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NotifyMissing();
    }
  }

  // Begin loading the image.
  imgLoader* il = imgLoader::NormalLoader();
  if (!il) {
    return NotifyMissing();
  }

  // Bug 1237405: `LOAD_ANONYMOUS` disables cookies, but we want to use a
  // temporary cookie jar instead. We should also use
  // `imgLoader::PrivateBrowsingLoader()` instead of the normal loader.
  // Unfortunately, the PB loader checks the load group, and asserts if its
  // load context's PB flag isn't set. The fix is to pass the load group to
  // `nsIAlertNotification::loadImage`.
  int32_t loadFlags = nsIRequest::LOAD_NORMAL;
  if (mInPrivateBrowsing) {
    loadFlags = nsIRequest::LOAD_ANONYMOUS;
  }

  rv = il->LoadImageXPCOM(
      mURI, nullptr, nullptr, mPrincipal, nullptr, this, nullptr, loadFlags,
      nullptr, nsIContentPolicy::TYPE_INTERNAL_IMAGE, getter_AddRefs(mRequest));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NotifyMissing();
  }

  return NS_OK;
}

nsresult AlertImageRequest::NotifyMissing() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  if (nsCOMPtr<nsIAlertNotificationImageListener> listener =
          std::move(mListener)) {
    nsresult rv = listener->OnImageMissing(mUserData);
    NS_RELEASE_THIS();
    return rv;
  }

  return NS_OK;
}

void AlertImageRequest::NotifyComplete() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  if (nsCOMPtr<nsIAlertNotificationImageListener> listener =
          std::move(mListener)) {
    listener->OnImageReady(mUserData, mRequest);
    NS_RELEASE_THIS();
  }
}

NS_IMPL_ISUPPORTS(AlertAction, nsIAlertAction)

AlertAction::AlertAction(const nsAString& aAction, const nsAString& aTitle)
    : mAction(aAction), mTitle(aTitle) {}

NS_IMETHODIMP
AlertAction::GetAction(nsAString& aAction) {
  aAction = mAction;
  return NS_OK;
}

NS_IMETHODIMP
AlertAction::GetTitle(nsAString& aTitle) {
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
AlertAction::GetIconURL(nsAString& aTitle) {
  aTitle.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
AlertAction::GetWindowsSystemActivationType(bool* aType) {
  *aType = false;
  return NS_OK;
}

NS_IMETHODIMP
AlertAction::GetOpaqueRelaunchData(nsAString& aData) {
  aData.Truncate();
  return NS_OK;
}

}  // namespace mozilla
