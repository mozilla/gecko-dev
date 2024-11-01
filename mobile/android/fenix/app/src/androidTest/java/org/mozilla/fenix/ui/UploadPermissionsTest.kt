package org.mozilla.fenix.ui

import android.os.Build
import mozilla.components.support.ktx.util.PromptAbuserDetector
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertAppWithPackageNameOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.denyPermission
import org.mozilla.fenix.helpers.AppAndSystemHelper.grantSystemPermission
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.navigationToolbar

class UploadPermissionsTest : TestSetup() {

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule(
        isNavigationBarCFREnabled = false,
        isPWAsPromptEnabled = false,
    )

    override fun setUp() {
        super.setUp()
        PromptAbuserDetector.validationsEnabled = false
    }

    override fun tearDown() {
        super.tearDown()
        PromptAbuserDetector.validationsEnabled = true
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2121537
    @SmokeTest
    @Test
    fun fileUploadPermissionTest() {
        val testPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            clickPageObject(itemWithResId("upload_file"))
            // Grant app permission to access storage
            grantSystemPermission()
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                assertAppWithPackageNameOpens("com.google.android.documentsui")
            } else {
                assertAppWithPackageNameOpens("com.android.documentsui")
            }
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2751914
    @Test
    fun uploadSelectedAudioFilesWhileNoPermissionGrantedTest() {
        val testPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            clickPageObject(itemWithResId("audioFileUpload"))
            // Deny app access to voice recording
            denyPermission()
            // Deny app access to audio files storage
            denyPermission()
            verifyPageContent("Choose audio file to upload")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2779525
    @Test
    fun uploadSelectedAudioFilesWhenStoragePermissionGrantedTest() {
        val testPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            clickPageObject(itemWithResId("audioFileUpload"))
            // Deny app access to voice recording
            denyPermission()
            // Grant app access to audio files storage
            grantSystemPermission()
            assertAppWithPackageNameOpens("com.google.android.documentsui")
        }
    }
}
