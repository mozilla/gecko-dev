/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.theme.AcornTheme
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem
import org.mozilla.fenix.home.sessioncontrol.SetupChecklistInteractor
import org.mozilla.fenix.theme.FirefoxTheme

private const val ROTATE_180 = 180F

/**
 * Renders a checklist for onboarding users.
 *
 * @param interactor the interactor to handle user actions.
 * @param checklistItems The list of [ChecklistItem] displayed in setup checklist.
 */
@Composable
fun ChecklistView(
    interactor: SetupChecklistInteractor,
    checklistItems: List<ChecklistItem>,
) {
    Column {
        checklistItems.forEachIndexed { index, item ->
            when (item) {
                is ChecklistItem.Group -> GroupWithTasks(
                    group = item,
                    onChecklistItemClicked = { clickedTask ->
                        interactor.onChecklistItemClicked(clickedTask)
                    },
                    // No divider for the last group, in case it is the last element
                    // in the parent composable.
                    addDivider = index != checklistItems.size - 1,
                )

                is ChecklistItem.Task -> Task(item) { interactor.onChecklistItemClicked(item) }
            }
        }
    }
}

@Composable
private fun Task(
    task: ChecklistItem.Task,
    onChecklistItemClicked: (ChecklistItem) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .clickable(onClick = { onChecklistItemClicked(task) }),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (task.isCompleted) {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_checkmark_24),
                contentDescription = stringResource(R.string.a11y_completed_task_description),
                modifier = Modifier.padding(16.dp),
                tint = AcornTheme.colors.iconPrimary,
            )
        } else {
            Spacer(Modifier.size(56.dp))
        }

        Text(
            text = stringResource(task.title),
            modifier = Modifier
                .weight(1f)
                .semantics { heading() },
            style = FirefoxTheme.typography.subtitle1,
            color = FirefoxTheme.colors.textPrimary,
        )

        Icon(
            painter = painterResource(task.icon),
            contentDescription = stringResource(R.string.a11y_task_icon_description),
            modifier = Modifier.padding(16.dp),
            tint = FirefoxTheme.colors.iconPrimary,
        )
    }
}

@Composable
private fun GroupWithTasks(
    group: ChecklistItem.Group,
    onChecklistItemClicked: (ChecklistItem) -> Unit,
    addDivider: Boolean,
) {
    Column {
        Group(group, onChecklistItemClicked)

        if (group.isExpanded) {
            group.tasks.forEach { task -> Task(task, onChecklistItemClicked) }
        }

        if (addDivider) {
            Divider()
        }
    }
}

@Composable
private fun Group(
    group: ChecklistItem.Group,
    onChecklistItemClicked: (ChecklistItem) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .clickable { onChecklistItemClicked(group) },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(
            modifier = Modifier
                .weight(1f)
                .padding(start = 16.dp),
            horizontalAlignment = Alignment.Start,
        ) {
            Text(
                text = stringResource(group.title, stringResource(R.string.firefox)),
                style = FirefoxTheme.typography.subtitle1,
                color = FirefoxTheme.colors.textPrimary,
                modifier = Modifier.semantics { heading() },
            )

            Text(
                text = "${group.progress.completedTasks}/${group.progress.totalTasks}",
                style = FirefoxTheme.typography.body2,
                color = FirefoxTheme.colors.textSecondary,
            )
        }

        Icon(
            painter = painterResource(id = R.drawable.ic_arrowhead_down),
            contentDescription = "",
            modifier = Modifier
                .padding(16.dp)
                .rotate(if (group.isExpanded) ROTATE_180 else 0f),
            tint = FirefoxTheme.colors.iconPrimary,
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun TasksChecklistPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(top = 16.dp),
        ) {
            val tasks = listOf(
                ChecklistItem.Task(
                    type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
                    title = R.string.setup_checklist_task_explore_extensions,
                    icon = R.drawable.ic_addons_extensions,
                    isCompleted = true,
                ),
                ChecklistItem.Task(
                    type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
                    title = R.string.setup_checklist_task_search_widget,
                    icon = R.drawable.ic_search,
                    isCompleted = false,
                ),
            )

            val group1 = ChecklistItem.Group(
                title = R.string.setup_checklist_group_essentials,
                tasks = tasks,
                isExpanded = true,
            )
            val group2 = ChecklistItem.Group(
                title = R.string.setup_checklist_group_customize,
                tasks = tasks,
                isExpanded = false,
            )

            ChecklistView(
                interactor = object : SetupChecklistInteractor {
                    override fun onChecklistItemClicked(item: ChecklistItem) { /* no op */ }
                    override fun onRemoveChecklistButtonClicked() { /* no op */ }
                },
                checklistItems = listOf(group1, group2),
            )
        }
    }
}
