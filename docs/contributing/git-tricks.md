# Git Tricks

This is a list of Git tricks to help you be more productive. Pick the ones you
like and ignore the rest — it's not an official guide.

## Worktrees

[Git worktrees](https://git-scm.com/docs/git-worktree) allow you to have
multiple parallel checkouts of a single `.git` repo. This can be useful if you
are working on more than one thing at a time, or if you need to temporarily
checkout another branch (e.g. for an uplift) but you don't want to rebuild the
world.

## Use a personal fork repository to track your work

:::{note}
Having a personal fork is **not required** to contribute to Firefox. But it can
be useful if you want to share in-progress patches with others or across
machines. There are also other alternatives to this, like sending patches
generated via `git format-patch`.
:::

:::{warning}
Be mindful of not pushing security-sensitive commits to your personal fork.
See [](<Fixing Security Bugs>).
:::

1. Ensure you have a Git checkout already set up.
1. Go to the [Firefox repository](https://github.com/mozilla-firefox/firefox).
1. Log into GitHub and [create a fork](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/fork-a-repo?tool=webui#forking-a-repository) under your username.
1. Go to your checkout directory and run:

```
$ git remote add fork git@github.com:<yourusername>/firefox.git
$ git remote set-url --push origin git@github.com:<yourusername>/firefox.git
```

Remember to `git fetch fork` if you need to fetch from a different machine for
example.

## `git-revise`: Efficiently update, split, and rearrange git commits

[`git-revise`](https://git-revise.readthedocs.io/en/latest/man.html) is an
in-memory rebase tool which allows you to reword, reorder and split patches in
memory, without touching the disk checkout. That means that you can reorder and
split patches really fast, without having to rebuild afterwards.

## `git wip`

If you have a set up like the above, you can use something like this to show
the branches in which you have unmerged work:

```
$ git config alias.wip "log --branches --remotes=fork --not --remotes=origin --simplify-by-decoration --decorate --oneline --graph"
```

That would generate some output like:

```
$ git wip
* 0577f64ec6e2 (HEAD -> git-docs) WIP: Add some more advanced git docs.
* 5c15717818ff (view-transition-scrolling) fixup! Bug 1961140 - Null out ASR for view transition captured contents. r=nical,mstange,botond
* fa8d2785fa3c (fork/kill-html-button-frame, remove-focus-outer) Fix combobox end padding.
* b9af5ad6a613 (fork/canvas-bg-clean-up, canvas-bg-clean-up) Bug 1956128 - Clean up AddCanvasBackgroundColorItem callers. r=mstange
... etc
```

## Git Maintenance

[Git maintenance](https://git-scm.com/docs/git-maintenance) can set up a job to
fetch, potentially clean-up, and much more, from time to time.

To use the default configuration just:

```
$ git maintenance start
```

For more advanced configuration please see the doc linked above.
