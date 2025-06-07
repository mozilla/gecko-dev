/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.util.Log
import androidx.browser.customtabs.CustomTabsIntent
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.core.graphics.createBitmap
import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiSelector
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.availableSearchEngines
import org.junit.Assert
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.Constants.recommendedAddons
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.waitForAppWindowToBeUpdated
import org.mozilla.fenix.utils.IntentUtils
import java.time.LocalDate
import java.time.LocalTime

object DataGenerationHelper {
    val appContext: Context = InstrumentationRegistry.getInstrumentation().targetContext

    fun createCustomTabIntent(
        pageUrl: String,
        customMenuItemLabel: String = "",
        customActionButtonDescription: String = "",
    ): Intent {
        Log.i(TAG, "createCustomTabIntent: Trying to create custom tab intent with url: $pageUrl")
        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        val pendingIntent = PendingIntent.getActivity(appContext, 0, Intent(), IntentUtils.defaultIntentPendingFlags)
        val customTabsIntent = CustomTabsIntent.Builder()
            .addMenuItem(customMenuItemLabel, pendingIntent)
            .setShareState(CustomTabsIntent.SHARE_STATE_ON)
            .setActionButton(
                createTestBitmap(),
                customActionButtonDescription,
                pendingIntent,
                true,
            )
            .build()
        customTabsIntent.intent.data = pageUrl.toUri()
        Log.i(TAG, "createCustomTabIntent: Created custom tab intent with url: $pageUrl")
        return customTabsIntent.intent
    }

    private fun createTestBitmap(): Bitmap {
        Log.i(TAG, "createTestBitmap: Trying to create a test bitmap")
        val bitmap = createBitmap(100, 100, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.GREEN)
        Log.i(TAG, "createTestBitmap: Created a test bitmap")
        return bitmap
    }

    fun getStringResource(id: Int, argument: String = TestHelper.appName) = TestHelper.appContext.resources.getString(id, argument)

    private val charPool: List<Char> = ('a'..'z') + ('A'..'Z') + ('0'..'9')
    fun generateRandomString(stringLength: Int): String {
        Log.i(TAG, "generateRandomString: Trying to generate a random string with $stringLength characters")
        val randomString =
            (1..stringLength)
                .map { kotlin.random.Random.nextInt(0, charPool.size) }
                .map(charPool::get)
                .joinToString("")
        Log.i(TAG, "generateRandomString: Generated random string: $randomString")

        return randomString
    }

    /**
     * Creates clipboard data.
     */
    fun setTextToClipBoard(context: Context, message: String) {
        Log.i(TAG, "setTextToClipBoard: Trying to set clipboard text to: $message")
        val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clipData = ClipData.newPlainText("label", message)

        clipBoard.setPrimaryClip(clipData)
        Log.i(TAG, "setTextToClipBoard: Clipboard text was set to: $message")
    }

    /**
     * Constructs a date and time placeholder string for sponsored Fx suggest links.
     * The format of the datetime is YYYYMMDDHH, where YYYY is the four-digit year,
     * MM is the two-digit month, DD is the two-digit day, and HH is the two-digit hour.
     * Single-digit months, days, and hours are padded with a leading zero to ensure
     * the correct format. For example, a date and time of January 10, 2024, at 3 PM
     * would be represented as "2024011015".
     *
     * @return A string representing the current date and time in the specified format.
     */
    fun getSponsoredFxSuggestPlaceHolder(): String {
        Log.i(TAG, "getSponsoredFxSuggestPlaceHolder: Trying to get the sponsored search suggestion placeholder")
        val currentDate = LocalDate.now()
        val currentTime = LocalTime.now()

        val currentDay = currentDate.dayOfMonth.toString().padStart(2, '0')
        val currentMonth = currentDate.monthValue.toString().padStart(2, '0')
        val currentYear = currentDate.year.toString()
        val currentHour = currentTime.hour.toString().padStart(2, '0')

        Log.i(TAG, "getSponsoredFxSuggestPlaceHolder: Got: ${currentYear + currentMonth + currentDay + currentHour} as the sponsored search suggestion placeholder")

        return currentYear + currentMonth + currentDay + currentHour
    }

    /**
     * Returns sponsored shortcut title based on the index.
     */
    fun getSponsoredShortcutTitle(position: Int): String {
        Log.i(TAG, "getSponsoredShortcutTitle: Trying to get the title of the sponsored shortcut at position: ${position - 1}")
        val sponsoredShortcut = mDevice.findObject(
            UiSelector()
                .resourceId("top_sites_list.top_site_item")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .resourceId("top_sites_list.top_site_item.top_site_title"),
        ).text
        Log.i(TAG, "getSponsoredShortcutTitle: The sponsored shortcut at position: ${position - 1} has title: $sponsoredShortcut")
        return sponsoredShortcut
    }

    /**
     * Returns the title of the first matching extension.
     */
    fun getRecommendedExtensionTitle(composeTestRule: ComposeTestRule): String {
        var verifiedCount = 0

        recommendedAddons.forEach { addon ->
            try {
                waitForAppWindowToBeUpdated()
                Log.i(TAG, "getRecommendedExtensionTitle: Trying to verify that addon: $addon is recommended and displayed")
                composeTestRule.onNodeWithContentDescription("Add $addon", substring = true).assertIsDisplayed()
                Log.i(TAG, "getRecommendedExtensionTitle: Verified that addon: $addon is recommended and displayed")

                verifiedCount++
            } catch (e: AssertionError) {
                Log.i(TAG, "getRecommendedExtensionTitle: Addon: $addon is not displayed, moving to the next one")
            }
            if (verifiedCount == 1) return addon
        }
        return "$TAG: No add-on found"
    }

    /**
     * The list of Search engines for the "home" region of the user.
     * For en-us it will return the 6 engines selected by default: Google, Bing, DuckDuckGo, Amazon, Ebay, Wikipedia.
     */
    fun getRegionSearchEnginesList(): List<SearchEngine> {
        Log.i(TAG, "getRegionSearchEnginesList: Trying to get the search engines based on the region of the user")
        val searchEnginesList = appContext.components.core.store.state.search.regionSearchEngines
        Assert.assertTrue("$TAG: Search engines list returned nothing", searchEnginesList.isNotEmpty())
        Log.i(TAG, "getRegionSearchEnginesList: Got $searchEnginesList based on the region of the user")
        return searchEnginesList
    }

    /**
     * The list of Search engines available to be added by user choice.
     * For en-us it will return the 2 engines: Reddit, Youtube.
     */
    fun getAvailableSearchEngines(): List<SearchEngine> {
        Log.i(TAG, "getAvailableSearchEngines: Trying to get the alternative search engines based on the region of the user")
        val searchEnginesList = TestHelper.appContext.components.core.store.state.search.availableSearchEngines
        Log.i(TAG, "getAvailableSearchEngines: Got $searchEnginesList based on the region of the user")
        return searchEnginesList
    }
}
