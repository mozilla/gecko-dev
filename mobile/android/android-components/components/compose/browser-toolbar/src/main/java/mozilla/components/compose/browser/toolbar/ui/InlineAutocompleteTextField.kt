/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.text.InputType.TYPE_CLASS_TEXT
import android.text.InputType.TYPE_TEXT_VARIATION_URI
import android.util.TypedValue
import android.view.Gravity
import android.view.inputmethod.EditorInfo
import androidx.annotation.ColorInt
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.graphics.toColorInt
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.BrowserEditToolbar
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.AutocompleteProvider
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.NamedThreadFactory
import mozilla.components.support.ktx.android.view.showKeyboard
import mozilla.components.ui.autocomplete.AutocompleteView
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import mozilla.components.ui.autocomplete.OnFilterListener
import java.util.concurrent.Executors
import kotlin.coroutines.CoroutineContext

private const val TEXT_SIZE = 15f
private const val TEXT_HIGHLIGHT_COLOR = "#5C592ACB"
private const val AUTOCOMPLETE_QUERY_THREADS = 3
private const val AUTOCOMPLETE_THREADS_FACTORY_NAME = "EditToolbar"

/**
 * Sub-component of the [BrowserEditToolbar] responsible for displaying a text field that is
 * capable of inline autocompletion.
 */
@Composable
internal fun InlineAutocompleteTextField(
    url: String,
    autocompleteProviders: List<AutocompleteProvider>,
    modifier: Modifier = Modifier,
    onUrlEdit: (String) -> Unit = {},
    onUrlCommitted: (String) -> Unit = {},
    onUrlSuggestionAutocompleted: (String) -> Unit = {},
) {
    val context = LocalContext.current
    val textColor = AcornTheme.colors.textPrimary
    val hintColor = AcornTheme.colors.textSecondary
    val backgroundColor = AcornTheme.colors.layer3
    val backgroundDrawable = remember { buildBackground(context, backgroundColor.toArgb()) }
    val autocompletedTextColor = remember { TEXT_HIGHLIGHT_COLOR.toColorInt() }
    val logger = remember { Logger("InlineAutocompleteTextField") }

    val autocompleteDispatcher = remember {
        SupervisorJob() +
            Executors.newFixedThreadPool(
                AUTOCOMPLETE_QUERY_THREADS,
                NamedThreadFactory(AUTOCOMPLETE_THREADS_FACTORY_NAME),
            ).asCoroutineDispatcher() +
            CoroutineExceptionHandler { _, throwable ->
                logger.error("Error while processing autocomplete input", throwable)
            }
    }

    var editText by remember { mutableStateOf<InlineAutocompleteEditText?>(null) }

    // Doing this here and not in the "update" block to change the autocomplete filter
    // only when the autocomplete providers change, and not for every recomposition / other parameter changes.
    LaunchedEffect(autocompleteProviders) {
        logger.debug("Refreshing autocomplete suggestions from ${autocompleteProviders.size} providers.")

        editText?.let {
            it.setOnFilterListener(
                AsyncFilterListener(
                    it, autocompleteDispatcher,
                    object : suspend (String, AutocompleteDelegate) -> Unit {
                        override suspend fun invoke(
                            query: String,
                            delegate: AutocompleteDelegate,
                        ) {
                            if (autocompleteProviders.isEmpty() || query.isBlank()) {
                                delegate.noAutocompleteResult(query)
                            } else {
                                val result = autocompleteProviders
                                    .firstNotNullOfOrNull { it.getAutocompleteSuggestion(query) }

                                if (result != null) {
                                    delegate.applyAutocompleteResult(result) {
                                        onUrlSuggestionAutocompleted(result.url)
                                    }
                                } else {
                                    delegate.noAutocompleteResult(query)
                                }
                            }
                        }
                    },
                ),
            )

            it.refreshAutocompleteSuggestions()
        }
    }

    AndroidView(
        factory = { context ->
            InlineAutocompleteEditText(context).apply {
                imeOptions = EditorInfo.IME_ACTION_GO or
                    EditorInfo.IME_FLAG_NO_EXTRACT_UI or
                    EditorInfo.IME_FLAG_NO_FULLSCREEN
                inputType = TYPE_CLASS_TEXT or TYPE_TEXT_VARIATION_URI
                setLines(1)
                gravity = Gravity.CENTER_VERTICAL
                setTextSize(TypedValue.COMPLEX_UNIT_SP, TEXT_SIZE)
                setFocusable(true)
                background = backgroundDrawable
                autoCompleteBackgroundColor = autocompletedTextColor
                setTextColor(textColor.toArgb())
                setHintTextColor(hintColor.toArgb())

                updateText(url)

                setOnCommitListener {
                    onUrlCommitted(text.toString())
                }

                setOnTextChangeListener { text, _ ->
                    onUrlEdit(text)
                }
            }.also {
                editText = it
            }
        },
        modifier = modifier,
        update = {
            if (url != it.originalText) {
                it.updateText(url)
                it.refreshAutocompleteSuggestions()
            }
        },
    )
}

