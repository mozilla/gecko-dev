/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.checklist

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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
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
import org.mozilla.fenix.theme.FirefoxTheme

private const val ROTATE_180 = 180F

/**
 * Renders a checklist for onboarding users.
 *
 * @param checkListItems The list of [ChecklistItem] displayed in setup checklist.
 * @param onChecklistItemClicked Gets invoked when the user clicks a check list item.
 */
@Composable
fun CheckListView(
    checkListItems: List<ChecklistItem>,
    onChecklistItemClicked: (ChecklistItem) -> Unit,
) {
    LazyColumn {
        itemsIndexed(checkListItems) { index, item ->
            when (item) {
                is ChecklistItem.Group -> GroupWithTasks(
                    group = item,
                    onChecklistItemClicked = onChecklistItemClicked,
                    // No divider for the last group, in case it is the last element
                    // in the parent composable.
                    addDivider = index != checkListItems.size - 1,
                )
                is ChecklistItem.Task -> Task(item, onChecklistItemClicked)
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
            text = task.title,
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
@Suppress("LongMethod")
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
                text = group.title,
                style = FirefoxTheme.typography.subtitle1,
                color = FirefoxTheme.colors.textPrimary,
                modifier = Modifier.semantics { heading() },
            )

            Text(
                text = "1/1",
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
                    title = "First task",
                    icon = R.drawable.ic_addons_extensions,
                    isCompleted = true,
                ),
                ChecklistItem.Task(
                    type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
                    title = "Second task",
                    icon = R.drawable.ic_search,
                    isCompleted = false,
                ),
            )

            val group1 = ChecklistItem.Group(
                title = "First group",
                tasks = tasks,
                isExpanded = true,
            )
            val group2 = ChecklistItem.Group(
                title = "Second group",
                tasks = tasks,
                isExpanded = false,
            )

            CheckListView(
                checkListItems = listOf(group1, group2),
                onChecklistItemClicked = {},
            )
        }
    }
}
