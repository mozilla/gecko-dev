#include <utility>

#include "gtest/gtest.h"
#include "nsServiceManagerUtils.h"
#include "../../../xpcom/threads/nsThreadManager.h"
#include "nsIDHCPClient.h"
#include "mozilla/Preferences.h"
#include "nsComponentManager.h"
#include "nsIPrefService.h"
#include "nsNetCID.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/GenericFactory.h"
#include "../../base/nsPACMan.h"
#include "mozilla/StaticMutex.h"

#define TEST_WPAD_DHCP_OPTION "http://pac/pac.dat"
#define TEST_ASSIGNED_PAC_URL "http://assignedpac/pac.dat"
#define WPAD_PREF 4
#define NETWORK_PROXY_TYPE_PREF_NAME "network.proxy.type"
#define GETTING_NETWORK_PROXY_TYPE_FAILED (-1)

static mozilla::StaticMutex sMutex;
MOZ_CONSTINIT nsCString WPADOptionResult MOZ_GUARDED_BY(sMutex);

namespace mozilla {
namespace net {

nsresult SetNetworkProxyType(int32_t pref) {
  return Preferences::SetInt(NETWORK_PROXY_TYPE_PREF_NAME, pref);
}

nsresult GetNetworkProxyType(int32_t* pref) {
  return Preferences::GetInt(NETWORK_PROXY_TYPE_PREF_NAME, pref);
}

class nsTestDHCPClient final : public nsIDHCPClient {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDHCPCLIENT

  nsTestDHCPClient() = default;

  nsresult Init() { return NS_OK; };

 private:
  ~nsTestDHCPClient() = default;
};

NS_IMETHODIMP
nsTestDHCPClient::GetOption(uint8_t option, nsACString& _retval) {
  mozilla::StaticMutexAutoLock lock(sMutex);
  _retval.Assign(WPADOptionResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsTestDHCPClient, nsIDHCPClient)

#define NS_TESTDHCPCLIENTSERVICE_CID /* {FEBF1D69-4D7D-4891-9524-045AD18B5593} \
                                      */                                       \
  {0xFEBF1D69, 0x4D7D, 0x4891, {0x95, 0x24, 0x04, 0x5a, 0xd1, 0x8b, 0x55, 0x93}}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsTestDHCPClient, Init)
NS_DEFINE_NAMED_CID(NS_TESTDHCPCLIENTSERVICE_CID);

void SetOptionResult(const char* result) {
  mozilla::StaticMutexAutoLock lock(sMutex);
  WPADOptionResult.Assign(result);
}

class ProcessPendingEventsAction final : public Runnable {
 public:
  ProcessPendingEventsAction() : Runnable("net::ProcessPendingEventsAction") {}

  NS_IMETHOD
  Run() override {
    if (NS_HasPendingEvents(nullptr)) {
      NS_WARNING("Found pending requests on PAC thread");
      nsresult rv;
      rv = NS_ProcessPendingEvents(nullptr);
      EXPECT_EQ(NS_OK, rv);
    }
    NS_WARNING("No pending requests on PAC thread");
    return NS_OK;
  }
};

class TestPACMan : public ::testing::Test {
 protected:
  RefPtr<nsPACMan> mPACMan;

  void ProcessAllEvents() {
    ProcessPendingEventsOnPACThread();
    nsresult rv;
    while (NS_HasPendingEvents(nullptr)) {
      NS_WARNING("Pending events on main thread");
      rv = NS_ProcessPendingEvents(nullptr);
      ASSERT_EQ(NS_OK, rv);
      ProcessPendingEventsOnPACThread();
    }
    NS_WARNING("End of pending events on main thread");
  }

  // This method is used to ensure that all pending events on the main thread
  // and the Proxy thread are processsed.
  // It iterates over ProcessAllEvents because simply calling ProcessAllEvents
  // once did not reliably process the events on both threads on all platforms.
  void ProcessAllEventsTenTimes() {
    for (int i = 0; i < 10; i++) {
      ProcessAllEvents();
    }
  }

  virtual void SetUp() {
    Preferences::SetBool("network.proxy.dhcp_wpad_only_one_outstanding", false);
    Preferences::SetFloat("network.proxy.dhcp_wpad_timeout_sec", 30);
    ASSERT_EQ(NS_OK, GetNetworkProxyType(&originalNetworkProxyTypePref));
    nsCOMPtr<nsIFactory> factory;
    nsresult rv = nsComponentManagerImpl::gComponentManager->GetClassObject(
        kNS_TESTDHCPCLIENTSERVICE_CID, NS_GET_IID(nsIFactory),
        getter_AddRefs(factory));
    if (NS_SUCCEEDED(rv) && factory) {
      rv = nsComponentManagerImpl::gComponentManager->UnregisterFactory(
          kNS_TESTDHCPCLIENTSERVICE_CID, factory);
      ASSERT_EQ(NS_OK, rv);
    }
    factory = new mozilla::GenericFactory(nsTestDHCPClientConstructor);
    nsComponentManagerImpl::gComponentManager->RegisterFactory(
        kNS_TESTDHCPCLIENTSERVICE_CID, "nsTestDHCPClient",
        NS_DHCPCLIENT_CONTRACTID, factory);

    mPACMan = new nsPACMan(nullptr);
    mPACMan->SetWPADOverDHCPEnabled(true);
    mPACMan->Init(nullptr);
    ASSERT_EQ(NS_OK, SetNetworkProxyType(WPAD_PREF));
  }

