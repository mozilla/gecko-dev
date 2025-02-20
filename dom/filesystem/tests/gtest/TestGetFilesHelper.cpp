/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsIFile.h"
#include "SpecialSystemDirectory.h"

using namespace mozilla;
using namespace mozilla::dom;

nsCOMPtr<nsIFile> MakeFileFromPathSegments(
    const nsTArray<const char*>& aPathSegments) {
  nsCOMPtr<nsIFile> file;
  MOZ_ALWAYS_SUCCEEDS(GetSpecialSystemDirectory(OS_CurrentWorkingDirectory,
                                                getter_AddRefs(file)));
  for (const auto* pathSegment : aPathSegments) {
    MOZ_ALWAYS_SUCCEEDS(
        file->AppendRelativePath(NS_ConvertASCIItoUTF16(pathSegment)));
  }
  return file;
}

nsresult AppendFileOrDirectory(nsTArray<OwningFileOrDirectory>& aDirectories,
                               const nsTArray<const char*>& aPathSegments) {
  nsCOMPtr<nsIFile> file = MakeFileFromPathSegments(aPathSegments);
  bool exists;
  MOZ_ALWAYS_SUCCEEDS(file->Exists(&exists));
  NS_ENSURE_TRUE(exists, NS_ERROR_FILE_NOT_FOUND);
  bool isDir;
  MOZ_ALWAYS_SUCCEEDS(file->IsDirectory(&isDir));
  if (isDir) {
    // We just need to iterate over the directory, so use the junk scope
    RefPtr<Directory> directory =
        Directory::Create(xpc::NativeGlobal(xpc::PrivilegedJunkScope()), file);
    NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

    OwningFileOrDirectory* owningDirectory = aDirectories.EmplaceBack();
    owningDirectory->SetAsDirectory() = directory;
  } else {
    OwningFileOrDirectory* owningDirectory = aDirectories.EmplaceBack();
    RefPtr<File> fileObject = File::CreateFromFile(
        xpc::NativeGlobal(xpc::PrivilegedJunkScope()), file);
    owningDirectory->SetAsFile() = fileObject;
  }
  return NS_OK;
}

struct BoolStruct {
  bool mValue = false;
};

class FilesCallback : public GetFilesCallback {
 public:
  FilesCallback(std::atomic<bool>& aGotResponse,
                const nsTArray<nsString>& aExpectedPaths)
      : mGotResponse(aGotResponse), mExpectedPaths(aExpectedPaths.Clone()) {}

  // -------------------
  // GetFilesCallback
  // -------------------
  void Callback(nsresult aStatus,
                const FallibleTArray<RefPtr<BlobImpl>>& aBlobImpls) override {
    EXPECT_EQ(aBlobImpls.Length(), mExpectedPaths.Length());
    for (const auto& blob : aBlobImpls) {
      nsString path;
      ErrorResult error;
      blob->GetMozFullPathInternal(path, error);
      ASSERT_EQ(error.StealNSResult(), NS_OK);
      ASSERT_TRUE(mExpectedPaths.Contains(path));
    }
    mGotResponse = true;
  }

 private:
  std::atomic<bool>& mGotResponse;
  nsTArray<nsString> mExpectedPaths;
};

nsTArray<nsString> GetExpectedPaths(
    const nsTArray<nsTArray<const char*>>& aPathSegmentsArray) {
  nsTArray<nsString> expectedPaths(aPathSegmentsArray.Length());
  for (const auto& pathSegments : aPathSegmentsArray) {
    auto file = MakeFileFromPathSegments(pathSegments);
    nsString expectedPath;
    MOZ_ALWAYS_SUCCEEDS(file->GetPath(expectedPath));
    expectedPaths.AppendElement(expectedPath);
  }
  return expectedPaths;
}

