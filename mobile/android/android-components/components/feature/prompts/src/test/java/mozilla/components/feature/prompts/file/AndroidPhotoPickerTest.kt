/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContract
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class AndroidPhotoPickerTest {

    private lateinit var context: Context
    private lateinit var fragment: Fragment
    private lateinit var packageManager: PackageManager
    private lateinit var singleMediaPicker: ActivityResultLauncher<PickVisualMediaRequest>
    private lateinit var multipleMediaPicker: ActivityResultLauncher<PickVisualMediaRequest>
    private lateinit var promptFeature: PromptFeature
    private lateinit var androidPhotoPicker: AndroidPhotoPicker

    @Before
    fun setup() {
        context = mock()
        fragment = mock()
        singleMediaPicker = mock()
        multipleMediaPicker = mock()
        promptFeature = mock()

        packageManager = mock()
        whenever(
            packageManager.resolveActivity(
                any(Intent::class.java),
                anyInt(),
            ),
        ).thenReturn(null)

        whenever(
            fragment.registerForActivityResult(
                any<ActivityResultContracts.PickVisualMedia>(),
                any(),
            ),
        ).thenReturn(mock())

        whenever(context.packageManager).thenReturn(packageManager)
        androidPhotoPicker = AndroidPhotoPicker(context, singleMediaPicker, multipleMediaPicker)
    }

    @Test
    fun `isPhotoPickerAvailable returns true when photo picker is available`() {
        // on Android 10 and above the system framework provided photo picker should be available
        assertTrue(androidPhotoPicker.isPhotoPickerAvailable)
    }

    @Test
    @Config(sdk = [28])
    fun `isPhotoPickerAvailable returns false when photo picker is not available`() {
        assertFalse(androidPhotoPicker.isPhotoPickerAvailable)
    }

    @Test
    fun `singleMediaPicker uses a proper ActivityResultContract`() {
        AndroidPhotoPicker.singleMediaPicker({ fragment }, { promptFeature })

        val contractCaptor = argumentCaptor<ActivityResultContract<Intent, ActivityResult>>()
        val callbackCaptor = argumentCaptor<ActivityResultCallback<ActivityResult>>()

        verify(fragment).registerForActivityResult(
            contractCaptor.capture(),
            callbackCaptor.capture(),
        )

        assertTrue(contractCaptor.value is ActivityResultContracts.PickVisualMedia)
        assertNotNull(callbackCaptor.value)
    }

    @Test
    fun `multipleMediaPicker uses a proper ActivityResultContract`() {
        AndroidPhotoPicker.multipleMediaPicker({ fragment }, { promptFeature })

        val contractCaptor = argumentCaptor<ActivityResultContract<Intent, ActivityResult>>()
        val callbackCaptor = argumentCaptor<ActivityResultCallback<ActivityResult>>()

        verify(fragment).registerForActivityResult(
            contractCaptor.capture(),
            callbackCaptor.capture(),
        )

        assertTrue(contractCaptor.value is ActivityResultContracts.PickMultipleVisualMedia)
        assertNotNull(callbackCaptor.value)
    }

    @Test
    fun `singleMediaPicker returns a valid ActivityResultLauncher`() {
        val launcher = AndroidPhotoPicker.singleMediaPicker(
            { fragment },
            { promptFeature },
        )

        assertNotNull(launcher)
    }

    @Test
    fun `multipleMediaPicker returns a valid ActivityResultLauncher`() {
        val launcher = AndroidPhotoPicker.multipleMediaPicker(
            { fragment },
            { promptFeature },
        )

        assertNotNull(launcher)
    }
}
