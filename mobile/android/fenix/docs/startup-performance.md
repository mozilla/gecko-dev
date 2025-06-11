# Startup performance

The following is meant as overview of startup execution performance. It should be noted that this data is at high risk of becoming out-of-date, but this can still serve as an introduction to the cost of the work being done during startup. It is always recommended to familiarize yourself with recently captured performance profiles to get a more accurate understanding.

```{mermaid}
gantt
    title App Initialization Timeline (ms view)
    dateFormat  ss.SSS
    axisFormat  %S.%L

    section FenixApplication
    init                             :a0, 00.000, 0.001s
    onCreate                         :a1, after a0, 0.150s
    Install crash reporter           :a2, 00.010, 0.001s
    Initialize Nimbus (and megazord) :a3, after a2, 0.060s
    GeckoEngine warmUp()             :a4, after a3, 0.010s
    Initialize BrowserStore          :a5, after a4, 0.010s
    Initialize Glean                 :a6, after a5, 0.010s
    Initialize web extension support :a7, after a6, 0.005s
    Finish megazord setup (Rust HTTP) :a8, after a7, 0.001s
    Warm BrowsersCache :a9, after a8, 0.001s
    Global app services providers :a10, after a9, 0.005s
    Setup Push                       :a11, after a10, 0.005s
    Records metrics :a12, after a11, 0.020s

    section IntentReceiverActivity
    onCreate :i1, after a1, 0.035s

    section HomeActivity
    onCreate :h1, after i1, 0.070s
    onStart :h2, after b1, 0.160s

    section BrowserFragment
    onCreateView :b1, after h1, 0.50s
    Tab preview created :b2, after h1, 0.025s
    Toolbar view initialization :b3, after b2, 0.020s
    onStart :b4, after h2, 0.020s

    section Background Threads
    Glean initialized :w, after a2, 0.150s
    Nimbus Database :w0, after a2, 0.060s
    Room db (recently closed) :w1, after w0, 0.070s
    Room db (downloads) :w2, after w1, 0.060s
    Room db (crashes) :w3, after w2, 0.050s

    section Gecko
    Gecko thread runs :g0, after a4, 1s

    section Gpu Process
    Render thread runs :r1, after g0, 1.100s

    section Tab Process
    Some web content work :t1, after g0, 0.100s
```

## Update log:

- 2025-05-06

Diagram initially synthesized from the following profiles:

[Samsung A55 using the newssite applink startup test](https://share.firefox.dev/43RN9pF)

[Samsung A55 cold start deeplink with more thread information](https://share.firefox.dev/3HpoX6u)
