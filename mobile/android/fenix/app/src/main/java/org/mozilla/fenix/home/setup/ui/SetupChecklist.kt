/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.PrimaryButton
import mozilla.components.compose.base.theme.layout.AcornLayout
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem
import org.mozilla.fenix.components.appstate.setup.checklist.SetupChecklistState
import org.mozilla.fenix.components.appstate.setup.checklist.checklistItemsAreGroups
import org.mozilla.fenix.components.appstate.setup.checklist.getSetupChecklistSubtitle
import org.mozilla.fenix.components.appstate.setup.checklist.getSetupChecklistTitle
import org.mozilla.fenix.home.sessioncontrol.SetupChecklistInteractor
import org.mozilla.fenix.theme.FirefoxTheme

private val elevation = AcornLayout.AcornElevation.xLarge
private val shapeChecklist = RoundedCornerShape(size = AcornLayout.AcornCorner.large)

/**
 * The Setup checklist displayed on homepage that contains onboarding tasks.
 *
 * @param setupChecklistState The [SetupChecklistState] used to manage the state of feature.
 * @param interactor The [SetupChecklistInteractor] used to handle user interactions.
 */
@Composable
fun SetupChecklist(setupChecklistState: SetupChecklistState, interactor: SetupChecklistInteractor) {
    Card(
        modifier = Modifier.padding(16.dp),
        shape = shapeChecklist,
        backgroundColor = FirefoxTheme.colors.layer1,
        elevation = elevation,
    ) {
        Column(
            verticalArrangement = Arrangement.spacedBy(0.dp, Alignment.CenterVertically),
            horizontalAlignment = Alignment.Start,
        ) {
            Header(setupChecklistState)

            ChecklistView(
                interactor = interactor,
                checklistItems = setupChecklistState.checklistItems,
            )

            if (setupChecklistState.progress.allTasksCompleted()) {
                Divider()

                RemoveChecklistButton(interactor)
            }
        }
    }
}

/**
 * The header for setup checklist that contains the title, the subtitle and the progress bar.
 */
@Composable
private fun Header(state: SetupChecklistState) {
    val context = LocalContext.current
    Column(
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp, Alignment.Top),
        horizontalAlignment = Alignment.Start,
    ) {
        val progress = state.progress

        Text(
            text = getSetupChecklistTitle(
                context = context,
                allTasksCompleted = progress.allTasksCompleted(),
            ),
            style = FirefoxTheme.typography.headline7,
            color = FirefoxTheme.colors.textPrimary,
            modifier = Modifier.semantics { heading() },
        )

        getSetupChecklistSubtitle(
            context = context,
            progress = progress,
            isGroups = state.checklistItems.checklistItemsAreGroups(),
        )
            ?.let {
                Text(
                    text = it,
                    style = FirefoxTheme.typography.body2,
                    color = FirefoxTheme.colors.textPrimary,
                )
            }

        ProgressBarSetupChecklistView(progress.totalTasks, progress.completedTasks)
    }
}

/**
 * The button that will remove the setup checklist from homepage.
 */
@Composable
private fun RemoveChecklistButton(interactor: SetupChecklistInteractor) {
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
                text = stringResource(R.string.setup_checklist_button_remove),
                modifier = Modifier.width(width = FirefoxTheme.layout.size.maxWidth.small),
                onClick = { interactor.onRemoveChecklistButtonClicked() },
            )
        }
    }
}

private class SetupChecklistPreviewParameterProvider :
    PreviewParameterProvider<SetupChecklistState> {
    override val values: Sequence<SetupChecklistState>
        get() = sequenceOf(
            SetupChecklistState(checklistItems = createPreviewTasks()),
            SetupChecklistState(checklistItems = createPreviewGroups()),
        )
}

private fun createPreviewTasks() = listOf(
    setAsDefaultTaskPreview(),
    webExtensionTaskPreview(),
    signInTaskPreview(),
)

private fun setAsDefaultTaskPreview() = ChecklistItem.Task(
    type = ChecklistItem.Task.Type.SET_AS_DEFAULT,
    title = R.string.setup_checklist_task_default_browser,
    icon = R.drawable.mozac_ic_web_extension_default_icon,
    isCompleted = false,
)

private fun webExtensionTaskPreview() = ChecklistItem.Task(
    type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
    title = R.string.setup_checklist_task_explore_extensions,
    icon = R.drawable.mozac_ic_web_extension_default_icon,
    isCompleted = false,
)

private fun signInTaskPreview() = ChecklistItem.Task(
    type = ChecklistItem.Task.Type.SIGN_IN,
    title = R.string.setup_checklist_task_account_sync,
    icon = R.drawable.mozac_ic_web_extension_default_icon,
    isCompleted = true,
)

private fun createPreviewGroups() = listOf(
    ChecklistItem.Group(
        title = R.string.setup_checklist_group_essentials,
        tasks = listOf(setAsDefaultTaskPreview(), signInTaskPreview()),
        isExpanded = true,
    ),
    ChecklistItem.Group(
        title = R.string.setup_checklist_group_customize,
        tasks = listOf(
            ChecklistItem.Task(
                type = ChecklistItem.Task.Type.SELECT_THEME,
                title = R.string.setup_checklist_task_toolbar_selection,
                icon = R.drawable.mozac_ic_web_extension_default_icon,
                isCompleted = false,
            ),
            ChecklistItem.Task(
                type = ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT,
                title = R.string.setup_checklist_task_theme_selection,
                icon = R.drawable.mozac_ic_web_extension_default_icon,
                isCompleted = false,
            ),
        ),
        isExpanded = false,
    ),
    ChecklistItem.Group(
        title = R.string.setup_checklist_group_helpful_tools,
        tasks = listOf(
            ChecklistItem.Task(
                type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
                title = R.string.setup_checklist_task_search_widget,
                icon = R.drawable.mozac_ic_web_extension_default_icon,
                isCompleted = false,
            ),
            ChecklistItem.Task(
                type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
                title = R.string.setup_checklist_task_explore_extensions,
                icon = R.drawable.mozac_ic_web_extension_default_icon,
                isCompleted = false,
            ),
        ),
        isExpanded = false,
    ),
)

@FlexibleWindowLightDarkPreview
@Composable
private fun SetupChecklistPreview(
    @PreviewParameter(SetupChecklistPreviewParameterProvider::class) initialState: SetupChecklistState,
) {
    FirefoxTheme {
        Spacer(Modifier.height(16.dp))

        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .fillMaxHeight()
                .padding(16.dp),
        ) {
            SetupChecklist(
                setupChecklistState = initialState,
                interactor = object : SetupChecklistInteractor {
                    override fun onChecklistItemClicked(item: ChecklistItem) { /* no op */ }
                    override fun onRemoveChecklistButtonClicked() { /* no op */ }
                },
            )
        }
    }
}
