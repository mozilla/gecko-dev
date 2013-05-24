/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************/
/* This is an automatically generated file. If you're not                    */
/* nsStrictTransportSecurityService.cpp, you shouldn't be #including it.     */
/*****************************************************************************/

#include "mozilla/StandardInteger.h"
const PRTime gPreloadListExpirationTime = INT64_C(1362156597190000);

class nsSTSPreload
{
  public:
    const char *mHost;
    const bool mIncludeSubdomains;
};

static const nsSTSPreload kSTSPreloadList[] = {
  { "alpha.irccloud.com", false },
  { "app.recurly.com", false },
  { "arivo.com.br", true },
  { "blog.torproject.org", false },
  { "business.medbank.com.mt", true },
  { "check.torproject.org", false },
  { "cloudsecurityalliance.org", false },
  { "crate.io", true },
  { "crypto.cat", true },
  { "crypto.is", true },
  { "developer.mydigipass.com", false },
  { "ebanking.indovinabank.com.vn", false },
  { "factor.cc", false },
  { "id.mayfirst.org", false },
  { "keyerror.com", true },
  { "logentries.com", false },
  { "login.sapo.pt", true },
  { "luneta.nearbuysystems.com", false },
  { "mattmccutchen.net", true },
  { "members.mayfirst.org", false },
  { "neg9.org", false },
  { "ottospora.nl", false },
  { "passwd.io", true },
  { "piratenlogin.de", true },
  { "pixi.me", true },
  { "riseup.net", true },
  { "sandbox.mydigipass.com", false },
  { "stripe.com", true },
  { "support.mayfirst.org", false },
  { "ubertt.org", true },
  { "www.apollo-auto.com", true },
  { "www.braintreepayments.com", false },
  { "www.cueup.com", false },
  { "www.mydigipass.com", false },
  { "www.noisebridge.net", false },
  { "www.torproject.org", false },
  { "m.youtube.com", false }
};
