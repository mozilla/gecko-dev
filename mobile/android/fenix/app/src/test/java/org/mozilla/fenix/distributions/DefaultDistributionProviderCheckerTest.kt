/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import android.content.ComponentName
import android.content.ContentProvider
import android.content.ContentValues
import android.content.IntentFilter
import android.content.pm.ApplicationInfo
import android.content.pm.ProviderInfo
import android.database.Cursor
import android.database.MatrixCursor
import android.net.Uri
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf

@RunWith(FenixRobolectricTestRunner::class)
class DefaultDistributionProviderCheckerTest {
    private val subject = DefaultDistributionProviderChecker(testContext)

    @Test
    fun `WHEN a content provider exists THEN the provider name is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "some.package",
            providerName = "myProvider",
        )

        val provider = subject.queryProvider()

        assertEquals("myProvider", provider)
    }

    @Test
    fun `WHEN a content provider does not exists THEN null is returned`() {
        val provider = subject.queryProvider()

        assertEquals(null, provider)
    }

    @Test
    fun `WHEN a content provider exists but does not have the encrypted_data column THEN null is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "some.package",
            columns = listOf(
                Pair("package_name", "org.mozilla.firefox"),
            ),
        )

        val provider = subject.queryProvider()

        assertEquals(null, provider)
    }

    @Test
    fun `WHEN a content provider exists but does not have the package_name column THEN null is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "some.package",
            columns = listOf(
                Pair("encrypted_data", "{\"provider\": \"provider\"}"),
            ),
        )

        val provider = subject.queryProvider()

        assertEquals(null, provider)
    }

    @Test
    fun `WHEN the encrypted_data column is not json THEN null is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "some.package",
            columns = listOf(
                Pair("package_name", "org.mozilla.firefox"),
                Pair("encrypted_data", "not json"),
            ),
        )

        val provider = subject.queryProvider()

        assertEquals(null, provider)
    }

    @Test
    fun `WHEN the encrypted_data column does not have a provider string THEN null is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "some.package",
            columns = listOf(
                Pair("package_name", "org.mozilla.firefox"),
                Pair("encrypted_data", "{\"test\": \"test\"}"),
            ),
        )

        val provider = subject.queryProvider()

        assertEquals(null, provider)
    }

    @Suppress("SameParameterValue")
    private fun createFakeContentProviderForAdjust(
        packageName: String,
        providerName: String = "provider",
        columns: List<Pair<String, String>> = listOf(
            Pair("package_name", "org.mozilla.firefox"),
            Pair("encrypted_data", "{\"provider\": \"$providerName\"}"),
        ),
    ) {
        val shadowPackageManager = shadowOf(testContext.packageManager)

        // Register a fake app with a fake content provider
        val providerInfo = ProviderInfo().apply {
            authority = packageName
            name = TestContentProvider::class.qualifiedName
            this.packageName = packageName
            applicationInfo = ApplicationInfo().apply {
                this.packageName = packageName
                flags = ApplicationInfo.FLAG_INSTALLED
            }
        }
        shadowPackageManager.addOrUpdateProvider(providerInfo)

        // Insert test data into a fake content provider
        val contentProvider = Robolectric.buildContentProvider(TestContentProvider::class.java)
            .create(providerInfo)
            .get()
        val uri = Uri.parse("content://$packageName/trackers")
        val values = ContentValues().apply {
            columns.forEach {
                put(it.first, it.second)
            }
        }
        contentProvider.insert(uri, values)

        // Make the content provider discoverable via an intent action
        shadowPackageManager.addIntentFilterForProvider(
            ComponentName(providerInfo.packageName, providerInfo.name),
            IntentFilter("com.attribution.REFERRAL_PROVIDER"),
        )
    }

    class TestContentProvider : ContentProvider() {
        private val database = mutableListOf<ContentValues>()

        override fun onCreate(): Boolean = true

        override fun insert(uri: Uri, values: ContentValues?): Uri {
            values?.let { database.add(it) }
            return uri
        }

        override fun query(
            uri: Uri,
            projection: Array<String>?,
            selection: String?,
            selectionArgs: Array<String>?,
            sortOrder: String?,
        ): Cursor {
            val cursor = MatrixCursor(projection ?: arrayOf())

            for (values in database) {
                val row = projection?.map { values.getAsString(it) }?.toTypedArray() ?: emptyArray()
                cursor.addRow(row)
            }
            return cursor
        }

        override fun update(
            uri: Uri,
            values: ContentValues?,
            selection: String?,
            selectionArgs: Array<String>?,
        ): Int = 1

        override fun delete(uri: Uri, selection: String?, selectionArgs: Array<String>?): Int = 1

        override fun getType(uri: Uri): String = ""
    }
}
