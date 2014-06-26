/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipboard_h_
#define nsClipboard_h_

#include "nsBaseClipboard.h"
#include "nsXPIDLString.h"

#import <Cocoa/Cocoa.h>

class nsITransferable;

class nsClipboard : public nsBaseClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  // nsIClipboard  
  NS_IMETHOD HasDataMatchingFlavors(const char** aFlavorList, uint32_t aLength,
                                    int32_t aWhichClipboard, bool *_retval);
  NS_IMETHOD SupportsFindClipboard(bool *_retval);

  // Helper methods, used also by nsDragService
  static NSDictionary* PasteboardDictFromTransferable(nsITransferable *aTransferable);
  static bool IsStringType(const nsCString& aMIMEType, NSString** aPasteboardType);
  static NSString* WrapHtmlForSystemPasteboard(NSString* aString);
  static nsresult TransferableFromPasteboard(nsITransferable *aTransferable, NSPasteboard *pboard);

protected:

  // impelement the native clipboard behavior
  NS_IMETHOD SetNativeClipboardData(int32_t aWhichClipboard);
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable, int32_t aWhichClipboard);
  
private:
  int32_t mCachedClipboard;
  int32_t mChangeCount; // Set to the native change count after any modification of the clipboard.
};

#endif // nsClipboard_h_
