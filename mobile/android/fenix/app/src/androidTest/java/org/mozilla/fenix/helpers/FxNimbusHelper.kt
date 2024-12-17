package org.mozilla.fenix.helpers

import android.util.Log
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.nimbus.Translations

object FxNimbusHelper {
    /**
     * Disable the translations prompt after a page that can be translated is loaded.
     */
    fun disablePageLoadTranslationsPrompt() {
        Log.i(TAG, "disableTranslationsPrompt: Trying to disable the translations prompt")
        FxNimbus.features.translations.withInitializer { _, _ ->
            Translations(
                mainFlowToolbarEnabled = false,
            )
        }
        Log.i(TAG, "disableTranslationsPrompt: Disabled the translations prompt")
    }

    /**
     * Enable the translations prompt after a page that can be translated is loaded.
     */
    fun enablePageLoadTranslationsPrompt() {
        Log.i(TAG, "enableTranslationsPrompt: Trying to enable the translations prompt")
        FxNimbus.features.translations.withInitializer { _, _ ->
            Translations(
                mainFlowToolbarEnabled = true,
            )
        }
        Log.i(TAG, "enableTranslationsPrompt: Enabled the translations prompt")
    }
}
