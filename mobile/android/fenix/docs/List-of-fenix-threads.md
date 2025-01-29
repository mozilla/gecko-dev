# List of Fenix Threads

To profile background threads using the Firefox Profiler, you need to specify their names. It uses a case-insensitive substring match, e.g. specifying `default` will match all threads in the kotlin default dispatcher which have a name like, `DefaultDispatcher-worker-*`. This document is a list of the threads in fenix (via `ThreadGroup.list()` as of Jan 2025) to make using this functionality easier:
```
AutoSave-thread-1
BrowserIcons-thread-1
BrowserIcons-thread-2
BrowserIcons-thread-3
BrowserStore-thread-1
Cleaner-0
Cleaner-1
Cleaner-2
Cleaner-3
Cleaner-4
Cleaner-5
Cleaner-6
Cleaner-7
ConnectivityThread
DefaultDispatcher-worker-1
DefaultDispatcher-worker-10
DefaultDispatcher-worker-11
DefaultDispatcher-worker-12
DefaultDispatcher-worker-13
DefaultDispatcher-worker-14
DefaultDispatcher-worker-15
DefaultDispatcher-worker-16
DefaultDispatcher-worker-17
DefaultDispatcher-worker-18
DefaultDispatcher-worker-2
DefaultDispatcher-worker-3
DefaultDispatcher-worker-4
DefaultDispatcher-worker-5
DefaultDispatcher-worker-6
DefaultDispatcher-worker-7
DefaultDispatcher-worker-8
DefaultDispatcher-worker-9
FinalizerDaemon
FinalizerWatchdogDaemon
FxaAccountManager-thread-1
Gecko
GleanAPIPool
HeapTaskDaemon
HistoryMetadataService-thread-1
IPC I/O Parent
JNA Cleaner
Jit thread pool worker thread 0
LeakCanary-Background-iCanHasHeap-Updater
LeakCanary-Heap-Dump
NimbusDbScope-thread-1
PlacesStorageWriteScope-thread-1
Profile Saver
ReferenceQueueDaemon
RenderThread
Signal Catcher
StreamTrans #13
StreamTrans #8
StreamTrans #9
SurfaceSyncGroupTimer
ThumbnailStorage-thread-1
WM.task-1
WM.task-2
WM.task-3
WifiManagerThread
androidx.work-1
arch_disk_io_0
arch_disk_io_1
arch_disk_io_2
arch_disk_io_3
binder:27622_1
binder:27622_2
binder:27622_3
binder:27622_4
binder:27622_5
glean.MetricsPingScheduler
hwuiTask0
hwuiTask1
kotlinx.coroutines.DefaultExecutor
launcher
main
pool-18-thread-1
pool-21-thread-1
pool-27-thread-1
queued-work-looper
```

Note that `arch_disk_io_*` represents the kotlin io dispatcher.
