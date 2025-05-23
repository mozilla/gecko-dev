Introduction to Jujutsu
#######################

Jujutsu (``jj`` on the command-line) is a modern DVCS, that uses ``git``
repositorie as its storage backend. It borrows extensively from Mercurial,
but has many more features.

.. contents:: Table of Contents

Links and Resources
-------------------

- Hope page https://jj-vcs.github.io/jj/latest/
- In-progress tutorial: https://steveklabnik.github.io/jujutsu-tutorial/
- Good introduction and context: https://v5.chriskrycho.com/essays/jj-init/
- Short introduction:
  https://neugierig.org/software/blog/2024/12/jujutsu.html
- Introduction for Mercurial users:
  https://ahal.ca/blog/2024/jujutsu-mercurial-haven/
- ``:ahal``'s configuration: https://github.com/ahal/dot-files/blob/main/dot-files/jj-config.toml

Quick Start
-----------

To get up and running with a fresh checkout, run the following commands
(line-separated) after installing ``git`` and ``jj`` (jujutsu):

::

   # Create a clone and enter the directory
   jj git clone --colocate https://github.com/mozilla-firefox/firefox
   cd firefox # Alternatively, if you have the Git clone already
   jj git init --colocate

   # Set up jujutsu revset aliases (which will affect e.g. the default `jj log` output)
   jj config edit --repo
   <edit>
   [git]
   fetch = ["origin"]

   [revset-aliases]
   "trunk()" = "main@origin"
   "immutable_heads()" = '''
   builtin_immutable_heads()
   | remote_bookmarks('autoland')
   | remote_bookmarks('beta')
   | remote_bookmarks('esr')
   | remote_bookmarks('release')
   '''
   </edit>

   # Track remote bookmarks (you can do this for the other bookmarks, too)
   jj bookmark track main@origin autoland@origin beta@origin release@origin

   # Move the working copy commit to bookmarks/central
   jj new main

General Tips
------------

Changes and Commits
~~~~~~~~~~~~~~~~~~~

``changes`` and ``commits`` are distinct concepts. While in git there’s
a one-to-one record of ``changes`` *as* ``commits`` (plus, perhaps,
staging), in jujutsu ``commits`` occur fairly frequently (basically any
time there’s a change and you run ``jj``, which results in a snapshot).
In this sense, ``commits`` can be considered literal snapshot/state
commits, whereas ``changes`` are the user-friendly unit-of-work that
developers are doing. ``changes`` have hashes that are alphabetic,
whereas ``commits`` have hashes which are the same as their git
counterparts (sha1, represented as hex characters).

Thus, a particular ``change`` points to one ``commit`` at a time,
however there is a history of ``commits`` recorded for each ``change``
(see ``jj evolog``, for example). You can specify either ``change``
hashes *or* ``commit`` hashes in revsets.

Firefox Main Tips
-----------------

Other Useful revset aliases (place in ``.jj/repo/config.toml``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

   [revset-aliases]
   # Get all changes with "Bug 12345" in the subject (can be improved with https://github.com/jj-vcs/jj/issues/5895)
   'bug_all(x)' = 'mutable() & subject(glob:"Bug *") & subject(x)'
   # Get head change(s) with "Bug 12345" in the subject
   'bug(x)' = 'heads(bug_all(x))'
   # Get root change(s) with "Bug 12345" in the subject
   'bug_root(x)' = 'roots(bug_all(x))'

``moz-phab``
~~~~~~~~~~~~

WIP support for Jujutsu in ``moz-phab`` is being developed at
```erichdongubler-mozilla/review``\ #1 <https://github.com/erichdongubler-mozilla/review/pull/1>`__.
You can install this via:

::

   pip install MozPhab@git+https://github.com/erichdongubler-mozilla/review@refs/pull/1/head

If you need to fall back to using Git with vanilla ``moz-phab``, most
operations require you to not be in a detached ``HEAD`` state. However,
Jujutsu frequently leaves it in one. One simple solution is to wrap the
``moz-phab`` command with a script like:

::

   #!/bin/sh
   git checkout -B moz-phab && moz-phab "$@"

You could instead make this a shell alias/function, if preferred.

``mach try``
~~~~~~~~~~~~

``./mach try`` requires a clean working directory to push. When editing
a change in Jujutsu, the changes will be moved to the index in Git.
Therefore in order to push to try, you must start a new empty change on
top of the change you want to push. E.g:

::

   $ jj new
   $ ./mach try ...
   $ jj prev --edit

The following alias automates this so you can use ``jj try-push <args>``
instead of ``./mach try <args>`` and it will create/remove a temporary
empty change:

::

   [aliases]
   try-push = ["util", "exec", "--", "bash", "-c", """
   #!/usr/bin/env bash
   set -euo pipefail
   jj new --quiet
   ./mach try $@ || true
   jj prev --edit --quiet
   """, ""]

See also `Bug 1929372 - [mozversioncontrol] Add unofficial support for
Jujutsu
repositories <https://bugzilla.mozilla.org/show_bug.cgi?id=1929372>`__

``mach lint``
~~~~~~~~~~~~~

| ``./mach lint`` can be integrated with ``jj fix``. Follow the
  instructions here:
| https://firefox-source-docs.mozilla.org/code-quality/lint/usage.html#jujutsu-integration

(adding the config to ``jj config edit --repo``)

The benefit of running ``jj fix`` over ``./mach lint --fix`` directly,
is that it will step through all your mutable commits and checkout each
file at that revision before running the fixers on it. So you’re
guaranteed to get the fix directly in the commit that introduced the
issue.

Rebasing work in progress (and automatically drop changes that have landed)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You want something like:

::

   jj git fetch && jj rebase --skip-emptied -r 'mutable() & mine()' -d main

This will:

1. Pull from the main repo
2. Rebase any mutable changesets you’ve made onto the (updated, tracked
   bookmark) ``main`` changeset, and drop any that become empty (because
   they have landed)

Of course you could narrow the scope of what you want to rebase by
altering the ``-r`` argument and providing specific revisions, or rebase
onto autoland or beta or other bookmarks if you want.

Dropping/pruning/removing obsolete commits
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

(Note: you may want to look at the `previous
tip <#rebasing-work-in-progress-(and-automatically-drop-changes-that-have-landed)>`__!)

You can use any of:

::

   jj abandon x
   jj abandon x y
   jj abandon x..z
   jj abandon x::y

To abandon individual revision ``x``, both individual revisions ``x``
and ``y``, or the range of commits from ``x`` to ``z``, respectively.

When you’re dealing with temporary changes that you have not committed
(“working directory changes”) this is also an easy way to revert those
(a la ``hg revert --no-backup –all``).

Watchman integration
~~~~~~~~~~~~~~~~~~~~

Tired of the frequent Snapshotting… message? Edit your global ``jj``
configuration by doing:

::

   jj config edit --user

and add the following:

::

   [core]
   fsmonitor = "watchman"

Instead of scanning the file system, ``jj`` will (much like ``hg``\ ’s
``fsmonitor`` extension) use file system events to be notified about
file changes, resulting in much shorter operation time, without having
to disable the snapshotting mechanism.
