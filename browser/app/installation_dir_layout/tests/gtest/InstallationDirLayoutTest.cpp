/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include <windows.h>
#include "InstallationDirLayout.h"
#include <string>
#include <iostream>
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

class InstallationDirLayoutTest : public ::testing::Test {
 protected:
  nsCOMPtr<nsIFile> gre_dir;

  void SetUp() override {
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(gre_dir));
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  void TearDown() override { gre_dir = nullptr; }
};

using FuncType = InstallationDirLayoutType (*)();

TEST_F(InstallationDirLayoutTest, SingleLayoutTest) {
  nsIFile* runtimelib_path;
  nsresult res = gre_dir->Clone(&runtimelib_path);
  ASSERT_TRUE(NS_SUCCEEDED(res));
  // Since this is the default, we don't need to access its by path.
  runtimelib_path->Append(u"InstallationDirLayout.dll"_ns);
  HINSTANCE hRuntimeLibrary =
      LoadLibraryExW(runtimelib_path->NativePath().get(), nullptr, 0);
  ASSERT_NE(hRuntimeLibrary, nullptr);
  FuncType dirLayoutFunc =
      (FuncType)GetProcAddress(hRuntimeLibrary, "GetInstallationDirLayoutType");
  ASSERT_NE(dirLayoutFunc, nullptr);

  InstallationDirLayoutType layoutType = dirLayoutFunc();
  ASSERT_EQ(layoutType, InstallationDirLayoutType::Single);
  bool freeResult = FreeLibrary(hRuntimeLibrary);
  ASSERT_TRUE(freeResult);
}

TEST_F(InstallationDirLayoutTest, VersionedLayoutTest) {
  nsIFile* runtimelib_path;
  nsresult res = gre_dir->Clone(&runtimelib_path);
  ASSERT_TRUE(NS_SUCCEEDED(res));
  runtimelib_path->Append(u"installation_dir_layout"_ns);
  runtimelib_path->Append(u"versioned"_ns);
  runtimelib_path->Append(u"InstallationDirLayout.dll"_ns);
  HINSTANCE hRuntimeLibrary =
      LoadLibraryExW(runtimelib_path->NativePath().get(), nullptr, 0);

  ASSERT_NE(hRuntimeLibrary, nullptr);
  FuncType dirLayoutFunc =
      (FuncType)GetProcAddress(hRuntimeLibrary, "GetInstallationDirLayoutType");
  ASSERT_NE(dirLayoutFunc, nullptr);

  InstallationDirLayoutType layoutType = dirLayoutFunc();
  ASSERT_EQ(layoutType, InstallationDirLayoutType::Versioned);
  bool freeResult = FreeLibrary(hRuntimeLibrary);
  ASSERT_TRUE(freeResult);
}