  virtual void TearDown() {
    mPACMan->Shutdown();
    if (originalNetworkProxyTypePref != GETTING_NETWORK_PROXY_TYPE_FAILED) {
      ASSERT_EQ(NS_OK, SetNetworkProxyType(originalNetworkProxyTypePref));
    }
  }

  nsCOMPtr<nsIDHCPClient> GetPACManDHCPCient() { return mPACMan->mDHCPClient; }

  void SetPACManDHCPCient(nsCOMPtr<nsIDHCPClient> aValue) {
    mPACMan->mDHCPClient = std::move(aValue);
  }

  void AssertPACSpecEqualTo(const char* aExpected) {
    ASSERT_STREQ(aExpected, mPACMan->mPACURISpec.Data());
  }

 private:
  int32_t originalNetworkProxyTypePref = GETTING_NETWORK_PROXY_TYPE_FAILED;

  void ProcessPendingEventsOnPACThread() {
    RefPtr<ProcessPendingEventsAction> action =
        new ProcessPendingEventsAction();

    mPACMan->DispatchToPAC(action.forget(), /*aSync =*/true);
  }
};

TEST_F(TestPACMan, TestCreateDHCPClientAndGetOption) {
  SetOptionResult(TEST_WPAD_DHCP_OPTION);
  nsCString spec;

  GetPACManDHCPCient()->GetOption(252, spec);

  ASSERT_STREQ(TEST_WPAD_DHCP_OPTION, spec.Data());
}

TEST_F(TestPACMan, TestCreateDHCPClientAndGetEmptyOption) {
  SetOptionResult("");
  nsCString spec;
  spec.AssignLiteral(TEST_ASSIGNED_PAC_URL);

  GetPACManDHCPCient()->GetOption(252, spec);

  ASSERT_TRUE(spec.IsEmpty());
}

TEST_F(TestPACMan,
       WhenTheDHCPClientExistsAndDHCPIsNonEmptyDHCPOptionIsUsedAsPACUri) {
  SetOptionResult(TEST_WPAD_DHCP_OPTION);

  mPACMan->LoadPACFromURI(""_ns);
  ProcessAllEventsTenTimes();

  mozilla::StaticMutexAutoLock lock(sMutex);
  ASSERT_STREQ(TEST_WPAD_DHCP_OPTION, WPADOptionResult.Data());
  AssertPACSpecEqualTo(TEST_WPAD_DHCP_OPTION);
}

TEST_F(TestPACMan, WhenTheDHCPResponseIsEmptyWPADDefaultsToStandardURL) {
  SetOptionResult(""_ns.Data());

  mPACMan->LoadPACFromURI(""_ns);
  ASSERT_TRUE(NS_HasPendingEvents(nullptr));
  ProcessAllEventsTenTimes();

  mozilla::StaticMutexAutoLock lock(sMutex);
  ASSERT_STREQ("", WPADOptionResult.Data());
  AssertPACSpecEqualTo("http://wpad/wpad.dat");
}

TEST_F(TestPACMan, WhenThereIsNoDHCPClientWPADDefaultsToStandardURL) {
  SetOptionResult(TEST_WPAD_DHCP_OPTION);
  SetPACManDHCPCient(nullptr);

  mPACMan->LoadPACFromURI(""_ns);
  ProcessAllEventsTenTimes();

  mozilla::StaticMutexAutoLock lock(sMutex);
  ASSERT_STREQ(TEST_WPAD_DHCP_OPTION, WPADOptionResult.Data());
  AssertPACSpecEqualTo("http://wpad/wpad.dat");
}

TEST_F(TestPACMan, WhenWPADOverDHCPIsPreffedOffWPADDefaultsToStandardURL) {
  SetOptionResult(TEST_WPAD_DHCP_OPTION);
  mPACMan->SetWPADOverDHCPEnabled(false);

  mPACMan->LoadPACFromURI(""_ns);
  ProcessAllEventsTenTimes();

  mozilla::StaticMutexAutoLock lock(sMutex);
  ASSERT_STREQ(TEST_WPAD_DHCP_OPTION, WPADOptionResult.Data());
  AssertPACSpecEqualTo("http://wpad/wpad.dat");
}

TEST_F(TestPACMan, WhenPACUriIsSetDirectlyItIsUsedRatherThanWPAD) {
  SetOptionResult(TEST_WPAD_DHCP_OPTION);
  nsCString spec;
  spec.AssignLiteral(TEST_ASSIGNED_PAC_URL);

  mPACMan->LoadPACFromURI(spec);
  ProcessAllEventsTenTimes();

  AssertPACSpecEqualTo(TEST_ASSIGNED_PAC_URL);
}

}  // namespace net
}  // namespace mozilla
