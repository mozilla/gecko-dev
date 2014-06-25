/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RedirectChannelRegistrar_h__
#define RedirectChannelRegistrar_h__

#include "nsIRedirectChannelRegistrar.h"

#include "nsIChannel.h"
#include "nsIParentChannel.h"
#include "nsInterfaceHashtable.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace net {

class RedirectChannelRegistrar MOZ_FINAL : public nsIRedirectChannelRegistrar
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREDIRECTCHANNELREGISTRAR

  RedirectChannelRegistrar();

private:
  ~RedirectChannelRegistrar() {}

protected:
  typedef nsInterfaceHashtable<nsUint32HashKey, nsIChannel>
          ChannelHashtable;
  typedef nsInterfaceHashtable<nsUint32HashKey, nsIParentChannel>
          ParentChannelHashtable;

  ChannelHashtable mRealChannels;
  ParentChannelHashtable mParentChannels;
  uint32_t mId;
};

}
}

#endif
