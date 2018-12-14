/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilePreferences.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

namespace mozilla {
namespace FilePreferences {

static bool sBlockUNCPaths = false;
typedef nsTArray<nsString> WinPaths;

static WinPaths& PathWhitelist() {
  static WinPaths sPaths;
  return sPaths;
}

#ifdef XP_WIN
typedef char16_t char_path_t;
#else
typedef char char_path_t;
#endif

typedef nsTArray<nsTString<char_path_t>> Paths;
static StaticAutoPtr<Paths> sBlacklist;

static Paths& PathBlacklist() {
  if (!sBlacklist) {
    sBlacklist = new nsTArray<nsTString<char_path_t>>();
    ClearOnShutdown(&sBlacklist);
  }
  return *sBlacklist;
}

static void AllowUNCDirectory(char const* directory) {
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

  if (!PathWhitelist().Contains(path)) {
    PathWhitelist().AppendElement(path);
  }
}

void InitPrefs() {
  sBlockUNCPaths =
      Preferences::GetBool("network.file.disable_unc_paths", false);

  PathBlacklist().Clear();
  nsAutoCString blacklist;
  Preferences::GetCString("network.file.path_blacklist", blacklist);

  Tokenizer p(blacklist);
  while (!p.CheckEOF()) {
    nsCString path;
    Unused << p.ReadUntil(Tokenizer::Token::Char(','), path);
    path.Trim(" ");
    if (!path.IsEmpty()) {
#ifdef XP_WIN
      PathBlacklist().AppendElement(NS_ConvertASCIItoUTF16(path));
#else
      PathBlacklist().AppendElement(path);
#endif
    }
    Unused << p.CheckChar(',');
  }
}

void InitDirectoriesWhitelist() {
  // NS_GRE_DIR is the installation path where the binary resides.
  AllowUNCDirectory(NS_GRE_DIR);
  // NS_APP_USER_PROFILE_50_DIR and NS_APP_USER_PROFILE_LOCAL_50_DIR are the two
  // parts of the profile we store permanent and local-specific data.
  AllowUNCDirectory(NS_APP_USER_PROFILE_50_DIR);
  AllowUNCDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR);
}

namespace {  // anon

template <typename TChar>
class TNormalizer {
 public:
  TNormalizer(const nsTSubstring<TChar>& aFilePath, const TChar aSeparator)
      : mFilePathCursor(aFilePath.BeginReading()),
        mFilePathEnd(aFilePath.EndReading()),
        mSeparator(aSeparator) {}

  bool Get(nsTSubstring<TChar>& aNormalizedFilePath) {
    aNormalizedFilePath.Truncate();

    // Windows UNC paths begin with double separator (\\)
    // Linux paths begin with just one separator (/)
    // If we want to use the normalizer for regular windows paths this code
    // will need to be updated.
#ifdef XP_WIN
    if (IsEOF()) {
      return true;
    }
    if (ConsumeSeparator()) {
      aNormalizedFilePath.Append(mSeparator);
    }
#endif

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

 private:
  bool ConsumeItem() {
    if (IsEOF()) {
      return false;
    }

    typename nsTString<TChar>::const_char_iterator nameBegin = mFilePathCursor;
    while (mFilePathCursor != mFilePathEnd) {
      if (*mFilePathCursor == mSeparator) {
        break;  // don't include the separator
      }
      ++mFilePathCursor;
    }

    mItem.Rebind(nameBegin, mFilePathCursor);
    return true;
  }

  bool ConsumeSeparator() {
    if (IsEOF()) {
      return false;
    }

    if (*mFilePathCursor != mSeparator) {
      return false;
    }

    ++mFilePathCursor;
    return true;
  }

  bool IsEOF() { return mFilePathCursor == mFilePathEnd; }

  bool ConsumeName() {
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

  bool CheckParentDir() {
    if (mItem.EqualsLiteral("..")) {
      ConsumeSeparator();
      // EOF is acceptable
      return true;
    }

    return false;
  }

  bool CheckCurrentDir() {
    if (mItem.EqualsLiteral(".")) {
      ConsumeSeparator();
      // EOF is acceptable
      return true;
    }

    return false;
  }

  typename nsTString<TChar>::const_char_iterator mFilePathCursor;
  typename nsTString<TChar>::const_char_iterator mFilePathEnd;

  nsTDependentSubstring<TChar> mItem;
  TChar const mSeparator;
  nsTArray<nsTDependentSubstring<TChar>> mStack;
};

}  // namespace

bool IsBlockedUNCPath(const nsAString& aFilePath) {
  typedef TNormalizer<char16_t> Normalizer;

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

  for (const auto& allowedPrefix : PathWhitelist()) {
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

#ifdef XP_WIN
const char16_t kPathSeparator = L'\\';
#else
const char kPathSeparator = '/';
#endif

bool IsAllowedPath(const nsTSubstring<char_path_t>& aFilePath) {
  typedef TNormalizer<char_path_t> Normalizer;
  // If sBlacklist has been cleared at shutdown, we must avoid calling
  // PathBlacklist() again, as that will recreate the array and we will leak.
  if (!sBlacklist) {
    return true;
  }

  if (PathBlacklist().Length() == 0) {
    return true;
  }

  nsTAutoString<char_path_t> normalized;
  if (!Normalizer(aFilePath, kPathSeparator).Get(normalized)) {
    // Broken paths are considered invalid and thus inaccessible
    return false;
  }

  for (const auto& prefix : PathBlacklist()) {
    if (StringBeginsWith(normalized, prefix)) {
      if (normalized.Length() > prefix.Length() &&
          normalized[prefix.Length()] != kPathSeparator) {
        continue;
      }
      return false;
    }
  }

  return true;
}

void testing::SetBlockUNCPaths(bool aBlock) { sBlockUNCPaths = aBlock; }

void testing::AddDirectoryToWhitelist(nsAString const& aPath) {
  PathWhitelist().AppendElement(aPath);
}

bool testing::NormalizePath(nsAString const& aPath, nsAString& aNormalized) {
  typedef TNormalizer<char16_t> Normalizer;
  Normalizer normalizer(aPath, L'\\');
  return normalizer.Get(aNormalized);
}

}  // namespace FilePreferences
}  // namespace mozilla
