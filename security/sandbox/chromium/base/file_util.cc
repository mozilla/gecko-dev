// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#if defined(OS_WIN)
#include <io.h>
#endif
#include <stdio.h>

#include <fstream>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace base {

namespace {

const FilePath::CharType kExtensionSeparator = FILE_PATH_LITERAL('.');

// The maximum number of 'uniquified' files we will try to create.
// This is used when the filename we're trying to download is already in use,
// so we create a new unique filename by appending " (nnn)" before the
// extension, where 1 <= nnn <= kMaxUniqueFiles.
// Also used by code that cleans up said files.
static const int kMaxUniqueFiles = 100;

}  // namespace

bool g_bug108724_debug = false;

int64 ComputeDirectorySize(const FilePath& root_path) {
  int64 running_size = 0;
  FileEnumerator file_iter(root_path, true, FileEnumerator::FILES);
  while (!file_iter.Next().empty())
    running_size += file_iter.GetInfo().GetSize();
  return running_size;
}

bool Move(const FilePath& from_path, const FilePath& to_path) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;
  return internal::MoveUnsafe(from_path, to_path);
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;
  return internal::CopyFileUnsafe(from_path, to_path);
}

bool ContentsEqual(const FilePath& filename1, const FilePath& filename2) {
  // We open the file in binary format even if they are text files because
  // we are just comparing that bytes are exactly same in both files and not
  // doing anything smart with text formatting.
  std::ifstream file1(filename1.value().c_str(),
                      std::ios::in | std::ios::binary);
  std::ifstream file2(filename2.value().c_str(),
                      std::ios::in | std::ios::binary);

  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open())
    return false;

  const int BUFFER_SIZE = 2056;
  char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
  do {
    file1.read(buffer1, BUFFER_SIZE);
    file2.read(buffer2, BUFFER_SIZE);

    if ((file1.eof() != file2.eof()) ||
        (file1.gcount() != file2.gcount()) ||
        (memcmp(buffer1, buffer2, file1.gcount()))) {
      file1.close();
      file2.close();
      return false;
    }
  } while (!file1.eof() || !file2.eof());

  file1.close();
  file2.close();
  return true;
}

bool TextContentsEqual(const FilePath& filename1, const FilePath& filename2) {
  std::ifstream file1(filename1.value().c_str(), std::ios::in);
  std::ifstream file2(filename2.value().c_str(), std::ios::in);

  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open())
    return false;

  do {
    std::string line1, line2;
    getline(file1, line1);
    getline(file2, line2);

    // Check for mismatched EOF states, or any error state.
    if ((file1.eof() != file2.eof()) ||
        file1.bad() || file2.bad()) {
      return false;
    }

    // Trim all '\r' and '\n' characters from the end of the line.
    std::string::size_type end1 = line1.find_last_not_of("\r\n");
    if (end1 == std::string::npos)
      line1.clear();
    else if (end1 + 1 < line1.length())
      line1.erase(end1 + 1);

    std::string::size_type end2 = line2.find_last_not_of("\r\n");
    if (end2 == std::string::npos)
      line2.clear();
    else if (end2 + 1 < line2.length())
      line2.erase(end2 + 1);

    if (line1 != line2)
      return false;
  } while (!file1.eof() || !file2.eof());

  return true;
}

bool ReadFileToString(const FilePath& path, std::string* contents) {
  if (path.ReferencesParent())
    return false;
  FILE* file = file_util::OpenFile(path, "rb");
  if (!file) {
    return false;
  }

  char buf[1 << 16];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), file)) > 0) {
    if (contents)
      contents->append(buf, len);
  }
  file_util::CloseFile(file);

  return true;
}

}  // namespace base

// -----------------------------------------------------------------------------

namespace file_util {

using base::FileEnumerator;
using base::FilePath;
using base::kExtensionSeparator;
using base::kMaxUniqueFiles;

bool IsDirectoryEmpty(const FilePath& dir_path) {
  FileEnumerator files(dir_path, false,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  if (files.Next().empty())
    return true;
  return false;
}

FILE* CreateAndOpenTemporaryFile(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory))
    return NULL;

  return CreateAndOpenTemporaryFileInDir(directory, path);
}

bool CreateDirectory(const base::FilePath& full_path) {
  return CreateDirectoryAndGetError(full_path, NULL);
}

bool GetFileSize(const FilePath& file_path, int64* file_size) {
  base::PlatformFileInfo info;
  if (!GetFileInfo(file_path, &info))
    return false;
  *file_size = info.size;
  return true;
}

bool TouchFile(const FilePath& path,
               const base::Time& last_accessed,
               const base::Time& last_modified) {
  int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE_ATTRIBUTES;

#if defined(OS_WIN)
  // On Windows, FILE_FLAG_BACKUP_SEMANTICS is needed to open a directory.
  if (DirectoryExists(path))
    flags |= base::PLATFORM_FILE_BACKUP_SEMANTICS;
#endif  // OS_WIN

  const base::PlatformFile file =
      base::CreatePlatformFile(path, flags, NULL, NULL);
  if (file != base::kInvalidPlatformFileValue) {
    bool result = base::TouchPlatformFile(file, last_accessed, last_modified);
    base::ClosePlatformFile(file);
    return result;
  }

  return false;
}

bool SetLastModifiedTime(const FilePath& path,
                         const base::Time& last_modified) {
  return TouchFile(path, last_modified, last_modified);
}

bool CloseFile(FILE* file) {
  if (file == NULL)
    return true;
  return fclose(file) == 0;
}

bool TruncateFile(FILE* file) {
  if (file == NULL)
    return false;
  long current_offset = ftell(file);
  if (current_offset == -1)
    return false;
#if defined(OS_WIN)
  int fd = _fileno(file);
  if (_chsize(fd, current_offset) != 0)
    return false;
#else
  int fd = fileno(file);
  if (ftruncate(fd, current_offset) != 0)
    return false;
#endif
  return true;
}

int GetUniquePathNumber(
    const FilePath& path,
    const FilePath::StringType& suffix) {
  bool have_suffix = !suffix.empty();
  if (!PathExists(path) &&
      (!have_suffix || !PathExists(FilePath(path.value() + suffix)))) {
    return 0;
  }

  FilePath new_path;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    new_path =
        path.InsertBeforeExtensionASCII(base::StringPrintf(" (%d)", count));
    if (!PathExists(new_path) &&
        (!have_suffix || !PathExists(FilePath(new_path.value() + suffix)))) {
      return count;
    }
  }

  return -1;
}

}  // namespace file_util
