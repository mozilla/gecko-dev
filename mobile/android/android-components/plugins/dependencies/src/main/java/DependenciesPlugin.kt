/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import org.gradle.api.Plugin
import org.gradle.api.initialization.Settings

// If you ever need to force a toolchain rebuild (taskcluster) then edit the following comment.
// FORCE REBUILD 2024-05-02

class DependenciesPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) = Unit
}

// Synchronized version numbers for dependencies used by (some) modules
object Versions {
    const val kotlin = "1.9.24"
    const val coroutines = "1.8.1"
    const val serialization = "1.6.3"
    const val python_envs_plugin = "0.0.31"

    const val mozilla_glean = "60.5.0"

    const val junit = "4.13.2"
    const val robolectric = "4.13"
    const val mockito = "5.12.0"
    const val maven_ant_tasks = "2.1.3"
    const val jacoco = "0.8.11"
    const val okhttp = "4.12.0"
    const val okio = "3.9.0"
    const val androidsvg = "1.4"

    const val android_gradle_plugin = "8.4.2"

    // This has to be synced to the gradlew plugin version. See
    // http://googlesamples.github.io/android-custom-lint-rules/api-guide/example.md.html#example:samplelintcheckgithubproject/lintversion?
    const val lint = "31.4.2"
    const val detekt = "1.23.6"
    const val ktlint = "0.49.1"

    const val sentry = "7.14.0"

    const val zxing = "3.5.3"

    const val disklrucache = "2.0.2"
    const val leakcanary = "2.14"

    const val ksp = "1.0.20"
    val ksp_plugin = "$kotlin-$ksp"

    // see https://android-developers.googleblog.com/2022/06/independent-versioning-of-Jetpack-Compose-libraries.html
    // for Jetpack Compose libraries versioning
    const val compose_compiler = "1.5.14"

    object AndroidX {
        const val activity = "1.9.1"
        const val annotation = "1.8.2"
        const val appcompat = "1.7.0"
        const val autofill = "1.1.0"
        const val benchmark = "1.3.0"
        const val browser = "1.8.0"
        const val biometric = "1.1.0"
        const val cardview = "1.0.0"
        const val collection = "1.4.3"
        const val compose_bom = "2024.06.00"
        const val constraintlayout = "2.1.4"
        const val coordinatorlayout = "1.2.0"
        const val core = "1.13.1"
        const val drawerlayout = "1.2.0"
        const val fragment = "1.8.2"
        const val recyclerview = "1.3.2"
        const val room = "2.6.1"
        const val savedstate = "1.2.1"
        const val paging = "3.3.2"
        const val palette = "1.0.0"
        const val preferences = "1.2.1"
        const val lifecycle = "2.8.4"
        const val media = "1.7.0"
        const val navigation = "2.7.7"
        const val transition = "1.5.1"
        const val tracing = "1.2.0"
        const val work = "2.9.1"
        const val arch = "2.2.0"
        const val localbroadcastmanager = "1.0.0"
        const val swiperefreshlayout = "1.1.0"
        const val datastore="1.1.1"
        const val test = "1.6.1"
        const val test_ext = "1.2.1"
        const val test_espresso = "3.6.1"
        const val test_orchestrator = "1.5.0"
        const val test_runner = "1.6.1"
        const val test_uiautomator = "2.3.0"
    }

    object Firebase {
        const val messaging = "24.0.0"
    }

    object Google {
        const val material = "1.12.0"
        const val osslicenses_plugin = "0.10.4"
        const val play_review = "2.0.1"
        const val play_services_ads_id = "16.0.0"
        const val play_services_base = "18.5.0"
        const val play_services_fido = "21.1.0"
    }
}

// Synchronized dependencies used by (some) modules
@Suppress("Unused", "MaxLineLength")
object ComponentsDependencies {
    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${Versions.coroutines}"
    const val kotlin_reflect = "org.jetbrains.kotlin:kotlin-reflect:${Versions.kotlin}"
    const val kotlin_json = "org.jetbrains.kotlinx:kotlinx-serialization-json:${Versions.serialization}"

