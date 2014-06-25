/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecGTK_h___
#define nsDeviceContextSpecGTK_h___

#include "nsIDeviceContextSpec.h"
#include "nsIPrintSettings.h"
#include "nsIPrintOptions.h" 
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

#include "nsCRT.h" /* should be <limits.h>? */

#include <gtk/gtk.h>
#if (MOZ_WIDGET_GTK == 2)
#include <gtk/gtkprinter.h>
#include <gtk/gtkprintjob.h>
#else
#include <gtk/gtkunixprint.h>
#endif

#define NS_PORTRAIT  0
#define NS_LANDSCAPE 1

typedef enum
{
  pmInvalid = 0,
  pmPostScript
} PrintMethod;

class nsDeviceContextSpecGTK : public nsIDeviceContextSpec
{
public:
  nsDeviceContextSpecGTK();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetSurfaceForPrinter(gfxASurface **surface);

  NS_IMETHOD Init(nsIWidget *aWidget, nsIPrintSettings* aPS, bool aIsPrintPreview);
  NS_IMETHOD BeginDocument(const nsAString& aTitle, char16_t * aPrintToFileName, int32_t aStartPage, int32_t aEndPage);
  NS_IMETHOD EndDocument();
  NS_IMETHOD BeginPage() { return NS_OK; }
  NS_IMETHOD EndPage() { return NS_OK; }

  NS_IMETHOD GetPath (const char **aPath);    
  static nsresult GetPrintMethod(const char *aPrinter, PrintMethod &aMethod);
  
protected:
  virtual ~nsDeviceContextSpecGTK();
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  bool mToPrinter : 1;      /* If true, print to printer */
  bool mIsPPreview : 1;     /* If true, is print preview */
  char   mPath[PATH_MAX];     /* If toPrinter = false, dest file */
  char   mPrinter[256];       /* Printer name */
  GtkPrintJob*      mPrintJob;
  GtkPrinter*       mGtkPrinter;
  GtkPrintSettings* mGtkPrintSettings;
  GtkPageSetup*     mGtkPageSetup;

  nsCString         mSpoolName;
  nsCOMPtr<nsIFile> mSpoolFile;

};

//-------------------------------------------------------------------------
// Printer Enumerator
//-------------------------------------------------------------------------
class nsPrinterEnumeratorGTK MOZ_FINAL : public nsIPrinterEnumerator
{
  ~nsPrinterEnumeratorGTK() {}
public:
  nsPrinterEnumeratorGTK();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTERENUMERATOR
};

#endif /* !nsDeviceContextSpecGTK_h___ */
