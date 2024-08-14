/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef TelemetryFixture_h_
#define TelemetryFixture_h_

#include "gtest/gtest.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Maybe.h"
#include "nsITelemetry.h"

class TelemetryTestFixture : public ::testing::Test {
 protected:
  TelemetryTestFixture() = default;
  virtual void SetUp() final;
  virtual void TestSpecificSetUp() {};

  JSObject* mCleanGlobal = nullptr;

  nsCOMPtr<nsITelemetry> mTelemetry;
  bool mSetupCalled = false;
};

// AutoJSAPI is annotated with MOZ_STACK_CLASS and thus cannot be
// used as a member of TelemetryTestFixture, since gtest instantiates
// that on the heap. To work around the problem, use the following class
// at the beginning of each Telemetry test.
// Note: this is very similar to AutoJSContext, but it allows to pass a
// global JS object in.
class MOZ_RAII AutoJSContextWithGlobal {
 public:
  explicit AutoJSContextWithGlobal(JSObject* aGlobalObject);
  JSContext* GetJSContext() const;

 protected:
  mozilla::Maybe<mozilla::dom::AutoJSAPI> mJsAPI;
};

#endif  // TelemetryFixture_h_
