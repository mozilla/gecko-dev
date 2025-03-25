/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/UniquePtr.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsIServiceWorkerManager.h"

#include "prtime.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {

struct HandlerStats {
  uint32_t swLoadCount;
  uint32_t swUpdatedCount;
  uint32_t swUnregisteredCount;
  nsCString lastValue;

  uint32_t swLoad2Count;
  uint32_t swUpdated2Count;
  uint32_t swUnregistered2Count;
  nsCString lastValue2;
};

MOZ_CONSTINIT mozilla::UniquePtr<HandlerStats> gHandlerStats;

void MaybeCreateHandlerStats() {
  if (!gHandlerStats) {
    gHandlerStats.reset(new HandlerStats());
  }
}

void swLoaded(const ServiceWorkerRegistrationData& data,
              const nsACString& aValue) {
  MaybeCreateHandlerStats();
  gHandlerStats->swLoadCount++;
  gHandlerStats->lastValue = aValue;
}

void swUpdated(const ServiceWorkerRegistrationData& data) {
  MaybeCreateHandlerStats();
  gHandlerStats->swUpdatedCount++;
}

void swUnregistered(const ServiceWorkerRegistrationData& data) {
  MaybeCreateHandlerStats();
  gHandlerStats->swUnregisteredCount++;
}

void swLoaded2(const ServiceWorkerRegistrationData& data,
               const nsACString& aValue) {
  MaybeCreateHandlerStats();
  gHandlerStats->swLoad2Count++;
  gHandlerStats->lastValue2 = aValue;
}

void swUpdated2(const ServiceWorkerRegistrationData& data) {
  MaybeCreateHandlerStats();
  gHandlerStats->swUpdated2Count++;
}

void swUnregistered2(const ServiceWorkerRegistrationData& data) {
  MaybeCreateHandlerStats();
  gHandlerStats->swUnregistered2Count++;
}

}  // namespace

class ServiceWorkerRegistrarTest : public ServiceWorkerRegistrar {
 public:
  ServiceWorkerRegistrarTest() {
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(mProfileDir));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    MOZ_RELEASE_ASSERT(mProfileDir);

    mExpandoHandlers.AppendElement(ExpandoHandler{
        nsCString("handler_test"), swLoaded, swUpdated, swUnregistered});
    mExpandoHandlers.AppendElement(ExpandoHandler{
        nsCString("handler_test2"), swLoaded2, swUpdated2, swUnregistered2});
  }

  nsresult TestReadData() { return ReadData(); }
  nsresult TestWriteData() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    return WriteData(mData);
  }
  void TestDeleteData() { DeleteData(); }

  void TestRegisterServiceWorker(const ServiceWorkerRegistrationData& aData) {
    mozilla::MonitorAutoLock lock(mMonitor);
    RegisterServiceWorkerInternal(aData);
  }

  nsTArray<ServiceWorkerData>& TestGetData() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    return mData;
  }
};

already_AddRefed<nsIFile> GetFile() {
  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  file->Append(nsLiteralString(SERVICEWORKERREGISTRAR_FILE));
  return file.forget();
}

bool CreateFile(const nsACString& aData) {
  nsCOMPtr<nsIFile> file = GetFile();

  nsCOMPtr<nsIOutputStream> stream;
  nsresult rv = NS_NewLocalFileOutputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t count;
  rv = stream->Write(aData.Data(), aData.Length(), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (count != aData.Length()) {
    return false;
  }

  return true;
}

TEST(ServiceWorkerRegistrar, TestNoFile)
{
  nsCOMPtr<nsIFile> file = GetFile();
  ASSERT_TRUE(file)
  << "GetFile must return a nsIFIle";

  bool exists;
  nsresult rv = file->Exists(&exists);
  ASSERT_EQ(NS_OK, rv) << "nsIFile::Exists cannot fail";

  if (exists) {
    rv = file->Remove(false);
    ASSERT_EQ(NS_OK, rv) << "nsIFile::Remove cannot fail";
  }

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length())
      << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestEmptyFile)
{
  ASSERT_TRUE(CreateFile(""_ns))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_NE(NS_OK, rv) << "ReadData() should fail if the file is empty";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length())
      << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestRightVersionFile)
{
  nsCString buffer;
  buffer.AppendInt(static_cast<uint32_t>(SERVICEWORKERREGISTRAR_VERSION));
  buffer.Append("\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv)
      << "ReadData() should not fail when the version is correct";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length())
      << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestWrongVersionFile)
{
  nsCString buffer;
  buffer.AppendInt(static_cast<uint32_t>(SERVICEWORKERREGISTRAR_VERSION));
  buffer.Append("bla\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_NE(NS_OK, rv)
      << "ReadData() should fail when the version is not correct";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length())
      << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestReadData)
{
  nsCString buffer;
  buffer.AppendInt(static_cast<uint32_t>(SERVICEWORKERREGISTRAR_VERSION));
  buffer.Append("\n");

  buffer.AppendLiteral("^inBrowser=1\n");
  buffer.AppendLiteral("https://scope_0.org\ncurrentWorkerURL 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TRUE "\n");
  buffer.AppendLiteral("cacheName 0\n");
  buffer.AppendInt(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
                   16);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("true\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("https://scope_1.org\ncurrentWorkerURL 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_FALSE "\n");
  buffer.AppendLiteral("cacheName 1\n");
  buffer.AppendInt(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL, 16);
  buffer.AppendLiteral("\n");
  PRTime ts = PR_Now();
  buffer.AppendInt(ts);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(ts);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(ts);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(1);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("false\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_TRUE(data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());
  ASSERT_EQ(false, data[0].mRegistration.navigationPreloadState().enabled());
  ASSERT_STREQ(
      "true",
      data[0].mRegistration.navigationPreloadState().headerValue().get());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  ASSERT_FALSE(data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)ts, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)ts, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)ts, data[1].mRegistration.lastUpdateTime());
  ASSERT_EQ(true, data[1].mRegistration.navigationPreloadState().enabled());
  ASSERT_STREQ(
      "false",
      data[1].mRegistration.navigationPreloadState().headerValue().get());
}

