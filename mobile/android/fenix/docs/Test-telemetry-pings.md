# Test Telemetry Pings

You can view your test pings [here](https://debug-ping-preview.firebaseapp.com/). Note that there is also a button within the Glean Debug Tools to access this link.
For instructions on how to send test pings, use one of the methods below. Note that before version 132,
you can only use method 2.

***Method 1 (Debug Drawer)***

Since version 132, telemetry pings can be tested via the Glean Debug Tools feature.

1. Click on the 3 dot menu.
2. Click on "Settings" / settings icon (if menu redesign is enabled).
3. Scroll down and click on "About Firefox".
4. Click on the Firefox logo 5 times to enable debug menu.
5. Go back to settings.
6. Click on secret settings.
7. Toggle on "Enable Debug Drawer".
8. Click on the Bug FAB (Floating Action Button).
9. Click on "Glean Debug Tools".

***Method 2 (ADB with a computer)***

Prior to version 132, telemetry pings must be tested by plugging your Android device into a computer
and following these instructions:

Watch a step by step [video](https://user-images.githubusercontent.com/6579541/170517089-7266b93e-7ff8-4ebb-ae01-4f2a7e558c66.mp4).

1. To send data by default. apply this patch:
``` diff

diff --git a/app/src/main/java/org/mozilla/fenix/FenixApplication.kt b/app/src/main/java/org/mozilla/fenix/FenixApplication.kt

index 4cb11de43..0c6fab136 100644

--- a/app/src/main/java/org/mozilla/fenix/FenixApplication.kt

+++ b/app/src/main/java/org/mozilla/fenix/FenixApplication.kt

@@ -293,9 +293,7 @@ open class FenixApplication : LocaleAwareApplication(), Provider {

     }



     private fun startMetricsIfEnabled() {

-        if (settings().isTelemetryEnabled) {

-            components.analytics.metrics.start(MetricServiceType.Data)

-        }

+        components.analytics.metrics.start(MetricServiceType.Data)



         if (settings().isMarketingTelemetryEnabled) {

             components.analytics.metrics.start(MetricServiceType.Marketing)

diff --git a/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt b/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt

index c38ebb62d..3ae102d97 100644

--- a/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt

+++ b/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt

@@ -50,7 +50,7 @@ interface MetricController {

             isMarketingDataTelemetryEnabled: () -> Boolean,

             settings: Settings

         ): MetricController {

-            return if (BuildConfig.TELEMETRY) {

+            return if (true) {

                 ReleaseMetricController(

                     services,

                     isDataTelemetryEnabled,

```

2. Trigger your pings.
3. Sends the pings using this command:
```
adb shell am start -n org.mozilla.fenix.debug/mozilla.telemetry.glean.debug.GleanDebugActivity \
 --ez logPings true \
 --es sendPing metrics \
 --es debugViewTag test-metrics-ping
```
4. View the pings [here](https://debug-ping-preview.firebaseapp.com/).

The parameter `sendPing` can be  `metrics` or `events` depending on your needs; additionally, `debugViewTag` can be customized to your preferred tag `debugViewTag your-metrics-ping`.