    const val testing_junit = "junit:junit:${Versions.junit}"
    const val testing_robolectric = "org.robolectric:robolectric:${Versions.robolectric}"
    const val testing_mockito = "org.mockito:mockito-core:${Versions.mockito}"
    const val testing_mockwebserver = "com.squareup.okhttp3:mockwebserver:${Versions.okhttp}"
    const val testing_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-test:${Versions.coroutines}"
    const val testing_maven_ant_tasks = "org.apache.maven:maven-ant-tasks:${Versions.maven_ant_tasks}"
    const val testing_leakcanary = "com.squareup.leakcanary:leakcanary-android-instrumentation:${Versions.leakcanary}"

    const val androidx_activity = "androidx.activity:activity:${Versions.AndroidX.activity}"
    const val androidx_activity_ktx = "androidx.activity:activity-ktx:${Versions.AndroidX.activity}"
    const val androidx_annotation = "androidx.annotation:annotation:${Versions.AndroidX.annotation}"
    const val androidx_appcompat = "androidx.appcompat:appcompat:${Versions.AndroidX.appcompat}"
    const val androidx_autofill = "androidx.autofill:autofill:${Versions.AndroidX.autofill}"
    const val androidx_arch_core_common = "androidx.arch.core:core-common:${Versions.AndroidX.arch}"
    const val androidx_arch_core_testing = "androidx.arch.core:core-testing:${Versions.AndroidX.arch}"
    const val androidx_benchmark_junit4 = "androidx.benchmark:benchmark-junit4:${Versions.AndroidX.benchmark}"
    const val androidx_benchmark_macro_junit4 = "androidx.benchmark:benchmark-macro-junit4:${Versions.AndroidX.benchmark}"
    const val androidx_biometric = "androidx.biometric:biometric:${Versions.AndroidX.biometric}"
    const val androidx_browser = "androidx.browser:browser:${Versions.AndroidX.browser}"
    const val androidx_cardview = "androidx.cardview:cardview:${Versions.AndroidX.cardview}"
    const val androidx_collection = "androidx.collection:collection:${Versions.AndroidX.collection}"

    const val androidx_compose_bom = "androidx.compose:compose-bom:${Versions.AndroidX.compose_bom}"
    const val androidx_compose_animation = "androidx.compose.animation:animation"
    const val androidx_compose_ui = "androidx.compose.ui:ui"
    const val androidx_compose_ui_graphics = "androidx.compose.ui:ui-graphics"
    const val androidx_compose_ui_test = "androidx.compose.ui:ui-test-junit4"
    const val androidx_compose_ui_test_manifest = "androidx.compose.ui:ui-test-manifest"
    const val androidx_compose_ui_tooling = "androidx.compose.ui:ui-tooling"
    const val androidx_compose_ui_tooling_preview = "androidx.compose.ui:ui-tooling-preview"
    const val androidx_compose_foundation = "androidx.compose.foundation:foundation"
    const val androidx_compose_material = "androidx.compose.material:material"
    const val androidx_compose_runtime_livedata = "androidx.compose.runtime:runtime-livedata"
    const val androidx_compose_runtime_saveable = "androidx.compose.runtime:runtime-saveable"

