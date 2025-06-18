/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons.ui

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsNotDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import mozilla.components.feature.addons.Addon
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.addons.AddonPermissionsUpdateRequest
import org.mozilla.fenix.theme.FirefoxTheme

class AddonPermissionsScreenTest {
    @get:Rule
    val composeTestRule = createComposeRule()

    @Test
    fun testNoPermissions() {
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = emptyList(),
                    optionalPermissions = emptyList(),
                    originPermissions = emptyList(),
                    requiredDataCollectionPermissions = emptyList(),
                    optionalDataCollectionPermissions = emptyList(),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {},
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsNotDisplayed()

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsDisplayed()
    }

    @Test
    fun testRequiredPermissions() {
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = listOf("Access browser tabs"),
                    optionalPermissions = emptyList(),
                    originPermissions = emptyList(),
                    requiredDataCollectionPermissions = emptyList(),
                    optionalDataCollectionPermissions = emptyList(),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {},
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Access browser tabs").assertIsDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsNotDisplayed()

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsNotDisplayed()
    }

    @Test
    fun testOptionalPermissions() {
        var request: AddonPermissionsUpdateRequest? = null
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = emptyList(),
                    optionalPermissions = listOf(
                        Addon.LocalizedPermission(
                            "Read and modify bookmarks",
                            Addon.Permission("bookmarks", false),
                        ),
                    ),
                    originPermissions = emptyList(),
                    requiredDataCollectionPermissions = emptyList(),
                    optionalDataCollectionPermissions = emptyList(),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {
                        request = it
                    },
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Read and modify bookmarks").assertIsDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsNotDisplayed()

        // Verify the toggle switch behavior.
        assertNull(request)
        composeTestRule.onNodeWithText("Read and modify bookmarks").performClick()
        assertArrayEquals(listOf("bookmarks").toTypedArray(), request!!.optionalPermissions.toTypedArray())
        assertTrue(request!!.originPermissions.isEmpty())
        assertTrue(request!!.dataCollectionPermissions.isEmpty())

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsNotDisplayed()
    }

    @Test
    fun testRequiredAndOptionalPermissions() {
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = listOf("Access browser tabs"),
                    optionalPermissions = listOf(
                        Addon.LocalizedPermission(
                            "Read and modify bookmarks",
                            Addon.Permission("bookmarks", false),
                        ),
                    ),
                    originPermissions = emptyList(),
                    requiredDataCollectionPermissions = emptyList(),
                    optionalDataCollectionPermissions = emptyList(),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {},
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Access browser tabs").assertIsDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Read and modify bookmarks").assertIsDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsNotDisplayed()

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsNotDisplayed()
    }

    @Test
    fun testRequiredDataCollectionPermissions() {
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = emptyList(),
                    optionalPermissions = emptyList(),
                    originPermissions = emptyList(),
                    requiredDataCollectionPermissions = listOf("health information"),
                    optionalDataCollectionPermissions = emptyList(),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {},
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsDisplayed()
        composeTestRule.onNodeWithText("The developer says this extension collects: health information.")
            .assertIsDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsNotDisplayed()

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsNotDisplayed()
    }

    @Test
    fun testNoneDataCollectionPermissions() {
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = emptyList(),
                    optionalPermissions = emptyList(),
                    originPermissions = emptyList(),
                    requiredDataCollectionPermissions = listOf(),
                    hasNoneDataCollection = true,
                    optionalDataCollectionPermissions = emptyList(),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {},
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsDisplayed()
        composeTestRule.onNodeWithText("The developer says this extension doesn’t require data collection.")
            .assertIsDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsNotDisplayed()

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsNotDisplayed()
    }

    @Test
    fun testOptionalDataCollectionPermissions() {
        var request: AddonPermissionsUpdateRequest? = null
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = emptyList(),
                    optionalPermissions = emptyList(),
                    originPermissions = emptyList(),
                    requiredDataCollectionPermissions = emptyList(),
                    optionalDataCollectionPermissions = listOf(
                        Addon.LocalizedPermission(
                            "Share health information with extension developer",
                            Addon.Permission("healthInfo", false),
                        ),
                    ),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {
                        request = it
                    },
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsNotDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Share health information with extension developer").assertIsDisplayed()

        // Verify the toggle switch behavior.
        assertNull(request)
        composeTestRule.onNodeWithText("Share health information with extension developer").performClick()
        assertTrue(request!!.optionalPermissions.isEmpty())
        assertTrue(request!!.originPermissions.isEmpty())
        assertArrayEquals(listOf("healthInfo").toTypedArray(), request!!.dataCollectionPermissions.toTypedArray())

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsNotDisplayed()
    }

    @Test
    fun testAllPermissions() {
        composeTestRule.setContent {
            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = listOf("Access browser tabs"),
                    optionalPermissions = listOf(
                        Addon.LocalizedPermission(
                            "Read and modify bookmarks",
                            Addon.Permission("bookmarks", false),
                        ),
                    ),
                    originPermissions = listOf(
                        Addon.LocalizedPermission(
                            "Access mozilla.org",
                            Addon.Permission("*://mozilla.org/*", false),
                        ),
                    ),
                    requiredDataCollectionPermissions = listOf("health information"),
                    optionalDataCollectionPermissions = listOf(
                        Addon.LocalizedPermission(
                            "Share health information with extension developer",
                            Addon.Permission("healthInfo", false),
                        ),
                    ),
                    isAllSitesSwitchVisible = false,
                    isAllSitesEnabled = false,
                    onAddOptionalPermissions = {},
                    onRemoveOptionalPermissions = {},
                    onAddAllSitesPermissions = {},
                    onRemoveAllSitesPermissions = {},
                    onLearnMoreClick = {},
                )
            }
        }

        composeTestRule.onNodeWithText("Required permissions:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Access browser tabs").assertIsDisplayed()
        composeTestRule.onNodeWithText("Access mozilla.org").assertIsDisplayed()
        composeTestRule.onNodeWithText("Optional permissions:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Read and modify bookmarks").assertIsDisplayed()
        composeTestRule.onNodeWithText("Required data collection:").assertIsDisplayed()
        composeTestRule.onNodeWithText("The developer says this extension collects: health information.")
            .assertIsDisplayed()
        composeTestRule.onNodeWithText("Optional data collection:").assertIsDisplayed()
        composeTestRule.onNodeWithText("Share health information with extension developer").assertIsDisplayed()

        composeTestRule.onNodeWithText("This extension doesn’t require any permissions.").assertIsNotDisplayed()
    }
}
