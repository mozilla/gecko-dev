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
import io.mockk.clearMocks
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.unmockkObject
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.Config
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf
import org.robolectric.shadows.ShadowBuild

@RunWith(FenixRobolectricTestRunner::class)
class DistributionIdUtilTest {

    private val browserStoreMock: BrowserStore = mockk(relaxed = true)
    private val browserStateMock: BrowserState = mockk(relaxed = true)

    @Before
    fun setup() {
        every { browserStoreMock.state } returns browserStateMock
        every { browserStateMock.distributionId } returns null
    }

    @After
    fun tearDown() {
        clearMocks(browserStoreMock, browserStateMock)
        unmockkObject(Config)
        ShadowBuild.reset()
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = getDistributionId(testContext, browserStoreMock) { true }

        assertEquals("vivo-001", distributionId)
    }

    @Test
    fun `WHEN a device is not made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        val distributionId = getDistributionId(testContext, browserStoreMock) { true }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is not found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = getDistributionId(testContext, browserStoreMock) { false }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the config channel is mozilla online THEN the proper id is returned`() {
        mockkObject(Config)
        every { Config.channel.isMozillaOnline } returns true

        val distributionId = getDistributionId(testContext, browserStoreMock)

        assertEquals("MozillaOnline", distributionId)
    }

    @Test
    fun `WHEN the device is not vivo AND the channel is not mozilla online THEN the proper id is returned`() {
        val distributionId = getDistributionId(testContext, browserStoreMock)

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the browser stores state already has a distribution Id assigned THEN that ID gets returned`() {
        every { browserStateMock.distributionId } returns "testId"

        val distributionId = getDistributionId(testContext, browserStoreMock)

        assertEquals("testId", distributionId)
    }

    @Test
    fun `WHEN there is a content provider for digital turbine with the correct provider name THEN the proper ID is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "com.dti.telefonica",
            providerName = "digital_turbine",
        )

        // test
        val distributionId = getDistributionId(testContext, browserStoreMock) { true }

        assertEquals("dt-001", distributionId)
    }

    @Test
    fun `WHEN there is a content provider but the provider name is not digital_tubrine THEN the proper ID is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "com.dti.telefonica",
            providerName = "bad_name",
        )

        // test
        val distributionId = getDistributionId(testContext, browserStoreMock) { true }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the encrypted_data column does not exist THEN the proper ID is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "com.dti.telefonica",
            columns = listOf(
                Pair("package_name", "org.mozilla.firefox"),
            ),
        )

        // test
        val distributionId = getDistributionId(testContext, browserStoreMock) { true }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN there is malformed json THEN the proper ID is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "com.dti.telefonica",
            columns = listOf(
                Pair("package_name", "org.mozilla.firefox"),
                Pair("encrypted_data", "not json"),
            ),
        )

        // test
        val distributionId = getDistributionId(testContext, browserStoreMock) { true }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN a content provider exists THEN the provider name is returned`() {
        createFakeContentProviderForAdjust(
            packageName = "some.package",
            providerName = "myProvider",
        )

        val provider = queryProvider(testContext)

        assertEquals("myProvider", provider)
    }

    @Test
    fun `WHEN a content provider does not exists THEN null is returned`() {
        val provider = queryProvider(testContext)

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

        val provider = queryProvider(testContext)

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

        val provider = queryProvider(testContext)

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

        val provider = queryProvider(testContext)

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

        val provider = queryProvider(testContext)

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
