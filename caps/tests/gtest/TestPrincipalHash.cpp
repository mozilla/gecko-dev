/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsIPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "nsNetUtil.h"

TEST(PrincipalHash, DocumentDomain)
{
  nsCOMPtr<nsIScriptSecurityManager> ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv =
      ssm->CreateContentPrincipalFromOrigin("https://sub.mozilla.org"_ns, getter_AddRefs(principal));
  EXPECT_NS_SUCCEEDED(rv);

  const auto hash = principal->GetHashValue();

  nsCOMPtr<nsIURI> domain;
  rv = NS_NewURI(getter_AddRefs(domain), "https://mozilla.org"_ns);
  EXPECT_NS_SUCCEEDED(rv);
  principal->SetDomain(domain);

  ASSERT_EQ(principal->GetHashValue(), hash) << "Principal hash shouldn't change";
}