TEST(ServiceWorkerRegistrar, TestDeleteData)
{
  ASSERT_TRUE(CreateFile("Foobar"_ns))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  swr->TestDeleteData();

  nsCOMPtr<nsIFile> file = GetFile();

  bool exists;
  nsresult rv = file->Exists(&exists);
  ASSERT_EQ(NS_OK, rv) << "nsIFile::Exists cannot fail";

  ASSERT_FALSE(exists)
  << "The file should not exist after a DeleteData().";
}

TEST(ServiceWorkerRegistrar, TestWriteData)
{
  {
    RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

    ServiceWorkerRegistrationData reg;

    reg.scope() = "https://scope_write_0.org"_ns;
    reg.currentWorkerURL() = "currentWorkerURL write 0"_ns;
    reg.currentWorkerHandlesFetch() = true;
    reg.cacheName() = u"cacheName write 0"_ns;
    reg.updateViaCache() =
        nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

    reg.currentWorkerInstalledTime() = PR_Now();
    reg.currentWorkerActivatedTime() = PR_Now();
    reg.lastUpdateTime() = PR_Now();

    const auto spec = "spec write 0"_ns;
    reg.principal() = mozilla::ipc::ContentPrincipalInfo(
        mozilla::OriginAttributes(), spec, spec, mozilla::Nothing(), spec);

    swr->TestRegisterServiceWorker(reg);

    nsresult rv = swr->TestWriteData();
    ASSERT_EQ(NS_OK, rv) << "WriteData() should not fail";
  }

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& dataArr =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)1, dataArr.Length()) << "1 entries should be found";

  const auto& data = dataArr[0];

  ASSERT_EQ(data.mRegistration.principal().type(),
            mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
  const mozilla::ipc::ContentPrincipalInfo& cInfo =
      data.mRegistration.principal();

  mozilla::OriginAttributes attrs;
  nsAutoCString suffix, expectSuffix;
  attrs.CreateSuffix(expectSuffix);
  cInfo.attrs().CreateSuffix(suffix);

  ASSERT_STREQ(expectSuffix.get(), suffix.get());

  ASSERT_STREQ("https://scope_write_0.org", cInfo.spec().get());
  ASSERT_STREQ("https://scope_write_0.org", data.mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL write 0",
               data.mRegistration.currentWorkerURL().get());

  ASSERT_EQ(true, data.mRegistration.currentWorkerHandlesFetch());

  ASSERT_STREQ("cacheName write 0",
               NS_ConvertUTF16toUTF8(data.mRegistration.cacheName()).get());

  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data.mRegistration.updateViaCache());

  ASSERT_NE((int64_t)0, data.mRegistration.currentWorkerInstalledTime());
  ASSERT_NE((int64_t)0, data.mRegistration.currentWorkerActivatedTime());
  ASSERT_NE((int64_t)0, data.mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestVersion2Migration)
{
  nsAutoCString buffer(
      "2"
      "\n");

  buffer.AppendLiteral("^appId=123&inBrowser=1\n");
  buffer.AppendLiteral(
      "spec 0\nhttps://scope_0.org\nscriptSpec 0\ncurrentWorkerURL "
      "0\nactiveCache 0\nwaitingCache 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(
      "spec 1\nhttps://scope_1.org\nscriptSpec 1\ncurrentWorkerURL "
      "1\nactiveCache 1\nwaitingCache 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_EQ(true, data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("activeCache 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  ASSERT_EQ(true, data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("activeCache 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestVersion3Migration)
{
  nsAutoCString buffer(
      "3"
      "\n");

  buffer.AppendLiteral("^appId=123&inBrowser=1\n");
  buffer.AppendLiteral(
      "spec 0\nhttps://scope_0.org\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(
      "spec 1\nhttps://scope_1.org\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_EQ(true, data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  ASSERT_EQ(true, data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestVersion4Migration)
{
  nsAutoCString buffer(
      "4"
      "\n");

  buffer.AppendLiteral("^appId=123&inBrowser=1\n");
  buffer.AppendLiteral(
      "https://scope_0.org\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(
      "https://scope_1.org\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  // default is true
  ASSERT_EQ(true, data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  // default is true
  ASSERT_EQ(true, data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestVersion5Migration)
{
  nsAutoCString buffer(
      "5"
      "\n");

  buffer.AppendLiteral("^appId=123&inBrowser=1\n");
  buffer.AppendLiteral("https://scope_0.org\ncurrentWorkerURL 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TRUE "\n");
  buffer.AppendLiteral("cacheName 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("https://scope_1.org\ncurrentWorkerURL 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_FALSE "\n");
  buffer.AppendLiteral("cacheName 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_TRUE(data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  ASSERT_FALSE(data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestVersion6Migration)
{
  nsAutoCString buffer(
      "6"
      "\n");

  buffer.AppendLiteral("^appId=123&inBrowser=1\n");
  buffer.AppendLiteral("https://scope_0.org\ncurrentWorkerURL 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TRUE "\n");
  buffer.AppendLiteral("cacheName 0\n");
  buffer.AppendInt(nsIRequest::LOAD_NORMAL, 16);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("https://scope_1.org\ncurrentWorkerURL 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_FALSE "\n");
  buffer.AppendLiteral("cacheName 1\n");
  buffer.AppendInt(nsIRequest::VALIDATE_ALWAYS, 16);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_TRUE(data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  ASSERT_FALSE(data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestVersion7Migration)
{
  nsAutoCString buffer(
      "7"
      "\n");

  buffer.AppendLiteral("^appId=123&inBrowser=1\n");
  buffer.AppendLiteral("https://scope_0.org\ncurrentWorkerURL 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TRUE "\n");
  buffer.AppendLiteral("cacheName 0\n");
  buffer.AppendInt(nsIRequest::LOAD_NORMAL, 16);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("https://scope_1.org\ncurrentWorkerURL 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_FALSE "\n");
  buffer.AppendLiteral("cacheName 1\n");
  buffer.AppendInt(nsIRequest::VALIDATE_ALWAYS, 16);
  buffer.AppendLiteral("\n");
  PRTime ts = PR_Now();
  buffer.AppendInt(ts);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(ts);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(ts);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_TRUE(data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  ASSERT_FALSE(data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)ts, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)ts, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)ts, data[1].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestDedupeRead)
{
  nsAutoCString buffer(
      "3"
      "\n");

  // unique entries
  buffer.AppendLiteral("^inBrowser=1\n");
  buffer.AppendLiteral(
      "spec 0\nhttps://scope_0.org\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(
      "spec 1\nhttps://scope_1.org\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  // dupe entries
  buffer.AppendLiteral("^inBrowser=1\n");
  buffer.AppendLiteral(
      "spec 1\nhttps://scope_0.org\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("^inBrowser=1\n");
  buffer.AppendLiteral(
      "spec 2\nhttps://scope_0.org\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.AppendLiteral("\n");
  buffer.AppendLiteral(
      "spec 3\nhttps://scope_1.org\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].mRegistration.principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 =
      data[0].mRegistration.principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("", suffix0.get());
  ASSERT_STREQ("https://scope_0.org", cInfo0.spec().get());
  ASSERT_STREQ("https://scope_0.org", data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 0",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_EQ(true, data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].mRegistration.principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo)
      << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 =
      data[1].mRegistration.principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("https://scope_1.org", cInfo1.spec().get());
  ASSERT_STREQ("https://scope_1.org", data[1].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL 1",
               data[1].mRegistration.currentWorkerURL().get());
  ASSERT_EQ(true, data[1].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1",
               NS_ConvertUTF16toUTF8(data[1].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[1].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[1].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestDedupeWrite)
{
  {
    RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

    for (int i = 0; i < 2; ++i) {
      ServiceWorkerRegistrationData reg;

      reg.scope() = "https://scope_write.dedupe"_ns;
      reg.currentWorkerURL() = nsPrintfCString("currentWorkerURL write %d", i);
      reg.currentWorkerHandlesFetch() = true;
      reg.cacheName() =
          NS_ConvertUTF8toUTF16(nsPrintfCString("cacheName write %d", i));
      reg.updateViaCache() =
          nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

      nsAutoCString spec;
      spec.AppendPrintf("spec write dedupe/%d", i);

      reg.principal() = mozilla::ipc::ContentPrincipalInfo(
          mozilla::OriginAttributes(), spec, spec, mozilla::Nothing(), spec);

      swr->TestRegisterServiceWorker(reg);
    }

    nsresult rv = swr->TestWriteData();
    ASSERT_EQ(NS_OK, rv) << "WriteData() should not fail";
  }

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  // Duplicate entries should be removed.
  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>& data =
      swr->TestGetData();
  ASSERT_EQ((uint32_t)1, data.Length()) << "1 entry should be found";

  ASSERT_EQ(data[0].mRegistration.principal().type(),
            mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
  const mozilla::ipc::ContentPrincipalInfo& cInfo =
      data[0].mRegistration.principal();

  mozilla::OriginAttributes attrs;
  nsAutoCString suffix, expectSuffix;
  attrs.CreateSuffix(expectSuffix);
  cInfo.attrs().CreateSuffix(suffix);

  // Last entry passed to RegisterServiceWorkerInternal() should overwrite
  // previous values.  So expect "1" in values here.
  ASSERT_STREQ(expectSuffix.get(), suffix.get());
  ASSERT_STREQ("https://scope_write.dedupe", cInfo.spec().get());
  ASSERT_STREQ("https://scope_write.dedupe",
               data[0].mRegistration.scope().get());
  ASSERT_STREQ("currentWorkerURL write 1",
               data[0].mRegistration.currentWorkerURL().get());
  ASSERT_EQ(true, data[0].mRegistration.currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName write 1",
               NS_ConvertUTF16toUTF8(data[0].mRegistration.cacheName()).get());
  ASSERT_EQ(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
            data[0].mRegistration.updateViaCache());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerInstalledTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.currentWorkerActivatedTime());
  ASSERT_EQ((int64_t)0, data[0].mRegistration.lastUpdateTime());
}

TEST(ServiceWorkerRegistrar, TestLoadHandler)
{
  nsCString buffer;
  buffer.AppendInt(static_cast<uint32_t>(SERVICEWORKERREGISTRAR_VERSION));
  buffer.Append("\n");

  buffer.AppendLiteral("^inBrowser=1\n");
  buffer.AppendLiteral("https://scope_0.org\ncurrentWorkerURL 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TRUE "\n");
  buffer.AppendLiteral("cacheName 0\n");
  buffer.AppendInt(nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS,
                   16);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendInt(0);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("true\n");
  buffer.AppendInt(2);
  buffer.AppendLiteral("\n");
  buffer.AppendLiteral("handler_test\n");
  buffer.AppendLiteral("hello world!\n");
  buffer.AppendLiteral("handler_test2\n");
  buffer.AppendLiteral("hello\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer))
  << "CreateFile should not fail";

  {
    RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;
    nsresult rv = swr->TestReadData();
    ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

    ASSERT_EQ(gHandlerStats->swLoadCount, (uint32_t)1)
        << "First handle correctly called";
    ASSERT_EQ(gHandlerStats->lastValue, nsCString("hello world!"))
        << "First handle correctly called correct value)";
    ASSERT_EQ(gHandlerStats->swUnregisteredCount, (uint32_t)0)
        << "No unregister calls yet";

    ASSERT_EQ(gHandlerStats->swLoad2Count, (uint32_t)1)
        << "Second handle correctly called";
    ASSERT_EQ(gHandlerStats->lastValue2, nsCString("hello"))
        << "Second handle correctly called correct value)";
    ASSERT_EQ(gHandlerStats->swUnregistered2Count, (uint32_t)0)
        << "No unregister calls yet";

    rv = swr->TestWriteData();
    ASSERT_EQ(NS_OK, rv) << "WriteData() should not fail";
  }

  {
    RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;
    nsresult rv = swr->TestReadData();
    ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

    ASSERT_EQ(gHandlerStats->swLoadCount, (uint32_t)2)
        << "First handle correctly called";
    ASSERT_EQ(gHandlerStats->lastValue, nsCString("hello world!"))
        << "First handle correctly called correct value)";
    ASSERT_EQ(gHandlerStats->swUnregisteredCount, (uint32_t)0)
        << "No unregister calls yet";

    ASSERT_EQ(gHandlerStats->swLoad2Count, (uint32_t)2)
        << "Second handle correctly called";
    ASSERT_EQ(gHandlerStats->lastValue2, nsCString("hello"))
        << "Second handle correctly called correct value)";
    ASSERT_EQ(gHandlerStats->swUnregistered2Count, (uint32_t)0)
        << "No unregister calls yet";
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  return rv;
}
