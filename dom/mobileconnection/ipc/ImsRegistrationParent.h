/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_ImsRegistrationParent_h
#define mozilla_dom_mobileconnection_ImsRegistrationParent_h

#include "mozilla/dom/mobileconnection/PImsRegistrationParent.h"
#include "mozilla/dom/mobileconnection/PImsRegistrationRequestParent.h"
#include "mozilla/dom/mobileconnection/PImsRegServiceFinderParent.h"
#include "nsIImsRegService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

class ImsRegServiceFinderParent : public PImsRegServiceFinderParent
{
public:
  virtual bool
  RecvCheckDeviceCapability(bool* aIsServiceInstalled,
                            nsTArray<uint32_t>* aEnabledServiceIds) MOZ_OVERRIDE;
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
};

/**
 * Parent actor of PImsRegistration. This object is created/destroyed along
 * with child actor.
 */
class ImsRegistrationParent : public PImsRegistrationParent
                            , public nsIImsRegListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGLISTENER

  explicit ImsRegistrationParent(uint32_t aServiceId);

protected:
  virtual
  ~ImsRegistrationParent()
  {
    MOZ_COUNT_DTOR(ImsRegistrationParent);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPImsRegistrationRequestConstructor(PImsRegistrationRequestParent* aActor,
                                         const ImsRegistrationRequest& aRequest) MOZ_OVERRIDE;

  virtual PImsRegistrationRequestParent*
  AllocPImsRegistrationRequestParent(const ImsRegistrationRequest& request) MOZ_OVERRIDE;

  virtual bool
  DeallocPImsRegistrationRequestParent(PImsRegistrationRequestParent* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvInit(bool* aEnabled, uint16_t* aProfile,
           int16_t* aCapability, nsString* aUnregisteredReason,
           nsTArray<uint16_t>* aSupportedBearers) MOZ_OVERRIDE;

private:
  bool mLive;
  nsCOMPtr<nsIImsRegHandler> mHandler;
};

/******************************************************************************
 * PImsRegistrationRequestParent
 ******************************************************************************/

/**
 * Parent actor of PImsRegistrationRequestParent. The object is created along
 * with child actor and destroyed after the callback function of
 * nsIImsRegCallback is called. Child actor might be destroyed before
 * any callback is triggered. So we use mLive to maintain the status of child
 * actor in order to present sending data to a dead one.
 */
class ImsRegistrationRequestParent : public PImsRegistrationRequestParent
                                   , public nsIImsRegCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGCALLBACK

  explicit ImsRegistrationRequestParent(nsIImsRegHandler* aHandler)
    : mHandler(aHandler)
    , mLive(true)
  {
    MOZ_COUNT_CTOR(ImsRegistrationRequestParent);
  }

  bool
  DoRequest(const SetImsEnabledRequest& aRequest);

  bool
  DoRequest(const SetImsPreferredProfileRequest& aRequest);

protected:
  virtual
  ~ImsRegistrationRequestParent()
  {
    MOZ_COUNT_DTOR(ImsRegistrationRequestParent);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  nsresult
  SendReply(const ImsRegistrationReply& aReply);

private:
  nsCOMPtr<nsIImsRegHandler> mHandler;
  bool mLive;
};

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobileconnection_ImsRegistrationParent_h
