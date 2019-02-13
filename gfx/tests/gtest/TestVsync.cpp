/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "MainThreadUtils.h"
#include "nsIThread.h"
#include "nsRefPtr.h"
#include "SoftwareVsyncSource.h"
#include "VsyncSource.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/VsyncDispatcher.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using ::testing::_;

// Timeout for vsync events to occur in milliseconds
const int kVsyncTimeoutMS = 50;

class TestVsyncObserver : public VsyncObserver {
public:
  TestVsyncObserver()
    : mDidGetVsyncNotification(false)
    , mVsyncMonitor("VsyncMonitor")
  {
  }

  virtual bool NotifyVsync(TimeStamp aVsyncTimeStamp) override {
    MonitorAutoLock lock(mVsyncMonitor);
    mDidGetVsyncNotification = true;
    mVsyncMonitor.Notify();
    return true;
  }

  void WaitForVsyncNotification()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (DidGetVsyncNotification()) {
      return;
    }

    { // scope lock
      MonitorAutoLock lock(mVsyncMonitor);
      PRIntervalTime timeout = PR_MillisecondsToInterval(kVsyncTimeoutMS);
      lock.Wait(timeout);
    }
  }

  bool DidGetVsyncNotification()
  {
    MonitorAutoLock lock(mVsyncMonitor);
    return mDidGetVsyncNotification;
  }

  void ResetVsyncNotification()
  {
    MonitorAutoLock lock(mVsyncMonitor);
    mDidGetVsyncNotification = false;
  }

private:
  bool mDidGetVsyncNotification;

private:
  Monitor mVsyncMonitor;
};

class VsyncTester : public ::testing::Test {
protected:
  explicit VsyncTester()
  {
    gfxPlatform::GetPlatform();
    gfxPrefs::GetSingleton();
    if (gfxPrefs::HardwareVsyncEnabled() ) {
      mVsyncSource = gfxPlatform::GetPlatform()->GetHardwareVsync();
      MOZ_RELEASE_ASSERT(mVsyncSource);
    }
  }

  virtual ~VsyncTester()
  {
    mVsyncSource = nullptr;
  }

  nsRefPtr<VsyncSource> mVsyncSource;
};

static void
FlushMainThreadLoop()
{
  // Some tasks are pushed onto the main thread when adding vsync observers
  // This function will ensure all tasks are executed on the main thread
  // before returning.
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = NS_OK;
  bool processed = true;
  while (processed && NS_SUCCEEDED(rv)) {
    rv = mainThread->ProcessNextEvent(false, &processed);
  }
}

// Tests that we can enable/disable vsync notifications
TEST_F(VsyncTester, EnableVsync)
{
  if (!gfxPrefs::HardwareVsyncEnabled()) {
    return;
  }

  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  globalDisplay.EnableVsync();
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());
}

// Test that if we have vsync enabled, the display should get vsync notifications
TEST_F(VsyncTester, CompositorGetVsyncNotifications)
{
  if (!gfxPrefs::HardwareVsyncEnabled() || !gfxPrefs::VsyncAlignedCompositor()) {
    return;
  }

  CompositorVsyncDispatcher::SetThreadAssertionsEnabled(false);

  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  nsRefPtr<CompositorVsyncDispatcher> vsyncDispatcher = new CompositorVsyncDispatcher();
  nsRefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();

  vsyncDispatcher->SetCompositorVsyncObserver(testVsyncObserver);
  FlushMainThreadLoop();
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher = nullptr;
  testVsyncObserver = nullptr;
}

// Test that if we have vsync enabled, the parent refresh driver should get notifications
TEST_F(VsyncTester, ParentRefreshDriverGetVsyncNotifications)
{
  if (!gfxPrefs::HardwareVsyncEnabled() || !gfxPrefs::VsyncAlignedRefreshDriver()) {
    return;
  }

  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  nsRefPtr<RefreshTimerVsyncDispatcher> vsyncDispatcher = globalDisplay.GetRefreshTimerVsyncDispatcher();
  ASSERT_TRUE(vsyncDispatcher != nullptr);

  nsRefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();
  vsyncDispatcher->SetParentRefreshTimer(testVsyncObserver);
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());
  vsyncDispatcher->SetParentRefreshTimer(nullptr);

  testVsyncObserver->ResetVsyncNotification();
  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_FALSE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher = nullptr;
  testVsyncObserver = nullptr;
}

// Test that child refresh vsync observers get vsync notifications
TEST_F(VsyncTester, ChildRefreshDriverGetVsyncNotifications)
{
  if (!gfxPrefs::HardwareVsyncEnabled() || !gfxPrefs::VsyncAlignedRefreshDriver()) {
    return;
  }

  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  nsRefPtr<RefreshTimerVsyncDispatcher> vsyncDispatcher = globalDisplay.GetRefreshTimerVsyncDispatcher();
  ASSERT_TRUE(vsyncDispatcher != nullptr);

  nsRefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();
  vsyncDispatcher->AddChildRefreshTimer(testVsyncObserver);
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher->RemoveChildRefreshTimer(testVsyncObserver);
  testVsyncObserver->ResetVsyncNotification();
  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_FALSE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher = nullptr;
  testVsyncObserver = nullptr;
}
