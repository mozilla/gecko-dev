/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "core/TelemetryEvent.h"
#include "gtest/gtest.h"
#include "js/Array.h"               // JS::GetArrayLength
#include "js/PropertyAndElement.h"  // JS_GetElement, JS_GetProperty
#include "mozilla/Maybe.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/glean/GleanMetrics.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;

// Test that we can properly record events using the C++ API.
TEST_F(TelemetryTestFixture, RecordEventNative) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Make sure we don't get events from other tests.
  Unused << mTelemetry->ClearEvents();

  const nsLiteralCString category("telemetry.test");
  const nsLiteralCString method("test1");
  const nsLiteralCString method2("test2");
  const nsLiteralCString object("object1");
  const nsLiteralCString object2("object2");
  const nsLiteralCString value("value");
  const nsLiteralCString valueLong(
      "this value is much too long and must be truncated to fit in the limit "
      "which at time of writing was 80 bytes.");
  const nsLiteralCString extraKey("key1");
  const nsLiteralCString extraValue("extra value");
  const nsLiteralCString extraValueLong(
      "this extra value is much too long and must be truncated to fit in the "
      "limit which at time of writing was 80 bytes.");

  // Try recording.
  TelemetryEvent::RecordEventNative(
      Telemetry::EventID::TelemetryTest_Test2_Object1, Nothing(), Nothing());

  // Try recording with normal value, extra
  CopyableTArray<EventExtraEntry> extra(
      {EventExtraEntry{extraKey, extraValue}});
  TelemetryEvent::RecordEventNative(
      Telemetry::EventID::TelemetryTest_Test1_Object2, mozilla::Some(value),
      mozilla::Some(extra));

  // Try recording with too-long value, extra
  CopyableTArray<EventExtraEntry> longish(
      {EventExtraEntry{extraKey, extraValueLong}});
  TelemetryEvent::RecordEventNative(
      Telemetry::EventID::TelemetryTest_Test2_Object2, mozilla::Some(valueLong),
      mozilla::Some(longish));

  JS::Rooted<JS::Value> eventsSnapshot(cx.GetJSContext());
  GetEventSnapshot(cx.GetJSContext(), &eventsSnapshot);

  ASSERT_TRUE(!EventPresent(cx.GetJSContext(), eventsSnapshot, category, method,
                            object))
  << "Test event must not be present when recorded before enabled.";
  ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, category, method2,
                           object))
  << "Test event must be present.";
  ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, category, method,
                           object2))
  << "Test event with value and extra must be present.";
  ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, category, method2,
                           object2))
  << "Test event with truncated value and extra must be present.";

  // Ensure that the truncations happened appropriately.
  JSContext* aCx = cx.GetJSContext();
  JS::Rooted<JSObject*> arrayObj(aCx, &eventsSnapshot.toObject());
  JS::Rooted<JS::Value> eventRecord(aCx);
  ASSERT_TRUE(JS_GetElement(aCx, arrayObj, 2, &eventRecord))
  << "Must be able to get record.";
  JS::Rooted<JSObject*> recordArray(aCx, &eventRecord.toObject());
  uint32_t recordLength;
  ASSERT_TRUE(JS::GetArrayLength(aCx, recordArray, &recordLength))
  << "Event record array must have length.";
  ASSERT_TRUE(recordLength == 6)
  << "Event record must have 6 elements.";

  JS::Rooted<JS::Value> str(aCx);
  nsAutoJSString jsStr;
  // The value string is at index 4
  ASSERT_TRUE(JS_GetElement(aCx, recordArray, 4, &str))
  << "Must be able to get value.";
  ASSERT_TRUE(jsStr.init(aCx, str))
  << "Value must be able to be init'd to a jsstring.";
  ASSERT_EQ(NS_ConvertUTF16toUTF8(jsStr).Length(), (uint32_t)80)
      << "Value must have been truncated to 80 bytes.";

  // Extra is at index 5
  JS::Rooted<JS::Value> obj(aCx);
  ASSERT_TRUE(JS_GetElement(aCx, recordArray, 5, &obj))
  << "Must be able to get extra.";
  JS::Rooted<JSObject*> extraObj(aCx, &obj.toObject());
  JS::Rooted<JS::Value> extraVal(aCx);
  ASSERT_TRUE(JS_GetProperty(aCx, extraObj, extraKey.get(), &extraVal))
  << "Must be able to get the extra key's value.";
  ASSERT_TRUE(jsStr.init(aCx, extraVal))
  << "Extra must be able to be init'd to a jsstring.";
  ASSERT_EQ(NS_ConvertUTF16toUTF8(jsStr).Length(), (uint32_t)80)
      << "Extra must have been truncated to 80 bytes.";
}

using mozilla::Some;
using mozilla::glean::test_only_ipc::AnEventExtra;
// Test that we get a mirrored value from the C++ Glean API.
TEST_F(TelemetryTestFixture, GIFFTValue) {
  // Reset FOG to clear the stores.
  const nsCString empty;
  mozilla::glean::impl::fog_test_reset(&empty, &empty);

  // Make sure we don't get events from other tests.
  Unused << mTelemetry->ClearEvents();

  // Enable recording of telemetry.test events.
  const nsCString category("telemetry.test");
  const nsCString method("mirror_with_extra");
  const nsCString object("object1");

  // Record in Glean.
  // We include an extra extra key (extra1, here) to ensure there's always six
  // items in the record array.
  AnEventExtra extra = {
      .extra1 = Some("a"_ns),
      .value = Some("some value"_ns),
  };
  mozilla::glean::test_only_ipc::an_event.Record(Some(extra));
  auto optEvents =
      mozilla::glean::test_only_ipc::an_event.TestGetValue().unwrap();
  ASSERT_TRUE(optEvents.isSome())
  << "There are Glean events.";
  auto events = optEvents.extract();
  ASSERT_EQ(1UL, events.Length()) << "There is exactly one Glean event.";

  // Assert in Telemetry.
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JS::Rooted<JS::Value> eventsSnapshot(cx.GetJSContext());
  GetEventSnapshot(cx.GetJSContext(), &eventsSnapshot);

  ASSERT_TRUE(
      EventPresent(cx.GetJSContext(), eventsSnapshot, category, method, object))
  << "Test event must be present.";

  // Ensure that the `value` value was mapped appropriately.
  JSContext* aCx = cx.GetJSContext();
  JS::Rooted<JSObject*> arrayObj(aCx, &eventsSnapshot.toObject());
  JS::Rooted<JS::Value> eventRecord(aCx);
  ASSERT_TRUE(JS_GetElement(aCx, arrayObj, 0, &eventRecord))
  << "Must be able to get record.";
  JS::Rooted<JSObject*> recordArray(aCx, &eventRecord.toObject());
  uint32_t recordLength;
  ASSERT_TRUE(JS::GetArrayLength(aCx, recordArray, &recordLength))
  << "Event record array must have length.";
  ASSERT_EQ(6UL, recordLength) << "Event record array must have 6 elements.";

  JS::Rooted<JS::Value> str(aCx);
  nsAutoJSString jsStr;
  // The value string is at index 4
  ASSERT_TRUE(JS_GetElement(aCx, recordArray, 4, &str))
  << "Must be able to get value.";
  ASSERT_TRUE(jsStr.init(aCx, str))
  << "Value must be able to be init'd to a jsstring.";
  ASSERT_STREQ(NS_ConvertUTF16toUTF8(jsStr).get(), "some value")
      << "The value of the value must be 'some value'";
}
