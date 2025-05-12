/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessClipboard.h"

#include "nsISupportsPrimitives.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"

namespace mozilla::widget {

NS_IMPL_ISUPPORTS_INHERITED0(HeadlessClipboard, nsBaseClipboard)

HeadlessClipboard::HeadlessClipboard()
    : nsBaseClipboard(mozilla::dom::ClipboardCapabilities(
          true /* supportsSelectionClipboard */,
          true /* supportsFindClipboard */,
          true /* supportsSelectionCache */)) {
  for (auto& clipboard : mClipboards) {
    clipboard = MakeUnique<HeadlessClipboardData>();
  }
}

NS_IMETHODIMP
HeadlessClipboard::SetNativeClipboardData(nsITransferable* aTransferable,
                                          ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  // Clear out the clipboard in order to set the new data.
  EmptyNativeClipboardData(aWhichClipboard);

  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanExport(flavors);
  if (NS_FAILED(rv)) {
    return rv;
  }

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  for (const auto& flavor : flavors) {
    if (!flavor.EqualsLiteral(kTextMime) && !flavor.EqualsLiteral(kHTMLMime)) {
      continue;
    }

    nsCOMPtr<nsISupports> data;
    rv = aTransferable->GetTransferData(flavor.get(), getter_AddRefs(data));
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsISupportsString> wideString = do_QueryInterface(data);
    if (!wideString) {
      continue;
    }

    nsAutoString utf16string;
    wideString->GetData(utf16string);
    flavor.EqualsLiteral(kTextMime) ? clipboard->SetText(utf16string)
                                    : clipboard->SetHTML(utf16string);
  }

  return NS_OK;
}

mozilla::Result<nsCOMPtr<nsISupports>, nsresult>
HeadlessClipboard::GetNativeClipboardData(const nsACString& aFlavor,
                                          ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  if (!aFlavor.EqualsLiteral(kTextMime) && !aFlavor.EqualsLiteral(kHTMLMime)) {
    return nsCOMPtr<nsISupports>{};
  }

  bool isText = aFlavor.EqualsLiteral(kTextMime);
  if (!(isText ? clipboard->HasText() : clipboard->HasHTML())) {
    return nsCOMPtr<nsISupports>{};
  }

  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  rv = dataWrapper->SetData(isText ? clipboard->GetText()
                                   : clipboard->GetHTML());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsCOMPtr<nsISupports>{};
  }

  return nsCOMPtr<nsISupports>(std::move(dataWrapper));
}

nsresult HeadlessClipboard::EmptyNativeClipboardData(
    ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);
  clipboard->Clear();
  return NS_OK;
}

mozilla::Result<int32_t, nsresult>
HeadlessClipboard::GetNativeClipboardSequenceNumber(
    ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);
  return clipboard->GetChangeCount();
  ;
}

mozilla::Result<bool, nsresult>
HeadlessClipboard::HasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, ClipboardType aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  auto& clipboard = mClipboards[aWhichClipboard];
  MOZ_ASSERT(clipboard);

  // Retrieve the union of all aHasType in aFlavorList
  for (auto& flavor : aFlavorList) {
    if ((flavor.EqualsLiteral(kTextMime) && clipboard->HasText()) ||
        (flavor.EqualsLiteral(kHTMLMime) && clipboard->HasHTML())) {
      return true;
    }
  }
  return false;
}

}  // namespace mozilla::widget
