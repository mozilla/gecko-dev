Try
===

"Try" is a way to "try out" a proposed change safely before review, without
officially landing it.  This functionality has been around for a *long* time in
various forms, and can sometimes show its age.

Access to "push to try" is typically available to a much larger group of
developers than those who can land changes in integration and release branches.
Specifically, try pushes are allowed for anyone with `SCM Level`_ 1, while
integration branches are at SCM level 3.

Scheduling a Task on Try
------------------------

There are three methods for scheduling a task on try: legacy try option syntax,
try task config, and an empty try.

Try Option Syntax
:::::::::::::::::

The first, older method is a command line string called ``try syntax`` which is passed
into the decision task via the commit message. The resulting commit is then
pushed to the https://hg.mozilla.org/try repository.  An example try syntax
might look like:

.. parsed-literal::

    try: -b o -p linux64 -u mochitest-1 -t none

This gets parsed by ``taskgraph.try_option_syntax:TryOptionSyntax`` and returns
a list of matching task labels. For more information see the
`TryServer wiki page <https://wiki.mozilla.org/Try>`_.

Try Task Config
:::::::::::::::

The second, more modern method specifies exactly the tasks to run.  That list
of tasks is usually generated locally with some :doc:`local tool </tools/try/selectors/fuzzy>`
and attached to the commit pushed to the try repository. This gives
finer control over exactly what runs and enables growth of an
ecosystem of tooling appropriate to varied circumstances.

Implementation
,,,,,,,,,,,,,,

This method uses a checked-in file called ``try_task_config.json`` which lives
at the root of the source dir. The JSON object in this file contains a
``tasks`` key giving the labels of the tasks to run.  For example, the
``try_task_config.json`` file might look like:

.. parsed-literal::

    {
      "version": 1,
      "tasks": [
        "test-windows10-64/opt-web-platform-tests-12",
        "test-windows7-32/opt-reftest-1",
        "test-windows7-32/opt-reftest-2",
        "test-windows7-32/opt-reftest-3",
        "build-linux64/debug",
        "source-test-mozlint-eslint"
      ]
    }

Very simply, this will run any task label that gets passed in as well as their
dependencies. While it is possible to manually commit this file and push to
try, it is mainly meant to be a generation target for various :doc:`tryselect </tools/try>`
choosers.  For example:

.. parsed-literal::

    $ ./mach try fuzzy

A list of all possible task labels can be obtained by running:

.. parsed-literal::

    $ ./mach taskgraph tasks

A list of task labels relevant to a tree (defaults to mozilla-central) can be
obtained with:

.. parsed-literal::

    $ ./mach taskgraph target

Modifying Tasks in a Try Push
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

It's possible to alter the definition of a task with templates. Templates are
`JSON-e`_ files that live in the `taskgraph module`_. Templates can be specified
from the ``try_task_config.json`` like this:

.. parsed-literal::

    {
      "version": 1,
      "tasks": [...],
      "templates": {
        artifact: {"enabled": 1}
      }
    }

Each key in the templates object denotes a new template to apply, and the value
denotes extra context to use while rendering. When specified, a template will
be applied to every task no matter what. If the template should only be applied
to certain kinds of tasks, this needs to be specified in the template itself
using JSON-e `condition statements`_.

The context available to the JSON-e render contains attributes from the
:py:class:`taskgraph.task.Task` class. It looks like this:

.. parsed-literal::

    {
      "attributes": task.attributes,
      "kind": task.kind,
      "label": task.label,
      "target_tasks": [<tasks from try_task_config.json>],
      "task": task.task,
      "taskId": task.task_id,
      "input": ...
    }

The ``input`` context can be any arbitrary value or object. What it contains
depends on each specific template. Templates must return objects that have have
either ``attributes`` or ``task`` as a top level key. All other top level keys
will be ignored. See the `existing templates`_ for examples.

Empty Try
:::::::::

If there is no try syntax or ``try_task_config.json``, the ``try_mode``
parameter is None and no tasks are selected to run.  The resulting push will
only have a decision task, but one with an "add jobs" action that can be used
to add the desired jobs to the try push.


Complex Configuration
:::::::::::::::::::::

If you need more control over the build configuration,
(:doc:`staging releases </tools/try/selectors/release>`, for example),
you can directly specify :doc:`parameters <parameters>`
to override from the ``try_task_config.json`` like this:

.. parsed-literal::

   {
       "version": 2,
       "parameters": {
           "optimize_target_tasks": true,
           "release_type": "beta",
           "target_tasks_method": "staging_release_builds"
       }
   }

This format can express a superset of the version 1 format, as the
version one configuration is equivalent to the following version 2
config.

.. parsed-literal::

   {
       "version": 2,
       "parameters": {
           "try_task_config": {...},
           "try_mode": "try_task_config",
       }
   }

.. _JSON-e: https://taskcluster.github.io/json-e/
.. _taskgraph module: https://dxr.mozilla.org/mozilla-central/source/taskcluster/taskgraph/templates
.. _condition statements: https://taskcluster.github.io/json-e/#%60$if%60%20-%20%60then%60%20-%20%60else%60
.. _existing templates: https://dxr.mozilla.org/mozilla-central/source/taskcluster/taskgraph/templates
.. _SCM Level: https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/


