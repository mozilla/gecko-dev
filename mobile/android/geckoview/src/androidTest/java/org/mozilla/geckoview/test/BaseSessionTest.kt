/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test

import android.os.Parcel
import android.support.test.InstrumentationRegistry
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule

import org.hamcrest.Matcher
import org.hamcrest.Matchers
import org.junit.Assume.assumeThat
import org.junit.Rule
import org.junit.rules.ErrorCollector

import kotlin.reflect.KClass

/**
 * Common base class for tests using GeckoSessionTestRule,
 * providing the test rule and other utilities.
 */
open class BaseSessionTest(noErrorCollector: Boolean = false) {
    companion object {
        const val CLICK_TO_RELOAD_HTML_PATH = "/assets/www/clickToReload.html"
        const val CONTENT_CRASH_URL = "about:crashcontent"
        const val DOWNLOAD_HTML_PATH = "/assets/www/download.html"
        const val FORMS_HTML_PATH = "/assets/www/forms.html"
        const val HELLO_HTML_PATH = "/assets/www/hello.html"
        const val HELLO2_HTML_PATH = "/assets/www/hello2.html"
        const val INPUTS_PATH = "/assets/www/inputs.html"
        const val INVALID_URI = "not a valid uri"
        const val LOREM_IPSUM_HTML_PATH = "/assets/www/loremIpsum.html"
        const val NEW_SESSION_CHILD_HTML_PATH = "/assets/www/newSession_child.html"
        const val NEW_SESSION_HTML_PATH = "/assets/www/newSession.html"
        const val POPUP_HTML_PATH = "/assets/www/popup.html"
        const val SAVE_STATE_PATH = "/assets/www/saveState.html"
        const val TITLE_CHANGE_HTML_PATH = "/assets/www/titleChange.html"
        const val TRACKERS_PATH = "/assets/www/trackers.html"
        const val UNKNOWN_HOST_URI = "http://www.test.invalid/"
        const val FULLSCREEN_PATH = "/assets/www/fullscreen.html"
    }

    @get:Rule val sessionRule = GeckoSessionTestRule()

    @get:Rule val errors = ErrorCollector()

    val mainSession get() = sessionRule.session

    fun <T> assertThat(reason: String, v: T, m: Matcher<in T>) = sessionRule.checkThat(reason, v, m)
    fun <T> assertInAutomationThat(reason: String, v: T, m: Matcher<in T>) =
            if (sessionRule.env.isAutomation) assertThat(reason, v, m)
            else assumeThat(reason, v, m)

    init {
        if (!noErrorCollector) {
            sessionRule.errorCollector = errors
        }
    }

    fun <T> forEachCall(vararg values: T): T = sessionRule.forEachCall(*values)

    fun getTestBytes(path: String) =
            InstrumentationRegistry.getTargetContext().resources.assets
                    .open(path.removePrefix("/assets/")).readBytes()

    val GeckoSession.isRemote
        get() = this.settings.getBoolean(GeckoSessionSettings.USE_MULTIPROCESS)

    fun createTestUrl(path: String) =
            GeckoSessionTestRule.APK_URI_PREFIX + path.removePrefix("/")

    fun GeckoSession.loadTestPath(path: String) =
            this.loadUri(createTestUrl(path))

    inline fun GeckoSession.toParcel(lambda: (Parcel) -> Unit) {
        val parcel = Parcel.obtain()
        try {
            this.writeToParcel(parcel, 0)

            val pos = parcel.dataPosition()
            parcel.setDataPosition(0)

            lambda(parcel)

            assertThat("Read parcel matches written parcel",
                       parcel.dataPosition(), Matchers.equalTo(pos))
        } finally {
            parcel.recycle()
        }
    }


    fun GeckoSession.open() =
            sessionRule.openSession(this)

    fun GeckoSession.waitForPageStop() =
            sessionRule.waitForPageStop(this)

    fun GeckoSession.waitForPageStops(count: Int) =
            sessionRule.waitForPageStops(this, count)

    fun GeckoSession.waitUntilCalled(ifce: KClass<*>, vararg methods: String) =
            sessionRule.waitUntilCalled(this, ifce, *methods)

    fun GeckoSession.waitUntilCalled(callback: Any) =
            sessionRule.waitUntilCalled(this, callback)

    fun GeckoSession.forCallbacksDuringWait(callback: Any) =
            sessionRule.forCallbacksDuringWait(this, callback)

    fun GeckoSession.delegateUntilTestEnd(callback: Any) =
            sessionRule.delegateUntilTestEnd(this, callback)

    fun GeckoSession.delegateDuringNextWait(callback: Any) =
            sessionRule.delegateDuringNextWait(this, callback)

    fun GeckoSession.synthesizeTap(x: Int, y: Int) =
            sessionRule.synthesizeTap(this, x, y)

    fun GeckoSession.evaluateJS(js: String): Any? =
            sessionRule.evaluateJS(this, js)

    fun GeckoSession.waitForJS(js: String): Any? =
            sessionRule.waitForJS(this, js)

    infix fun Any?.dot(prop: Any): Any? =
            if (prop is Int) this.asJSList<Any>()[prop] else this.asJSMap<Any>()[prop]

    @Suppress("UNCHECKED_CAST")
    fun <T> Any?.asJSMap(): Map<String, T> = this as Map<String, T>

    @Suppress("UNCHECKED_CAST")
    fun <T> Any?.asJSList(): List<T> = this as List<T>

    fun Any?.asJSPromise(): GeckoSessionTestRule.PromiseWrapper =
            this as GeckoSessionTestRule.PromiseWrapper
}
