# Mozharness
This repository is a downstream read-only copy of:
http://hg.mozilla.org/build/mozharness/

### Submitting changes
We do not support the github Pull Request workflow, since github is only a downstream
mirror for us. However, feel free to fork from us and make changes. Then, rather than
submitting a pull request, please create a patch for your changes (capture the output
of your changes using e.g. git diff) and attach the patch file to a Bugzilla bug,
created in the following component:
https://bugzilla.mozilla.org/enter_bug.cgi?product=Release%20Engineering&component=Mozharness

This bug will get triaged by us.

### Docs
* https://developer.mozilla.org/en-US/docs/Mozharness_FAQ
* https://wiki.mozilla.org/ReleaseEngineering/Mozharness
* http://moz-releng-mozharness.readthedocs.org/en/latest/mozharness.mozilla.html
* http://moz-releng-docs.readthedocs.org/en/latest/software.html#mozharness

### To run mozharness unit tests
```
pip install tox
tox
```

### To run tests in travis
Please note if you fork this repository and wish to run the tests in travis,
you will need to enable your github fork in both travis and coveralls. In both
cases you can log in with your github account, you do not need to set up a new
one. To enable:
* https://travis-ci.org/profile
* https://coveralls.io/repos/new

After enabling, you will need to push changes to your repo in order for a travis
job to be triggered.

### To match commits to upstream hg changesets
Add this following section to the .git/config file in your local clone:
```
[remote "mozilla"]
	url = git@github.com:mozilla/build-mozharness
	fetch = +refs/heads/*:refs/remotes/mozilla/*
	fetch = +refs/notes/*:refs/notes/*
```
then to match a git commit to an upstream hg changeset:
```
git fetch mozilla
git log
```
This will produce output like this:
```
commit c6dc279ab791d7cd11ccc57d2d83a61dc5e0dd09
Author: Simarpreet Singh <s244sing@uwaterloo.ca>
Date:   Mon Dec 22 14:46:56 2014 -0500

    Bug 1078619 - Allow to run talos jobs as a developer. r=armenzg

Notes:
    Upstream source: https://hg.mozilla.org/build/mozharness/rev/7204ff2ff48a6d31dc2fd6aa25465962f93a91ee

commit dce9aae0dadf3875afd44c8e61b70fd5ba91f91f
Author: Ankit Goyal <ankit.goyal90@hotmail.com>
Date:   Mon Dec 22 10:18:16 2014 -0500

    Bug 1113081 - Remove references to metro mode from talos.py script. r=jmaher

Notes:
    Upstream source: https://hg.mozilla.org/build/mozharness/rev/0424b451c005724c08a12bfe64733142305f4476

commit 7bc17c00dafb144b9982dff2e19e8da91229c6c5
Author: Peter Moore <pmoore@mozilla.com>
Date:   Fri Dec 19 20:35:55 2014 +0100

    Bug 1076810 - coveralls publish failures should not cause travis job result to be failure,r=rail

Notes:
    Upstream source: https://hg.mozilla.org/build/mozharness/rev/701d2eda2aece7c63d34e907bcd657f0895d1c4e
```
This allows you to map a git commit SHA to an hg changeset SHA ("Upstream source").


Happy contributing! =)
