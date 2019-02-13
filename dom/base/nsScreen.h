/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsScreen_h___
#define nsScreen_h___

#include "mozilla/Attributes.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HalScreenConfiguration.h"
#include "nsIDOMScreen.h"
#include "nsCOMPtr.h"
#include "nsRect.h"

class nsDeviceContext;

// Script "screen" object
class nsScreen : public mozilla::DOMEventTargetHelper
               , public nsIDOMScreen
               , public mozilla::hal::ScreenConfigurationObserver
{
  typedef mozilla::ErrorResult ErrorResult;
public:
  static already_AddRefed<nsScreen> Create(nsPIDOMWindow* aWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSCREEN
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(mozilla::DOMEventTargetHelper)

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  int32_t GetTop(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetRect(rect);
    return rect.y;
  }

  int32_t GetLeft(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetRect(rect);
    return rect.x;
  }

  int32_t GetWidth(ErrorResult& aRv)
  {
    nsRect rect;
    if (IsDeviceSizePageSize()) {
      nsCOMPtr<nsPIDOMWindow> owner = GetOwner();
      if (owner) {
        int32_t innerWidth = 0;
        aRv = owner->GetInnerWidth(&innerWidth);
        return innerWidth;
      }
    }

    aRv = GetRect(rect);
    return rect.width;
  }

  int32_t GetHeight(ErrorResult& aRv)
  {
    nsRect rect;
    if (IsDeviceSizePageSize()) {
      nsCOMPtr<nsPIDOMWindow> owner = GetOwner();
      if (owner) {
        int32_t innerHeight = 0;
        aRv = owner->GetInnerHeight(&innerHeight);
        return innerHeight;
      }
    }

    aRv = GetRect(rect);
    return rect.height;
  }

  int32_t GetPixelDepth(ErrorResult& aRv);
  int32_t GetColorDepth(ErrorResult& aRv)
  {
    return GetPixelDepth(aRv);
  }

  int32_t GetAvailTop(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.y;
  }

  int32_t GetAvailLeft(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.x;
  }

  int32_t GetAvailWidth(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.width;
  }

  int32_t GetAvailHeight(ErrorResult& aRv)
  {
    nsRect rect;
    aRv = GetAvailRect(rect);
    return rect.height;
  }

  void GetMozOrientation(nsString& aOrientation);

  IMPL_EVENT_HANDLER(mozorientationchange)

  bool MozLockOrientation(const nsAString& aOrientation, ErrorResult& aRv);
  bool MozLockOrientation(const mozilla::dom::Sequence<nsString>& aOrientations, ErrorResult& aRv);
  void MozUnlockOrientation();

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Notify(const mozilla::hal::ScreenConfiguration& aConfiguration) override;

protected:
  nsDeviceContext* GetDeviceContext();
  nsresult GetRect(nsRect& aRect);
  nsresult GetAvailRect(nsRect& aRect);
  nsresult GetWindowInnerRect(nsRect& aRect);

  mozilla::dom::ScreenOrientation mOrientation;

private:
  class FullScreenEventListener final : public nsIDOMEventListener
  {
    ~FullScreenEventListener() {}
  public:
    FullScreenEventListener() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER
  };

  explicit nsScreen(nsPIDOMWindow* aWindow);
  virtual ~nsScreen();

  enum LockPermission {
    LOCK_DENIED,
    FULLSCREEN_LOCK_ALLOWED,
    LOCK_ALLOWED
  };

  LockPermission GetLockOrientationPermission() const;

  bool IsDeviceSizePageSize();

  bool ShouldResistFingerprinting() const;

  nsRefPtr<FullScreenEventListener> mEventListener;
};

#endif /* nsScreen_h___ */
