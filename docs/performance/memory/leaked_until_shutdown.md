# Debugging a "leaked until shutdown" problem using cycle collector logs

Here's an example of how to use cycle collector logs to understand what might be causing a "leaked until shutdown" problem.

## Introduction

First off, what do I mean? There are a number of different leak checkers in Firefox. The one we are talking about has errors that look like this:

    TEST_END: FAIL, expected PASS - leaked 1 window(s) until shutdown [url = chrome://browser/content/browser.xhtml]

Different things besides "window" can appear here, like docshell. The important part is the "until shutdown". What this means is that a window was created during a specific test, but it was not destroyed immediately after we finished the test. However, it was destroyed by the time we shut down the entire browser. This generally means that chrome JS stored a reference to the window in a global variable somewhere, and forgot to clear it before the test went away. In the worst case, this can mean that chrome JS is leaking every web page we open. This kind of leak wouldn't be detected by our other leak checkers because they run at shutdown, and all chrome JS is cleared away before we shut down.

## Initial investigation

Now that you hopefully understand a bit more what this leak means, we can talk about how an investigation should proceed. First, you want to get more information about which test created the leaked window, so you can figure out how to reproduce the leak without running a bunch of tests.

The information appears one line before the line I showed above, like this:

    INFO TEST-INFO | browser/components/sessionstore/test/browser_tab_groups_empty.js | windows(s) leaked: [pid = 6245] [serial = 12], [pid = 6245] [serial = 13]
    TEST_END: FAIL, expected PASS - leaked 1 window(s) until shutdown [url = chrome://browser/content/browser.xhtml]

This means the window was created while running the test `browser_tab_groups_empty.js`. The two windows are uniquely identified by the strings `[pid = 6245] [serial = 12]` and `[pid = 6245] [serial = 13]`. You can search in the test log to see the exact moments these windows were created or destroyed, which can be useful.

For instance, this is the place where the first window was created:

    GECKO(6245) [Parent 6245: Main Thread]: I/DocShellAndDOMWindowLeak ++DOMWINDOW == 12 (198390000) [pid = 6245] [serial = 12] [outer = 0]

The place where the window was destroyed will look similar, except that it will have `--DOMWINDOW` instead of `++DOMWINDOW`.

Before we get into the complexity of analyzing cycle collector logs, this is a good moment to sit back for a few minutes and look over your patch and see if you can figure out where you might have created a strong reference to a window without clearing it when the window is closed. With a small patch, it can be easier to figure it out by reasoning rather than using cycle collector logs.

## Getting a cycle collector log

If that doesn't work, we can take a look at creating and analyzing cycle collector logs.

The first thing you want to do is figure out whether the leak is in a parent process or a child process, so that we can create less useless logs. In the example above, the logging for the `++DOMWINDOW` says `[Parent 6245: Main Thread]` so we can tell it is in the parent process. It could also look like `[Child 6246: Main Thread]`, which would mean that it is in a child process.

The second thing you want to do is make sure you can reproduce the leak when it is run alone. Sometimes there are weird interactions between tests so we want to check that's not the case here. While a debug build is not needed to get cycle collector logs, if the error you are trying to fix is only showing up in a debug build, you'll need to use one, too.

For our example test, you'd run something like this to confirm the error:

    ./mach test --headless browser_tab_groups_empty.js

With that confirmed, we need to re-run the test with a lot of environment variables set, in order to capture some logs. It'll look something like this:

    MOZ_DISABLE_CONTENT_SANDBOX=t MOZ_CC_LOG_DIRECTORY=~/logs/emptylog/ MOZ_CC_LOG_ALL=1 MOZ_CC_LOG_PROCESS=main MOZ_CC_LOG_THREAD=main ./mach test --headless browser_tab_groups_empty.js

Breaking this down:
1. `MOZ_DISABLE_CONTENT_SANDBOX=t` disables the sandbox so that the content process can save logs to the disk. This isn't needed if you are only logging the parent process.
2. `MOZ_CC_LOG_DIRECTORY=~/logs/emptylog/` sets the directory we are saving the logs to. Adjust it as appropriate. The directory must already exist.
3. `MOZ_CC_LOG_ALL=1` logs every cycle collection while the browser is running. We have to do this because we can't tell in advance which CC will capture the state when the window is leaking. This means we will have many logs.
4. `MOZ_CC_LOG_PROCESS=main` only records logs for the main process. This is what you need if you previously determined that the leak was in the parent process. If instead you determined that the leak was in a child process, the right side will be `content` instead of `main`. The purpose of this is to reduce the number of logs you have to sift through, and also to avoid hiding the leak by changing timing (logging can be slow).
5. `MOZ_CC_LOG_THREAD=main` only records logs for the main thread, ignoring CCs on worker threads. All of the things that this leak checker cares about are main thread only, so it'll always be like this. As with the previous one, the goal here is to reduce the number of logs we produce.

There are lots of different ways to run the cycle collector, including on try, but we won't go into them here.

Now run the test to get the logs. In addition to the cycle collector logs, you'll want to keep around a copy of the normal test log.

## Analyzing the log

In the test log for the run where we captured cycle collector logs, we're going to look up the same information we had in the initial leak log. First, we want information about which windows leaked:

    INFO TEST-INFO | browser/components/sessionstore/test/browser_tab_groups_empty.js | windows(s) leaked: [pid = 90804] [serial = 12], [pid = 90804] [serial = 13]

This tells us that the PID of the process that had leaked windows was 90804.

Now look up the information for those leaked windows in the test log:

    GECKO(90804) [Parent 90804: Main Thread]: I/DocShellAndDOMWindowLeak --DOMWINDOW == 11 (135153800) [pid = 90804] [serial = 12] [outer = 0] [url = chrome://browser/content/browser.xhtml]
    GECKO(90804) [Parent 90804: Main Thread]: I/DocShellAndDOMWindowLeak --DOMWINDOW == 10 (14b22fc00) [pid = 90804] [serial = 13] [outer = 0] [url = about:blank]

This tells us that 0x135153800 and 0x14b22fc00 are the addresses of the windows in question. For now, let's focus on the window `0x135153800`. One is an inner and one is an outer, so if we fix one leak we'll probably fix them both.

Now we go to the directory where the CC logs were saved, and see which logs have the windows we're interested in, using the information about the PID and addresses we just gathered:

```
~/logs/emptylog % grep nsGlobalWindow cc-edges.90804* | grep 135153800
cc-edges.90804-1.log:0x135153800 [rc=2] nsGlobalWindowOuter # 30 outer
cc-edges.90804-2.log:0x135153800 [rc=2] nsGlobalWindowOuter # 30 outer
cc-edges.90804-3.log:0x135153800 [rc=2] nsGlobalWindowOuter # 30 outer
cc-edges.90804-4.log:0x135153800 [rc=2] nsGlobalWindowOuter # 30 outer
cc-edges.90804-5.log:0x135153800 [rc=2] nsGlobalWindowOuter # 30 outer
cc-edges.90804-6.log:0x135153800 [rc=2] nsGlobalWindowOuter # 30 outer
cc-edges.90804-7.log:0x135153800 [rc=2] nsGlobalWindowOuter # 30 outer
```

The number in the file name indicates the order these logs were created in. `cc-edges.90804-7.log` is the final log that the windows appeared in, so that is probably the log for the CC where it was freed. We can confirm this by looking for the special `[garbage]` indicator in the log:

    ~/logs/emptylog % grep 0x135153800 cc-edges.90804* | grep garbage
    cc-edges.90804-7.log:0x135153800 [garbage]

Because the window was cleaned up in log 7, we want to investigate log 6, because the window was still alive, but probably should have already been cleaned up. (The leak checking framework runs a bunch of CCs before the point where the window should be cleaned up.) To do this, we need the `find_roots` script [from here](https://github.com/amccreight/heapgraph/). You give this script the name of the log and the address of the object:

```
~/logs/emptylog % ~/tools/heapgraph/find_roots.py cc-edges.90804-6.log 0x14b22fc00
Parsing cc-edges.90804-6.log. Done loading graph.

0x135df8640 [LoadedScript]
    --[mModuleRecord]--> 0x2cf45673da80 [JS Object (Module)]
    --[**UNKNOWN SLOT 1**]--> 0x2cf45673dad8 [JS Object (ModuleEnvironmentObject)]
    --[SessionStoreTestUtils]--> 0x39112798e660 [JS Object (Object)]
    --[windowGlobal]--> 0x2cf45676e660 [JS Object (Proxy)]
    --[baseshape_global, proxy target]--> 0x112cd36e8c90 [JS Object (Window)]
    --[UnwrapDOMObject(obj)]--> 0x14b22fc00 [nsGlobalWindowInner # 31 inner chrome://browser/content/browser.xhtml]

    Root 0x135df8640 is a ref counted object with 1 unknown edge(s).
    known edges:
        0x2cf45676e5f0 [JS Object (ScriptSource)]  --[]--> 0x135df8640
```

What this ASCII art means is that there is a path from a `LoadedScript` object to the window.

`--[mModuleRecord]-->` is an edge in this path, with the label `mModuleRecord`.

In this specific case, the fact that the `SessionStoreTestUtils` object still holds a reference to the window after the test closed seemed suspicious, and the person investigating the leak changed the test to avoid the use of `SessionStoreTestUtils`.
