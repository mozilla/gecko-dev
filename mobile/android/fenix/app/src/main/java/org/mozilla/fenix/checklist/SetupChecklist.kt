/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.checklist

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.theme.layout.AcornLayout
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.home.setup.store.SetupChecklistAction
import org.mozilla.fenix.home.setup.store.SetupChecklistState
import org.mozilla.fenix.home.setup.store.SetupChecklistStore
import org.mozilla.fenix.theme.FirefoxTheme

private val elevation = AcornLayout.AcornElevation.xLarge
private val shapeChecklist = RoundedCornerShape(size = AcornLayout.AcornCorner.large)

/**
 * The Setup checklist displayed on homepage that contains onboarding tasks.
 *
 * @param setupChecklistStore The [SetupChecklistStore] used to manage the state of feature.
 * @param title The checklist's title.
 * @param subtitle The checklist's subtitle.
 * @param allTasksCompleted If all tasks are completed.
 * @param labelRemoveChecklistButton The label of the checklist's button to remove it.
 * @param onChecklistItemClicked Gets invoked when the user clicks a check list item.
 * @param onRemoveChecklistButtonClicked Invoked when the remove button is clicked.
 */
@Composable
fun SetupChecklist(
    setupChecklistStore: SetupChecklistStore,
    title: String,
    subtitle: String,
    allTasksCompleted: Boolean,
    labelRemoveChecklistButton: String,
    onChecklistItemClicked: (ChecklistItem) -> Unit,
    onRemoveChecklistButtonClicked: () -> Unit,
) {
    val setupChecklistState by setupChecklistStore.observeAsState(initialValue = setupChecklistStore.state) { it }

    Card(
        shape = shapeChecklist,
        backgroundColor = FirefoxTheme.colors.layer1,
        elevation = elevation,
    ) {
        Column(
            verticalArrangement = Arrangement.spacedBy(0.dp, Alignment.CenterVertically),
            horizontalAlignment = Alignment.Start,
        ) {
            Header(title, subtitle)

            CheckListView(setupChecklistState.checklistItems, onChecklistItemClicked)

            if (allTasksCompleted) {
                RemoveChecklistButton(labelRemoveChecklistButton, onRemoveChecklistButtonClicked)
            }
        }
    }
}

/**
 * The header for setup checklist that contains the title, the subtitle and the progress bar
 */
@Composable
private fun Header(title: String, subtitle: String) {
    Column(
        modifier = Modifier.padding(
            horizontal = 16.dp,
            vertical = 12.dp,
        ),
        verticalArrangement = Arrangement.spacedBy(
            12.dp,
            Alignment.Top,
        ),
        horizontalAlignment = Alignment.Start,
    ) {
        Text(
            text = title,
            style = FirefoxTheme.typography.headline7,
            color = FirefoxTheme.colors.textPrimary,
            modifier = Modifier.semantics { heading() },
        )

        Text(
            text = subtitle,
            style = FirefoxTheme.typography.body2,
            color = FirefoxTheme.colors.textPrimary,
        )

        ProgressIndicatorSetupChecklist()
    }
}

/**
 * The button that will remove the setup checklist from homepage.
 */
@Composable
fun RemoveChecklistButton(
    labelRemoveChecklistButton: String,
    onRemoveChecklistButtonClicked: () -> Unit,
) {
    Column(
        modifier = Modifier.padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp, Alignment.CenterVertically),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp, Alignment.CenterHorizontally),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            PrimaryButton(
                text = labelRemoveChecklistButton,
                modifier = Modifier
                    .width(width = FirefoxTheme.layout.size.maxWidth.small),
                onClick = onRemoveChecklistButtonClicked,
            )
        }
    }
}

private class SetupChecklistPreviewParameterProvider :
    PreviewParameterProvider<SetupChecklistState> {
    override val values: Sequence<SetupChecklistState>
        get() = sequenceOf(
            SetupChecklistState(),
            SetupChecklistState(
                checklistItems = listOf(
                    ChecklistItem.Group(
                        title = "First group",
                        tasks = listOf(
                            ChecklistItem.Task(
                                title = "First task",
                                icon = R.drawable.ic_addons_extensions,
                                isCompleted = true,
                            ),
                            ChecklistItem.Task(
                                title = "Second task",
                                icon = R.drawable.ic_search,
                                isCompleted = false,
                            ),
                        ),
                        isExpanded = true,
                    ),
                    ChecklistItem.Group(
                        title = "Second group",
                        tasks = listOf(
                            ChecklistItem.Task(
                                title = "Third task",
                                icon = R.drawable.ic_addons_extensions,
                                isCompleted = true,
                            ),
                            ChecklistItem.Task(
                                title = "Forth task",
                                icon = R.drawable.ic_search,
                                isCompleted = false,
                            ),
                        ),
                        isExpanded = false,
                    ),
                ),
            ),
        )
}

@FlexibleWindowLightDarkPreview
@Composable
private fun SetupChecklistPreview(
    @PreviewParameter(SetupChecklistPreviewParameterProvider::class) initialState: SetupChecklistState,
) {
    val store = remember { SetupChecklistStore(initialState = initialState) }

    FirefoxTheme {
        Spacer(Modifier.height(16.dp))

        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(16.dp),
        ) {
            SetupChecklist(
                setupChecklistStore = store,
                title = "Finish setting up Firefox",
                subtitle = "Complete all 6 steps to set up Firefox for the best browsing experience.",
                allTasksCompleted = true,
                labelRemoveChecklistButton = "Remove checklist",
                onChecklistItemClicked = { item ->
                    store.dispatch(SetupChecklistAction.ChecklistItemClicked(item))
                },
                onRemoveChecklistButtonClicked = {},
            )
        }
    }
}
