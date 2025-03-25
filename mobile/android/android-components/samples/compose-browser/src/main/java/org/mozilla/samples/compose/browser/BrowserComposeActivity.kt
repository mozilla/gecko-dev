/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Scaffold
import androidx.compose.ui.Modifier
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.support.ktx.android.view.setupPersistentInsets
import org.mozilla.samples.compose.browser.browser.BrowserScreen
import org.mozilla.samples.compose.browser.ext.components
import org.mozilla.samples.compose.browser.settings.SettingsScreen

/**
 * Ladies and gentleman, the browser. ¯\_(ツ)_/¯
 */
class BrowserComposeActivity : AppCompatActivity() {
    companion object {
        const val ROUTE_BROWSER = "browser"
        const val ROUTE_SETTINGS = "settings"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        window.setupPersistentInsets()

        setContent {
            val navController = rememberNavController()

            AcornTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    Box(modifier = Modifier.padding(innerPadding)) {
                        NavHost(navController, startDestination = ROUTE_BROWSER) {
                            composable(ROUTE_BROWSER) { BrowserScreen() }
                            composable(ROUTE_SETTINGS) { SettingsScreen() }
                        }
                    }
                }
            }
        }

        components.fxSuggestIngestionScheduler.startPeriodicIngestion()
    }
}
