/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilePreferences.h"

#include "mozilla/Preferences.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsString.h"

namespace mozilla {
namespace FilePreferences {

static bool sBlockUNCPaths = false;
typedef nsTArray<nsString> Paths;

static Paths& PathArray()
{
  static Paths sPaths;
  return sPaths;
}

static void AllowDirectory(char const* directory)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(directory, getter_AddRefs(file));
  if (!file) {
    return;
  }

  nsString path;
  if (NS_FAILED(file->GetTarget(path))) {
    return;
  }

  // The whitelist makes sense only for UNC paths, because this code is used
  // to block only UNC paths, hence, no need to add non-UNC directories here
  // as those would never pass the check.
  if (!StringBeginsWith(path, NS_LITERAL_STRING("\\\\"))) {
    return;
  }

  if (!PathArray().Contains(path)) {
    PathArray().AppendElement(path);
  }
}

void InitPrefs()
{
  sBlockUNCPaths = Preferences::GetBool("network.file.disable_unc_paths", false);
}

void InitDirectoriesWhitelist()
{
  // NS_GRE_DIR is the installation path where the binary resides.
  AllowDirectory(NS_GRE_DIR);
  // NS_APP_USER_PROFILE_50_DIR and NS_APP_USER_PROFILE_LOCAL_50_DIR are the two
  // parts of the profile we store permanent and local-specific data.
  AllowDirectory(NS_APP_USER_PROFILE_50_DIR);
  AllowDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR);
}

namespace { // anon

class Normalizer
{
public:
  Normalizer(const nsAString& aFilePath, const char16_t aSeparator);
  bool Get(nsAString& aNormalizedFilePath);

private:
  bool ConsumeItem();
  bool ConsumeSeparator();
  bool IsEOF() { return mFilePathCursor == mFilePathEnd; }

  bool ConsumeName();
  bool CheckParentDir();
  bool CheckCurrentDir();

  nsString::const_char_iterator mFilePathCursor;
  nsString::const_char_iterator mFilePathEnd;

  nsDependentSubstring mItem;
  char16_t const mSeparator;
  nsTArray<nsDependentSubstring> mStack;
};

Normalizer::Normalizer(const nsAString& aFilePath, const char16_t aSeparator)
  : mFilePathCursor(aFilePath.BeginReading())
  , mFilePathEnd(aFilePath.EndReading())
  , mSeparator(aSeparator)
{
}

bool Normalizer::ConsumeItem()
{
  if (IsEOF()) {
    return false;
  }

  nsString::const_char_iterator nameBegin = mFilePathCursor;
  while (mFilePathCursor != mFilePathEnd) {
    if (*mFilePathCursor == mSeparator) {
      break; // don't include the separator
    }
    ++mFilePathCursor;
  }

  mItem.Rebind(nameBegin, mFilePathCursor);
  return true;
}

bool Normalizer::ConsumeSeparator()
{
  if (IsEOF()) {
    return false;
  }

  if (*mFilePathCursor != mSeparator) {
    return false;
  }

  ++mFilePathCursor;
  return true;
}

bool Normalizer::Get(nsAString& aNormalizedFilePath)
{
  aNormalizedFilePath.Truncate();

  if (IsEOF()) {
    return true;
  }
  if (ConsumeSeparator()) {
    aNormalizedFilePath.Append(mSeparator);
  }

  if (IsEOF()) {
    return true;
  }
  if (ConsumeSeparator()) {
    aNormalizedFilePath.Append(mSeparator);
  }

  while (!IsEOF()) {
    if (!ConsumeName()) {
      return false;
    }
  }

  for (auto const& name : mStack) {
    aNormalizedFilePath.Append(name);
  }

  return true;
}

bool Normalizer::ConsumeName()
{
  if (!ConsumeItem()) {
    return true;
  }

  if (CheckCurrentDir()) {
    return true;
  }

  if (CheckParentDir()) {
    if (!mStack.Length()) {
      // This means there are more \.. than valid names
      return false;
    }

    mStack.RemoveElementAt(mStack.Length() - 1);
    return true;
  }

  if (mItem.IsEmpty()) {
    // this means an empty name (a lone slash), which is illegal
    return false;
  }

  if (ConsumeSeparator()) {
    mItem.Rebind(mItem.BeginReading(), mFilePathCursor);
  }
  mStack.AppendElement(mItem);

  return true;
}

bool Normalizer::CheckCurrentDir()
{
  if (mItem == NS_LITERAL_STRING(".")) {
    ConsumeSeparator();
    // EOF is acceptable
    return true;
  }

  return false;
}

bool Normalizer::CheckParentDir()
{
  if (mItem == NS_LITERAL_STRING("..")) {
    ConsumeSeparator();
    // EOF is acceptable
    return true;
  }

  return false;
}

} // anon

bool IsBlockedUNCPath(const nsAString& aFilePath)
{
  if (!sBlockUNCPaths) {
    return false;
  }

  if (!StringBeginsWith(aFilePath, NS_LITERAL_STRING("\\\\"))) {
    return false;
  }

  nsAutoString normalized;
  if (!Normalizer(aFilePath, L'\\').Get(normalized)) {
    // Broken paths are considered invalid and thus inaccessible
    return true;
  }

  for (const auto& allowedPrefix : PathArray()) {
    if (StringBeginsWith(normalized, allowedPrefix)) {
      if (normalized.Length() == allowedPrefix.Length()) {
        return false;
      }
      if (normalized[allowedPrefix.Length()] == L'\\') {
        return false;
      }

      // When we are here, the path has a form "\\path\prefixevil"
      // while we have an allowed prefix of "\\path\prefix".
      // Note that we don't want to add a slash to the end of a prefix
      // so that opening the directory (no slash at the end) still works.
      break;
    }
  }

  return true;
}

void testing::SetBlockUNCPaths(bool aBlock)
{
  sBlockUNCPaths = aBlock;
}

void testing::AddDirectoryToWhitelist(nsAString const & aPath)
{
  PathArray().AppendElement(aPath);
}

bool testing::NormalizePath(nsAString const & aPath, nsAString & aNormalized)
{
  Normalizer normalizer(aPath, L'\\');
  return normalizer.Get(aNormalized);
}

} // ::FilePreferences
} // ::mozilla
