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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.theme.AcornTheme
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.IconButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Renders a checklist for onboarding users.
 *
 * @param checkListItems The list of [ChecklistItem] displayed in setup checklist.
 */
@Composable
fun CheckListView(checkListItems: List<ChecklistItem>) {
    LazyColumn {
        items(checkListItems) { item ->
            when (item) {
                is ChecklistItem.Group -> GroupWithTasks(item)
                is ChecklistItem.Task -> Task(item)
            }
        }
    }
}

@Composable
private fun Task(task: ChecklistItem.Task) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp),
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
            style = FirefoxTheme.typography.headline7,
            color = FirefoxTheme.colors.textPrimary,
        )

        Icon(
            painter = task.icon,
            contentDescription = stringResource(R.string.a11y_task_icon_description),
            modifier = Modifier.padding(16.dp),
            tint = FirefoxTheme.colors.iconPrimary,
        )
    }
}

@Composable
private fun GroupWithTasks(group: ChecklistItem.Group) {
    Column {
        Group(group)
        if (group.isExpanded) {
            group.tasks.forEach { task -> Task(task) }
        }
    }
}

@Composable
@Suppress("LongMethod")
private fun Group(group: ChecklistItem.Group) {
    Row(
        modifier = Modifier
            .padding(
                horizontal = 16.dp,
                vertical = 8.dp,
            )
            .fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.Start,
        ) {
            Text(
                text = group.title,
                style = FirefoxTheme.typography.headline7,
                color = FirefoxTheme.colors.textPrimary,
                modifier = Modifier.semantics { heading() },
            )

            Text(
                text = "1/1",
                style = FirefoxTheme.typography.body2,
                color = FirefoxTheme.colors.textSecondary,
            )
        }

        Row(horizontalArrangement = Arrangement.End) {
            IconButton(onClick = {}) {
                Icon(
                    painter = painterResource(id = R.drawable.ic_arrowhead_down),
                    tint = FirefoxTheme.colors.textPrimary,
                    contentDescription = "",
                    modifier = Modifier
                        .size(26.dp)
                        .padding(1.dp),
                )
            }
        }
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
                    title = "First task",
                    icon = painterResource(id = R.drawable.ic_addons_extensions),
                    isCompleted = true,
                ),
                ChecklistItem.Task(
                    title = "Second task",
                    icon = painterResource(id = R.drawable.ic_search),
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
            )
        }
    }
}
