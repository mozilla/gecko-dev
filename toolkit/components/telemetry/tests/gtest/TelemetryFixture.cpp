/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TelemetryFixture.h"
#include "mozilla/dom/SimpleGlobalObject.h"

using namespace mozilla;

void TelemetryTestFixture::SetUp() {
  ASSERT_FALSE(mSetupCalled)
  << "TelemetryTestFixture::Setup() called multiple times! This does not "
     "need to be called from TestSpecificSetUp().";
  mSetupCalled = true;
  mTelemetry = do_GetService("@mozilla.org/base/telemetry;1");

  // Run specific test setup here so mCleanGlobal won't get GC'd before the test
  // run starts.
  TestSpecificSetUp();

  mCleanGlobal = dom::SimpleGlobalObject::Create(
      dom::SimpleGlobalObject::GlobalType::BindingDetail);

  // The test must fail if we failed getting the global.
  ASSERT_NE(mCleanGlobal, nullptr)
      << "SimpleGlobalObject must return a valid global object.";
}

AutoJSContextWithGlobal::AutoJSContextWithGlobal(JSObject* aGlobalObject) {
  // The JS API must initialize correctly.
  JS::Rooted<JSObject*> globalObject(dom::RootingCx(), aGlobalObject);
  mJsAPI.emplace();
  MOZ_ALWAYS_TRUE(mJsAPI->Init(globalObject));
}

JSContext* AutoJSContextWithGlobal::GetJSContext() const {
  return mJsAPI->cx();
}