/**
 * Wraps [filter] execution in a coroutine context, cancelling prior executions on every invocation.
 * [coroutineContext] must be of type that doesn't propagate cancellation of its children upwards.
 */
private class AsyncFilterListener(
    private val urlView: AutocompleteView,
    override val coroutineContext: CoroutineContext,
    private val filter: suspend (String, AutocompleteDelegate) -> Unit,
    private val uiContext: CoroutineContext = Dispatchers.Main,
) : OnFilterListener, CoroutineScope {
    override fun invoke(text: String) {
        // We got a new input, so whatever past autocomplete queries we still have running are
        // irrelevant. We cancel them, but do not depend on cancellation to take place.
        coroutineContext.cancelChildren()

        CoroutineScope(coroutineContext).launch {
            filter(text, AsyncAutocompleteDelegate(urlView, this, uiContext))
        }
    }
}

/**
 * An autocomplete delegate which is aware of its parent scope (to check for cancellations).
 * Responsible for processing autocompletion results and discarding stale results when [urlView] moved on.
 */
private class AsyncAutocompleteDelegate(
    private val urlView: AutocompleteView,
    private val parentScope: CoroutineScope,
    override val coroutineContext: CoroutineContext,
    private val logger: Logger = Logger("AsyncAutocompleteDelegate"),
) : AutocompleteDelegate, CoroutineScope {
    override fun applyAutocompleteResult(result: AutocompleteResult, onApplied: () -> Unit) {
        // Bail out if we were cancelled already.
        if (!parentScope.isActive) {
            logger.debug("Autocomplete request cancelled. Discarding results.")
            return
        }

        // Process results on the UI dispatcher.
        CoroutineScope(coroutineContext).launch {
            // Ignore this result if the query is stale.
            if (result.input == urlView.originalText.lowercase()) {
                urlView.applyAutocompleteResult(
                    InlineAutocompleteEditText.AutocompleteResult(
                        text = result.text,
                        source = result.source,
                        totalItems = result.totalItems,
                    ),
                )
                onApplied()
            } else {
                logger.debug("Discarding stale autocomplete result.")
            }
        }
    }

    override fun noAutocompleteResult(input: String) {
        // Bail out if we were cancelled already.
        if (!parentScope.isActive) {
            logger.debug("Autocomplete request cancelled. Discarding 'noAutocompleteResult'.")
            return
        }

        // Process results on the UI thread.
        CoroutineScope(coroutineContext).launch {
            // Ignore this result if the query is stale.
            if (input == urlView.originalText) {
                urlView.noAutocompleteResult()
            } else {
                logger.debug("Discarding stale lack of autocomplete results.")
            }
        }
    }
}

private fun buildBackground(
    context: Context,
    @ColorInt color: Int,
    cornerRadius: Float = 8f,
): GradientDrawable {
    val cornerRadiusPx = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP,
        cornerRadius,
        context.resources.displayMetrics,
    )

    return GradientDrawable().apply {
        shape = GradientDrawable.RECTANGLE
        setColor(color)
        this.cornerRadius = cornerRadiusPx
    }
}

private fun InlineAutocompleteEditText.updateText(newText: String) {
    // Avoid running the code for focusing this if the updated text is the one user already typed.
    // But ensure focusing this if just starting to type.
    if (text.toString() == newText && newText.isNotEmpty()) return

    setText(text = newText, shouldAutoComplete = false)
    setSelection(newText.length)
    if (!hasFocus()) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            // On Android 14 this needs to be called before requestFocus() in order to receive focus.
            isFocusableInTouchMode = true
        }
        requestFocus()
        showKeyboard()
    }
}

@PreviewLightDark
@Composable
private fun BrowserEditToolbarPreview() {
    InlineAutocompleteTextField(
        url = "http://www.mozilla.org",
        autocompleteProviders = emptyList(),
    )
}
