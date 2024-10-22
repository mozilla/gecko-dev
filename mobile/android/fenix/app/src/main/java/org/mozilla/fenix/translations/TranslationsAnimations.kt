/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.togetherWith
import androidx.compose.runtime.Composable
import org.mozilla.fenix.utils.DELAY_MS_TRANSLATIONS
import org.mozilla.fenix.utils.DURATION_MS_TRANSLATIONS
import org.mozilla.fenix.utils.contentGrowth
import org.mozilla.fenix.utils.enterMenu
import org.mozilla.fenix.utils.enterSubmenu
import org.mozilla.fenix.utils.exitMenu
import org.mozilla.fenix.utils.exitSubmenu

@Composable
internal fun TranslationsAnimation(
    showMainSheet: Boolean,
    content: @Composable AnimatedVisibilityScope.(Boolean) -> Unit,
) {
    AnimatedContent(
        targetState = showMainSheet,
        transitionSpec = {
            if (initialState && !targetState) {
                // Entering the settings area from the main translation area
                enterSubmenu(
                    duration = DURATION_MS_TRANSLATIONS,
                    delay = DELAY_MS_TRANSLATIONS,
                ).togetherWith(
                    exitMenu(duration = DURATION_MS_TRANSLATIONS),
                ) using SizeTransform { initialSize, targetSize ->
                    contentGrowth(
                        initialSize = initialSize,
                        targetSize = targetSize,
                        duration = DURATION_MS_TRANSLATIONS,
                    )
                }
            } else {
                // Entering the main translations area from the settings area
                (
                    enterMenu(
                        duration = DURATION_MS_TRANSLATIONS,
                        delay = DELAY_MS_TRANSLATIONS,
                    )
                    ).togetherWith(
                    exitSubmenu(duration = DURATION_MS_TRANSLATIONS),
                ) using SizeTransform { initialSize, targetSize ->
                    contentGrowth(
                        initialSize = initialSize,
                        targetSize = targetSize,
                        duration = DURATION_MS_TRANSLATIONS,
                    )
                }
            }
        },
        label = "TranslationsAnimation",
    ) { showMainPage ->
        content(showMainPage)
    }
}
