/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.checklist

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.IconButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The list with all onboarding tasks.
 *
 * @param tasks The list of the tasks displayed in setup checklist.
 */
@Composable
fun TasksListView(tasks: List<Task>) {
    Column {
        tasks.forEach { task ->
            TaskRow(task)
        }
    }
}

/**
 * The task that contains the title of the task and the number of subtasks completed if any.
 */
@Composable
@Suppress("LongMethod")
fun TaskRow(task: Task) {
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
                text = task.title,
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
            val task1 = Task(
                title = "First task",
                isCompleted = false,
                iCollapsed = true,
            )
            val task2 = Task(
                title = "Second task",
                isCompleted = true,
                iCollapsed = true,
            )

            TasksListView(
                tasks = listOf(task1, task2),
            )
        }
    }
}
