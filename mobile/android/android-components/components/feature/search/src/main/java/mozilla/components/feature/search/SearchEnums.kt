/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.search

/**
 * Enum class representing the remote-services SearchUpdateChannel.
 */
enum class SearchUpdateChannel {
    NIGHTLY,
    AURORA,
    BETA,
    RELEASE,
    ESR,
    DEFAULT,
}

/**
 * Convert between the android-components defined [SearchUpdateChannel] type into
 * the remote-services one, so consumers of android-components
 * do not have to know about application services.
 */
fun SearchUpdateChannel.into(): mozilla.appservices.search.SearchUpdateChannel {
    return when (this) {
        SearchUpdateChannel.RELEASE -> mozilla.appservices.search.SearchUpdateChannel.RELEASE
        SearchUpdateChannel.NIGHTLY -> mozilla.appservices.search.SearchUpdateChannel.NIGHTLY
        SearchUpdateChannel.ESR -> mozilla.appservices.search.SearchUpdateChannel.ESR
        SearchUpdateChannel.BETA -> mozilla.appservices.search.SearchUpdateChannel.BETA
        SearchUpdateChannel.AURORA -> mozilla.appservices.search.SearchUpdateChannel.AURORA
        SearchUpdateChannel.DEFAULT -> mozilla.appservices.search.SearchUpdateChannel.DEFAULT
    }
}

/**
 * Enum class representing the remote-services SearchDeviceType.
 */
enum class SearchDeviceType {
    SMARTPHONE,
    TABLET,
    NONE,
}

/**
 * Convert between the android-components defined [SearchDeviceType] type into
 * the remote-services one, so consumers of android-components
 * do not have to know about application services.
 */
fun SearchDeviceType.into(): mozilla.appservices.search.SearchDeviceType {
    return when (this) {
        SearchDeviceType.TABLET -> mozilla.appservices.search.SearchDeviceType.TABLET
        SearchDeviceType.SMARTPHONE -> mozilla.appservices.search.SearchDeviceType.SMARTPHONE
        SearchDeviceType.NONE -> mozilla.appservices.search.SearchDeviceType.NONE
    }
}

/**
 * Enum class representing the remote-services SearchApplicationName.
 */
enum class SearchApplicationName {
    FIREFOX_ANDROID,
    FOCUS_ANDROID,
    FIREFOX,
}

/**
 * Convert between the android-components defined [SearchApplicationName] type into
 * the remote-services one, so consumers of android-components
 * do not have to know about application services.
 */
fun SearchApplicationName.into(): mozilla.appservices.search.SearchApplicationName {
    return when (this) {
        SearchApplicationName.FIREFOX_ANDROID -> mozilla.appservices.search.SearchApplicationName.FIREFOX_ANDROID
        SearchApplicationName.FOCUS_ANDROID -> mozilla.appservices.search.SearchApplicationName.FOCUS_ANDROID
        SearchApplicationName.FIREFOX -> mozilla.appservices.search.SearchApplicationName.FIREFOX
    }
}
