/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIStatusReporter.h"
#include "nsCOMArray.h"
#include "nsString.h"

class nsStatusReporter final : public nsIStatusReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTATUSREPORTER

  nsStatusReporter(nsACString& aProcess, nsACString& aDesc);

private:
  nsCString sProcess;
  nsCString sName;
  nsCString sDesc;

  virtual ~nsStatusReporter();
};


class nsStatusReporterManager : public nsIStatusReporterManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTATUSREPORTERMANAGER

  nsStatusReporterManager();

private:
  nsCOMArray<nsIStatusReporter> mReporters;

  virtual ~nsStatusReporterManager();
};