void ExpectGetFilesHelperResponse(
    RefPtr<GetFilesHelper> aHelper,
    const nsTArray<nsTArray<const char*>>& aPathSegmentsArray) {
  nsTArray<nsString> expectedPaths = GetExpectedPaths(aPathSegmentsArray);

  std::atomic<bool> gotCallbackResponse = false;
  std::atomic<bool> gotMozPromiseResponse = false;
  RefPtr<FilesCallback> callback =
      MakeRefPtr<FilesCallback>(gotCallbackResponse, expectedPaths);
  aHelper->AddCallback(callback);
  auto mozPromise = MakeRefPtr<GetFilesHelper::MozPromiseType>(__func__);
  aHelper->AddMozPromise(mozPromise,
                         xpc::NativeGlobal(xpc::PrivilegedJunkScope()));
  mozPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [&gotMozPromiseResponse,
       &expectedPaths](const nsTArray<RefPtr<mozilla::dom::File>>& aFiles) {
        EXPECT_EQ(aFiles.Length(), expectedPaths.Length());
        for (const auto& file : aFiles) {
          nsString path;
          ErrorResult error;
          file->GetMozFullPathInternal(path, error);
          ASSERT_EQ(error.StealNSResult(), NS_OK);
          ASSERT_TRUE(expectedPaths.Contains(path));
        }
        gotMozPromiseResponse = true;
      },
      []() { FAIL() << "MozPromise got rejected!"; });
  // Make timedOut a RefPtr so if we get a response after this function
  // has finished we can safely check that (and don't start accessing stack
  // values that don't exist anymore)
  RefPtr timedOut = MakeRefPtr<media::Refcountable<BoolStruct>>();

  RefPtr<CancelableRunnable> timer =
      NS_NewCancelableRunnableFunction("GetFilesHelper timeout", [&] {
        if (!gotCallbackResponse.load() || !gotMozPromiseResponse.load()) {
          timedOut->mValue = true;
        }
      });
  constexpr uint32_t kTimeout = 10000;
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), kTimeout);
  mozilla::SpinEventLoopUntil(
      "Waiting for GetFilesHelper result"_ns, [&, timedOut]() {
        return (gotCallbackResponse.load() && gotMozPromiseResponse.load()) ||
               timedOut->mValue;
      });
  timer->Cancel();
  EXPECT_TRUE(gotCallbackResponse);
  EXPECT_TRUE(gotMozPromiseResponse);
  EXPECT_FALSE(timedOut->mValue);
}

TEST(GetFilesHelper, TestSingleDirectory)
{
  nsTArray<OwningFileOrDirectory> directories;
  MOZ_ALWAYS_SUCCEEDS(
      AppendFileOrDirectory(directories, {"getfiles", "inner2"}));

  ErrorResult error;
  RefPtr<GetFilesHelper> helper =
      GetFilesHelper::Create(directories, true, error);
  ASSERT_EQ(error.StealNSResult(), NS_OK);

  nsTArray<nsTArray<const char*>> pathSegmentsArray;
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner2", "fileinner2.txt"});
  ExpectGetFilesHelperResponse(helper, pathSegmentsArray);
}

TEST(GetFilesHelper, TestSingleNestedDirectory)
{
  nsTArray<OwningFileOrDirectory> directories;
  MOZ_ALWAYS_SUCCEEDS(
      AppendFileOrDirectory(directories, {"getfiles", "inner1"}));

  ErrorResult error;
  RefPtr<GetFilesHelper> helper =
      GetFilesHelper::Create(directories, true, error);
  ASSERT_EQ(error.StealNSResult(), NS_OK);

  nsTArray<nsTArray<const char*>> pathSegmentsArray;
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "fileinner1.txt"});
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "inner", "fileinnerinner1.txt"});
  ExpectGetFilesHelperResponse(helper, pathSegmentsArray);
}

