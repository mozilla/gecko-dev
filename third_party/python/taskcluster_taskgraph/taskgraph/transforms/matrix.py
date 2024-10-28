# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Transforms used to split one task definition into many tasks, governed by a
matrix defined in the definition.
"""

from copy import deepcopy
from textwrap import dedent

from voluptuous import Extra, Optional, Required

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema
from taskgraph.util.templates import substitute_task_fields

MATRIX_SCHEMA = Schema(
    {
        Required("name"): str,
        Optional("matrix"): {
            Optional(
                "exclude",
                description=dedent(
                    """
                Exclude the specified combination(s) of matrix values from the
                final list of tasks.

                If only a subset of the possible rows are present in the
                exclusion rule, then *all* combinations including that subset
                subset will be excluded.
                """.lstrip()
                ),
            ): [{str: str}],
            Optional(
                "set-name",
                description=dedent(
                    """
                Sets the task name to the specified format string.

                Useful for cases where the default of joining matrix values by
                a dash is not desired.
                """.lstrip()
                ),
            ): str,
            Optional(
                "substitution-fields",
                description=dedent(
                    """
                List of fields in the task definition to substitute matrix values into.

                If not specified, all fields in the task definition will be
                substituted.
                """
                ),
            ): [str],
            Extra: [str],
        },
        Extra: object,
    },
)
"""Schema for matrix transforms."""

transforms = TransformSequence()
transforms.add_validate(MATRIX_SCHEMA)


def _resolve_matrix(tasks, key, values, exclude):
    for task in tasks:
        for value in values:
            new_task = deepcopy(task)
            new_task["name"] = f"{new_task['name']}-{value}"

            matrix = new_task.setdefault("attributes", {}).setdefault("matrix", {})
            matrix[key] = value

            for rule in exclude:
                if all(matrix.get(k) == v for k, v in rule.items()):
                    break
            else:
                yield new_task


@transforms.add
def split_matrix(config, tasks):
    for task in tasks:
        if "matrix" not in task:
            yield task
            continue

        matrix = task.pop("matrix")
        set_name = matrix.pop("set-name", None)
        fields = matrix.pop("substitution-fields", task.keys())
        exclude = matrix.pop("exclude", {})

        new_tasks = [task]
        for key, values in matrix.items():
            new_tasks = _resolve_matrix(new_tasks, key, values, exclude)

        for new_task in new_tasks:
            if set_name:
                if "name" not in fields:
                    fields.append("name")
                new_task["name"] = set_name

            substitute_task_fields(
                new_task,
                fields,
                matrix=new_task["attributes"]["matrix"],
            )
            yield new_task
