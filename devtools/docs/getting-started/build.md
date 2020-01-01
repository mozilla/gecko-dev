# Build and Run Firefox

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

Go to the new folder that was created and run the bootstrap script.

```bash
cd mozilla-central/
./mach bootstrap
```

The script will ask which version of Firefox you want to build. Choose **1. Firefox for Desktop Artifact Mode**. (This is the fastest option, and is recommended for DevTools contributing since you generally won't need to write Rust or C++.) You can go with the recommended defaults for the rest of the questions. 

*Note: when you are prompted for your name, adding unicode characters can crash the process. The workaround here is to use ascii-friendly characters and later on edit your `~/.hgrc` file manually to use the unicode characters in your name.*

After this script successfully completes, create a file called `mozconfig` in your mozilla-central directory and paste in this line:

```bash
ac_add_options --enable-artifact-builds
```

## Build Firefox

Open a new terminal window and run these commands:

```bash
./mach configure
./mach build
```

If your system needs additional dependencies, the above commands will fail, and error messages will be printed to your screen. Follow their advice and then try running the commands again until they complete successfully. (For example, some MacOS users may need to run `brew link python`.)

If you get stuck, you can consult the [full build docs](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build), search the internet for a specific error message, or [get in touch](https://firefox-dev.tools/) with the DevToolos community via Slack.

## Run Firefox

To run the local Firefox build you just compiled:

```bash
./mach run
```

If successful, your local Firefox will open—it's called Nightly and has a blue globe icon.

Your local Firefox will run using an empty temporary profile which is discarded when you close the browser. We will look into [persistent development profiles later](./development-profiles.md). But first...

⭐️  **Time for some congratulations!** You managed to get Firefox's code, build tools and dependencies, and run your very own copy of Firefox! Well done! ⭐   ️

## Rebuild Firefox

<!--TODO: it would be valuable to explain how to pull changes! -->

Once you complete a first successful build, you can build again by running `./mach build`. 

You can also ask the `mach` script to build only changed files for a faster process:

```bash
./mach build faster
```

Sometimes, if you haven't updated in a while, you'll be told that you need to *clobber*, or basically delete precompiled stuff and start from scratch, because there are too many changes:

```bash
./mach clobber
```

It is a bit tedious to do this manually, but fortunately you can add an entry to `mozconfig` to have this done automatically for you each time it's required. Add this and save the file:

```
# Automatically clobber when an incremental build is not possible
mk_add_options AUTOCLOBBER=1
```
