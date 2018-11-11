/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsUTF8ConverterService_h__
#define nsUTF8ConverterService_h__

#include "nsIUTF8ConverterService.h"

//==============================================================

class nsUTF8ConverterService: public nsIUTF8ConverterService {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUTF8CONVERTERSERVICE

  nsUTF8ConverterService() {}

private:
  virtual ~nsUTF8ConverterService() {}
};

#endif // nsUTF8ConverterService_h__