    const val androidx_safeargs = "androidx.navigation:navigation-safe-args-gradle-plugin:${Versions.AndroidX.navigation}"
    const val androidx_navigation_fragment = "androidx.navigation:navigation-fragment-ktx:${Versions.AndroidX.navigation}"
    const val androidx_navigation_ui = "androidx.navigation:navigation-ui:${Versions.AndroidX.navigation}"
    const val androidx_compose_navigation = "androidx.navigation:navigation-compose:${Versions.AndroidX.navigation}"
    const val androidx_constraintlayout = "androidx.constraintlayout:constraintlayout:${Versions.AndroidX.constraintlayout}"
    const val androidx_core = "androidx.core:core:${Versions.AndroidX.core}"
    const val androidx_core_ktx = "androidx.core:core-ktx:${Versions.AndroidX.core}"
    const val androidx_coordinatorlayout = "androidx.coordinatorlayout:coordinatorlayout:${Versions.AndroidX.coordinatorlayout}"
    const val androidx_drawerlayout = "androidx.drawerlayout:drawerlayout:${Versions.AndroidX.drawerlayout}"
    const val androidx_fragment = "androidx.fragment:fragment:${Versions.AndroidX.fragment}"
    const val androidx_lifecycle_common = "androidx.lifecycle:lifecycle-common:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_compose = "androidx.lifecycle:lifecycle-runtime-compose:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_livedata = "androidx.lifecycle:lifecycle-livedata-ktx:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_process = "androidx.lifecycle:lifecycle-process:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_runtime = "androidx.lifecycle:lifecycle-runtime-ktx:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_service = "androidx.lifecycle:lifecycle-service:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_viewmodel = "androidx.lifecycle:lifecycle-viewmodel-ktx:${Versions.AndroidX.lifecycle}"
    const val androidx_media = "androidx.media:media:${Versions.AndroidX.media}"
    const val androidx_paging = "androidx.paging:paging-runtime:${Versions.AndroidX.paging}"
    const val androidx_palette = "androidx.palette:palette-ktx:${Versions.AndroidX.palette}"
    const val androidx_preferences = "androidx.preference:preference:${Versions.AndroidX.preferences}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${Versions.AndroidX.recyclerview}"
    const val androidx_room_runtime = "androidx.room:room-ktx:${Versions.AndroidX.room}"
    const val androidx_room_compiler = "androidx.room:room-compiler:${Versions.AndroidX.room}"
    const val androidx_room_testing = "androidx.room:room-testing:${Versions.AndroidX.room}"
    const val androidx_savedstate = "androidx.savedstate:savedstate:${Versions.AndroidX.savedstate}"
    const val androidx_tracing = "androidx.tracing:tracing:${Versions.AndroidX.tracing}"
    const val androidx_transition = "androidx.transition:transition:${Versions.AndroidX.transition}"
    const val androidx_work_runtime = "androidx.work:work-runtime:${Versions.AndroidX.work}"
    const val androidx_work_testing = "androidx.work:work-testing:${Versions.AndroidX.work}"
    const val androidx_localbroadcastmanager = "androidx.localbroadcastmanager:localbroadcastmanager:${Versions.AndroidX.localbroadcastmanager}"
    const val androidx_swiperefreshlayout = "androidx.swiperefreshlayout:swiperefreshlayout:${Versions.AndroidX.swiperefreshlayout}"
    const val androidx_datastore = "androidx.datastore:datastore:${Versions.AndroidX.datastore}"
    const val androidx_datastore_preferences = "androidx.datastore:datastore-preferences:${Versions.AndroidX.datastore}"

    const val androidx_espresso_contrib = "androidx.test.espresso:espresso-contrib:${Versions.AndroidX.test_espresso}"
    const val androidx_espresso_core = "androidx.test.espresso:espresso-core:${Versions.AndroidX.test_espresso}"
    const val androidx_espresso_idling_resource = "androidx.test.espresso:espresso-idling-resource:${Versions.AndroidX.test_espresso}"
    const val androidx_espresso_intents = "androidx.test.espresso:espresso-intents:${Versions.AndroidX.test_espresso}"
    const val androidx_espresso_web = "androidx.test.espresso:espresso-web:${Versions.AndroidX.test_espresso}"
    const val androidx_test_core = "androidx.test:core-ktx:${Versions.AndroidX.test}"
    const val androidx_test_junit = "androidx.test.ext:junit-ktx:${Versions.AndroidX.test_ext}"
    const val androidx_test_orchestrator = "androidx.test:orchestrator:${Versions.AndroidX.test_orchestrator}"
    const val androidx_test_runner = "androidx.test:runner:${Versions.AndroidX.test_runner}"
    const val androidx_test_rules = "androidx.test:rules:${Versions.AndroidX.test}"
    const val androidx_test_uiautomator = "androidx.test.uiautomator:uiautomator:${Versions.AndroidX.test_uiautomator}"

    const val google_material = "com.google.android.material:material:${Versions.Google.material}"

    const val plugin_serialization = "org.jetbrains.kotlin.plugin.serialization:org.jetbrains.kotlin.plugin.serialization.gradle.plugin:${Versions.kotlin}"

    const val leakcanary = "com.squareup.leakcanary:leakcanary-android:${Versions.leakcanary}"

    const val tools_androidgradle = "com.android.tools.build:gradle:${Versions.android_gradle_plugin}"
    const val tools_benchmarkgradle = "androidx.benchmark:benchmark-gradle-plugin:${Versions.AndroidX.benchmark}"
    const val tools_kotlingradle = "org.jetbrains.kotlin:kotlin-gradle-plugin:${Versions.kotlin}"

    const val tools_lint = "com.android.tools.lint:lint:${Versions.lint}"
    const val tools_lintapi = "com.android.tools.lint:lint-api:${Versions.lint}"
    const val tools_lintchecks = "com.android.tools.lint:lint-checks:${Versions.lint}"
    const val tools_linttests = "com.android.tools.lint:lint-tests:${Versions.lint}"