TEST(GetFilesHelper, TestSingleNestedDirectoryNoRecursion)
{
  nsTArray<OwningFileOrDirectory> directories;
  MOZ_ALWAYS_SUCCEEDS(
      AppendFileOrDirectory(directories, {"getfiles", "inner1"}));

  ErrorResult error;
  RefPtr<GetFilesHelper> helper =
      GetFilesHelper::Create(directories, false, error);
  ASSERT_EQ(error.StealNSResult(), NS_OK);

  nsTArray<nsTArray<const char*>> pathSegmentsArray;
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "fileinner1.txt"});
  ExpectGetFilesHelperResponse(helper, pathSegmentsArray);
}

TEST(GetFilesHelper, TestSingleDirectoryWithMultipleNestedChildren)
{
  nsTArray<OwningFileOrDirectory> directories;
  MOZ_ALWAYS_SUCCEEDS(AppendFileOrDirectory(directories, {"getfiles"}));

  ErrorResult error;
  RefPtr<GetFilesHelper> helper =
      GetFilesHelper::Create(directories, true, error);
  ASSERT_EQ(error.StealNSResult(), NS_OK);

  nsTArray<nsTArray<const char*>> pathSegmentsArray;
  pathSegmentsArray.AppendElement(nsTArray{"getfiles", "file1.txt"});
  pathSegmentsArray.AppendElement(nsTArray{"getfiles", "file2.txt"});
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "fileinner1.txt"});
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "inner", "fileinnerinner1.txt"});
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner2", "fileinner2.txt"});
  ExpectGetFilesHelperResponse(helper, pathSegmentsArray);
}

TEST(GetFilesHelper, TestSingleFile)
{
  nsTArray<OwningFileOrDirectory> directories;
  MOZ_ALWAYS_SUCCEEDS(AppendFileOrDirectory(
      directories, {"getfiles", "inner1", "fileinner1.txt"}));

  ErrorResult error;
  RefPtr<GetFilesHelper> helper =
      GetFilesHelper::Create(directories, true, error);
  ASSERT_EQ(error.StealNSResult(), NS_OK);

  nsTArray<nsTArray<const char*>> pathSegmentsArray;
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "fileinner1.txt"});
  ExpectGetFilesHelperResponse(helper, pathSegmentsArray);
}

TEST(GetFilesHelper, TestMultipleFiles)
{
  nsTArray<OwningFileOrDirectory> directories;
  MOZ_ALWAYS_SUCCEEDS(AppendFileOrDirectory(
      directories, {"getfiles", "inner1", "fileinner1.txt"}));
  MOZ_ALWAYS_SUCCEEDS(AppendFileOrDirectory(
      directories, {"getfiles", "inner2", "fileinner2.txt"}));

  ErrorResult error;
  RefPtr<GetFilesHelper> helper =
      GetFilesHelper::Create(directories, true, error);
  ASSERT_EQ(error.StealNSResult(), NS_OK);

  nsTArray<nsTArray<const char*>> pathSegmentsArray;
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "fileinner1.txt"});
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner2", "fileinner2.txt"});
  ExpectGetFilesHelperResponse(helper, pathSegmentsArray);
}

// Content Analysis can use GetFilesHelper with multiple directories,
// so make sure that works.
TEST(GetFilesHelper, TestMultipleDirectories)
{
  nsTArray<OwningFileOrDirectory> directories;
  MOZ_ALWAYS_SUCCEEDS(
      AppendFileOrDirectory(directories, {"getfiles", "inner1"}));
  MOZ_ALWAYS_SUCCEEDS(
      AppendFileOrDirectory(directories, {"getfiles", "inner2"}));

  ErrorResult error;
  RefPtr<GetFilesHelper> helper =
      GetFilesHelper::Create(directories, true, error);
  ASSERT_EQ(error.StealNSResult(), NS_OK);

  nsTArray<nsTArray<const char*>> pathSegmentsArray;
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "fileinner1.txt"});
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner1", "inner", "fileinnerinner1.txt"});
  pathSegmentsArray.AppendElement(
      nsTArray{"getfiles", "inner2", "fileinner2.txt"});
  ExpectGetFilesHelperResponse(helper, pathSegmentsArray);
}
