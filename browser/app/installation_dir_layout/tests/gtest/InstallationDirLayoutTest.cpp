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

class InstallationDirLayoutTest : public ::testing::Test {
 protected:
  std::wstring dist_bin_dir;

  void SetUp() override {
    wchar_t pathbuf[MAX_PATH];
    DWORD dwRet = GetCurrentDirectoryW(sizeof(pathbuf), pathbuf);
    if (dwRet == 0) {
      FAIL() << "Failed getting cwd";
    }
    if (dwRet > sizeof(pathbuf)) {
      FAIL() << "Path was too long for buffer";
    }
    // We have the current directory in pathbuf. Now construct the path to
    // dist/bin.
    wchar_t* cut_location = wcsstr(pathbuf, L"\\_tests\\gtest");
    if (!cut_location) {
      FAIL() << "\\_tests\\gtest directory not found in path";
    }
    // Null-terminate the string at the cut location.
    *cut_location = 0;
    // And then stuff it into a wstring, so we can let the standard library
    // worry about buffer limits
    dist_bin_dir.append(pathbuf);
    dist_bin_dir.append(L"\\dist\\bin");
  }

  void TearDown() override {}
};

using FuncType = InstallationDirLayoutType (*)();

TEST_F(InstallationDirLayoutTest, SingleLayoutTest) {
  std::wstring runtimelib_path;
  runtimelib_path.append(dist_bin_dir);
  // Since this is the default, we don't need to access its by path.
  runtimelib_path.append(L"\\InstallationDirLayout.dll");
  std::wcout << L"Here is dll str: " << runtimelib_path << std::endl;
  HINSTANCE hRuntimeLibrary =
      LoadLibraryEx(runtimelib_path.c_str(), nullptr, 0);
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
  std::wstring runtimelib_path;
  runtimelib_path.append(dist_bin_dir);
  runtimelib_path.append(
      L"\\installation_dir_layout\\versioned\\InstallationDirLayout.dll");
  HINSTANCE hRuntimeLibrary =
      LoadLibraryEx(runtimelib_path.c_str(), nullptr, 0);
  ASSERT_NE(hRuntimeLibrary, nullptr);
  FuncType dirLayoutFunc =
      (FuncType)GetProcAddress(hRuntimeLibrary, "GetInstallationDirLayoutType");
  ASSERT_NE(dirLayoutFunc, nullptr);

  InstallationDirLayoutType layoutType = dirLayoutFunc();
  ASSERT_EQ(layoutType, InstallationDirLayoutType::Versioned);
  bool freeResult = FreeLibrary(hRuntimeLibrary);
  ASSERT_TRUE(freeResult);
}