    const val tools_detekt_api = "io.gitlab.arturbosch.detekt:detekt-api:${Versions.detekt}"
    const val tools_detekt_test = "io.gitlab.arturbosch.detekt:detekt-test:${Versions.detekt}"

    val mozilla_appservices_fxaclient = "${ApplicationServicesConfig.groupId}:fxaclient:${ApplicationServicesConfig.version}"
    val mozilla_appservices_nimbus = "${ApplicationServicesConfig.groupId}:nimbus:${ApplicationServicesConfig.version}"
    val mozilla_appservices_autofill = "${ApplicationServicesConfig.groupId}:autofill:${ApplicationServicesConfig.version}"
    val mozilla_appservices_logins = "${ApplicationServicesConfig.groupId}:logins:${ApplicationServicesConfig.version}"
    val mozilla_appservices_places = "${ApplicationServicesConfig.groupId}:places:${ApplicationServicesConfig.version}"
    val mozilla_appservices_syncmanager = "${ApplicationServicesConfig.groupId}:syncmanager:${ApplicationServicesConfig.version}"
    val mozilla_remote_settings = "${ApplicationServicesConfig.groupId}:remotesettings:${ApplicationServicesConfig.version}"
    val mozilla_appservices_push = "${ApplicationServicesConfig.groupId}:push:${ApplicationServicesConfig.version}"
    val mozilla_appservices_tabs = "${ApplicationServicesConfig.groupId}:tabs:${ApplicationServicesConfig.version}"
    val mozilla_appservices_suggest = "${ApplicationServicesConfig.groupId}:suggest:${ApplicationServicesConfig.version}"
    val mozilla_appservices_httpconfig = "${ApplicationServicesConfig.groupId}:httpconfig:${ApplicationServicesConfig.version}"
    val mozilla_appservices_full_megazord = "${ApplicationServicesConfig.groupId}:full-megazord:${ApplicationServicesConfig.version}"
    val mozilla_appservices_full_megazord_forUnitTests = "${ApplicationServicesConfig.groupId}:full-megazord-forUnitTests:${ApplicationServicesConfig.version}"

    val mozilla_appservices_errorsupport = "${ApplicationServicesConfig.groupId}:errorsupport:${ApplicationServicesConfig.version}"
    val mozilla_appservices_rust_log_forwarder = "${ApplicationServicesConfig.groupId}:rust-log-forwarder:${ApplicationServicesConfig.version}"
    val mozilla_appservices_sync15 = "${ApplicationServicesConfig.groupId}:sync15:${ApplicationServicesConfig.version}"

    const val mozilla_glean = "org.mozilla.telemetry:glean:${Versions.mozilla_glean}"
    const val mozilla_glean_forUnitTests = "org.mozilla.telemetry:glean-native-forUnitTests:${Versions.mozilla_glean}"

    const val thirdparty_okhttp = "com.squareup.okhttp3:okhttp:${Versions.okhttp}"
    const val thirdparty_okhttp_urlconnection = "com.squareup.okhttp3:okhttp-urlconnection:${Versions.okhttp}"
    const val thirdparty_okio = "com.squareup.okio:okio:${Versions.okio}"
    const val thirdparty_sentry = "io.sentry:sentry-android:${Versions.sentry}"
    const val thirdparty_zxing = "com.google.zxing:core:${Versions.zxing}"
    const val thirdparty_disklrucache = "com.jakewharton:disklrucache:${Versions.disklrucache}"
    const val thirdparty_androidsvg = "com.caverock:androidsvg-aar:${Versions.androidsvg}"

    const val firebase_messaging = "com.google.firebase:firebase-messaging:${Versions.Firebase.messaging}"

    const val osslicenses_plugin = "com.google.android.gms:oss-licenses-plugin:${Versions.Google.osslicenses_plugin}"
    const val play_review = "com.google.android.play:review:${Versions.Google.play_review}"
    const val play_review_ktx = "com.google.android.play:review-ktx:${Versions.Google.play_review}"
    const val play_services_ads_id = "com.google.android.gms:play-services-ads-identifier:${Versions.Google.play_services_ads_id}"
    const val play_services_base = "com.google.android.gms:play-services-base:${Versions.Google.play_services_base}"
    const val play_services_fido = "com.google.android.gms:play-services-fido:${Versions.Google.play_services_fido}"
}
