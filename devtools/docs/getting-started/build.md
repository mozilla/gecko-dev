# Build and Run Firefox Developer Tools

The first big step to working on DevTools is building Firefox's source code.

## Install Mercurial

Firefox's source code is hosted on a [Mercurial repository](https://hg.mozilla.org/mozilla-central/). If you don't already have it, [install Mercurial](https://www.mercurial-scm.org/downloads). Its website provides instructions for the most popular package managers like `brew` and `apt-get`.

## Get the code

The repository is about 5 GB (20 GB after building). It will take about 30 minutes to download, depending on your connection.

```bash
cd ~ # or the folder where you store your projects, for example ~/projects
hg clone https://hg.mozilla.org/mozilla-central
```

## Install dependencies

The following section is for Linux and MacOS users. Window users, [follow these instructions](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Windows_Prerequisites) instead.

Go to the new folder that was created and run Firefox's bootstrap script.

```bash
cd mozilla-central/
./mach bootstrap
```

The script will ask which version of Firefox you want to build. Choose **1. Firefox for Desktop Artifact Mode**. This is the fastest option, and is recommended for DevTools contributing since you generally won't need to write Rust or C++.

You can go with the recommended defaults for the rest of the questions. *Note: when you are prompted for your name, adding unicode characters can crash the process. The workaround here is to use ascii-friendly characters and later on edit your `~/.hgrc` file manually to use the unicode characters in your name.*

[Full build docs](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) 

## Build and Run Firefox

Run this:

```bash
./mach configure
./mach build
```

Please note, if this fails it might be possible you need to run the `bootstrap.py` script first. Download the [bootstrap.py script](https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py) and save it in your project directory. Then run `python bootstrap.py` and follow the prompted steps.

**Note:** if using Windows, you might need to type the commands without the `./`:

```bash
mach bootstrap
mach configure
mach build
```

If your system needs additional dependencies installed (for example, Python, or a compiler, etc) the above commands might fail, and various diagnostic messages will be printed to your screen. Follow their advice and then try running the command that failed again, until the three of them complete successfully.

Some error messages can be quite cryptic. It is a good idea to consult the [documentation](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) specific to the platform you're using. Sometimes searching in the internet for the specific error message you get can help, and you can also [get in touch](https://firefox-dev.tools/#getting-in-touch) if you get stuck.

Once you complete a first successful build, you should be able to build again by running only this command:

```bash
./mach build
```

By the way, building takes a long time (specially on slow computers).

### Running your own compiled version of Firefox

To run the Firefox you just compiled:

```bash
./mach run
```

This will run using an empty temporary profile which is discarded when you close the browser. We will look into [persistent development profiles later](./development-profiles.md). But first...

⭐️  **Time for some congratulations!** You managed to get Firefox's code, build tools and dependencies, and just run your very own copy of Firefox! Well done! ⭐   ️ 

### Rebuilding

<!--TODO: it would be valuable to explain how to pull changes! -->

Suppose you pulled the latest changes from the remote repository (or made some changes, to experiment and see what happens) and want to build again.

You can ask the `mach` script to build only changed files:

```bash
./mach build faster
```

This should be faster (a matter of seconds).

Sometimes, if you haven't updated in a while, you'll be told that you need to *clobber*, or basically delete precompiled stuff and start from scratch, because there are too many changes. The way to do it is:

```bash
./mach clobber
```

It is a bit tedious to do this manually, but fortunately you can add an entry to `mozconfig` to have this done automatically for you each time it's required. Add this and save the file:

```
# Automatically clobber when an incremental build is not possible
mk_add_options AUTOCLOBBER=1
```
