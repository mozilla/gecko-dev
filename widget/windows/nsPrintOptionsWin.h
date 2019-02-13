/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintOptionsWin_h__
#define nsPrintOptionsWin_h__

#include "mozilla/embedding/PPrinting.h"
#include "nsPrintOptionsImpl.h"  

class nsIPrintSettings;
class nsIWebBrowserPrint;

//*****************************************************************************
//***    nsPrintOptions
//*****************************************************************************
class nsPrintOptionsWin : public nsPrintOptions
{
public:
  nsPrintOptionsWin();
  virtual ~nsPrintOptionsWin();

  NS_IMETHODIMP SerializeToPrintData(nsIPrintSettings* aSettings,
                                     nsIWebBrowserPrint* aWBP,
                                     mozilla::embedding::PrintData* data);
  NS_IMETHODIMP DeserializeToPrintSettings(const mozilla::embedding::PrintData& data,
                                           nsIPrintSettings* settings);

  virtual nsresult _CreatePrintSettings(nsIPrintSettings **_retval);
};



#endif /* nsPrintOptions_h__ */
