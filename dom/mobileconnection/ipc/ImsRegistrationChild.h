/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_ImsRegistrationChild_h
#define mozilla_dom_mobileconnection_ImsRegistrationChild_h

#include "mozilla/dom/mobileconnection/PImsRegistrationChild.h"
#include "mozilla/dom/mobileconnection/PImsRegistrationRequestChild.h"
#include "mozilla/dom/mobileconnection/PImsRegServiceFinderChild.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIImsRegService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

/**
 * Child actor of PImsRegistration. The object is created by
 * ImsRegIPCService and destroyed after ImsRegIPCService is
 * shutdown. For multi-sim device, more than one instance will
 * be created and each instance represents the ImsRegHandler per sim slot.
 */
class ImsRegistrationChild MOZ_FINAL : public PImsRegistrationChild
                                     , public nsIImsRegHandler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGHANDLER

  explicit ImsRegistrationChild(uint32_t aServiceId);

  void
  Init();

  void
  Shutdown();

private:
  ImsRegistrationChild() = delete;

  // MOZ_FINAL suppresses -Werror,-Wdelete-non-virtual-dtor
  ~ImsRegistrationChild()
  {
    MOZ_COUNT_DTOR(ImsRegistrationChild);
  }

protected:
  bool
  SendRequest(const ImsRegistrationRequest& aRequest,
              nsIImsRegCallback* aCallback);

  virtual void
  ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual PImsRegistrationRequestChild*
  AllocPImsRegistrationRequestChild(const ImsRegistrationRequest& request) MOZ_OVERRIDE;

  virtual bool
  DeallocPImsRegistrationRequestChild(PImsRegistrationRequestChild* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyEnabledStateChanged(const bool& aEnabled) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyPreferredProfileChanged(const uint16_t& aProfile) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyImsCapabilityChanged(const int16_t& aCapability,
                                 const nsString& UnregisteredReason) MOZ_OVERRIDE;

private:
  bool mLive;
  nsCOMArray<nsIImsRegListener> mListeners;
  bool mEnabled;
  uint16_t mPreferredProfile;
  int16_t mCapability;
  nsString mUnregisteredReason;
  nsTArray<uint16_t> mSupportedBearers;
};

/******************************************************************************
 * PImsRegistrationRequestChild
 ******************************************************************************/

/**
 * Child actor of PImsRegistrationRequest. The object is created when an
 * asynchronous request is made and destroyed after receiving the response sent
 * by parent actor.
 */
class ImsRegistrationRequestChild : public PImsRegistrationRequestChild
{
public:
  explicit ImsRegistrationRequestChild(nsIImsRegCallback* aRequestCallback)
    : mRequestCallback(aRequestCallback)
  {
    MOZ_COUNT_CTOR(ImsRegistrationRequestChild);
    MOZ_ASSERT(mRequestCallback);
  }

  bool
  DoReply(const ImsRegistrationReplySuccess& aReply);

  bool
  DoReply(const ImsRegistrationReplyError& aReply);

protected:
  virtual
  ~ImsRegistrationRequestChild()
  {
    MOZ_COUNT_DTOR(ImsRegistrationRequestChild);
  }

  virtual void
  ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__(const ImsRegistrationReply& aReply) MOZ_OVERRIDE;

private:
  nsCOMPtr<nsIImsRegCallback> mRequestCallback;
};

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobileconnection_ImsRegistrationChild_h
