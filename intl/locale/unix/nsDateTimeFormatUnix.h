
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDateTimeFormatUnix_h__
#define nsDateTimeFormatUnix_h__


#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsIUnicodeDecoder.h"

#define kPlatformLocaleLength 64

class nsDateTimeFormatUnix : public nsIDateTimeFormat {

public: 
  NS_DECL_THREADSAFE_ISUPPORTS 

  // performs a locale sensitive date formatting operation on the time_t parameter
  NS_IMETHOD FormatTime(nsILocale* locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const time_t  timetTime, 
                        nsAString& stringOut) override;

  // performs a locale sensitive date formatting operation on the struct tm parameter
  NS_IMETHOD FormatTMTime(nsILocale* locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const struct tm*  tmTime, 
                        nsAString& stringOut) override;

  // performs a locale sensitive date formatting operation on the PRTime parameter
  NS_IMETHOD FormatPRTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const PRTime  prTime, 
                          nsAString& stringOut) override;

  // performs a locale sensitive date formatting operation on the PRExplodedTime parameter
  NS_IMETHOD FormatPRExplodedTime(nsILocale* locale, 
                                  const nsDateFormatSelector  dateFormatSelector, 
                                  const nsTimeFormatSelector timeFormatSelector, 
                                  const PRExplodedTime*  explodedTime, 
                                  nsAString& stringOut) override;


  nsDateTimeFormatUnix() {mLocale.Truncate();mAppLocale.Truncate();}

private:
  virtual ~nsDateTimeFormatUnix() {}

  // init this interface to a specified locale
  NS_IMETHOD Initialize(nsILocale* locale);

  void LocalePreferred24hour();

  nsString    mLocale;
  nsString    mAppLocale;
  nsCString   mCharset;        // in order to convert API result to unicode
  nsCString   mPlatformLocale; // for setlocale
  bool        mLocalePreferred24hour;                       // true if 24 hour format is preferred by current locale
  bool        mLocaleAMPMfirst;                             // true if AM/PM string is preferred before the time
  nsCOMPtr <nsIUnicodeDecoder>   mDecoder;
};

#endif  /* nsDateTimeFormatUnix_h__ */
